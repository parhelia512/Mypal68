/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerParent.h"

#include "nsProfiler.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/IOInterposer.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfileBufferControlledChunkManager.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace ipc;

class ProfilerParentTracker;

// This class is responsible for gathering updates from chunk managers in
// different process, and request for the oldest chunks to be destroyed whenever
// the given memory limit is reached.
class ProfileBufferGlobalController final {
 public:
  ProfileBufferGlobalController(ProfilerParentTracker& aTracker,
                                size_t aMaximumBytes);

  ~ProfileBufferGlobalController();

  void Clear();

  void HandleChunkManagerUpdate(
      base::ProcessId aProcessId,
      ProfileBufferControlledChunkManager::Update&& aUpdate);

 private:
  ProfilerParentTracker& mTracker;
  const size_t mMaximumBytes;

  const base::ProcessId mParentProcessId = base::GetCurrentProcId();

  // Set to null when we receive the final empty update.
  ProfileBufferControlledChunkManager* mParentChunkManager =
      profiler_get_controlled_chunk_manager();

  size_t mUnreleasedTotalBytes = 0;

  struct PidAndBytes {
    base::ProcessId mProcessId;
    size_t mBytes;

    // For searching and sorting.
    bool operator==(base::ProcessId aSearchedProcessId) const {
      return mProcessId == aSearchedProcessId;
    }
    bool operator==(const PidAndBytes& aOther) const {
      return mProcessId == aOther.mProcessId;
    }
    bool operator<(base::ProcessId aSearchedProcessId) const {
      return mProcessId < aSearchedProcessId;
    }
    bool operator<(const PidAndBytes& aOther) const {
      return mProcessId < aOther.mProcessId;
    }
  };
  using PidAndBytesArray = nsTArray<PidAndBytes>;
  PidAndBytesArray mUnreleasedBytesByPid;

  size_t mReleasedTotalBytes = 0;

  struct TimeStampAndBytesAndPid {
    TimeStamp mTimeStamp;
    size_t mBytes;
    base::ProcessId mProcessId;

    // For searching and sorting.
    bool operator==(const TimeStampAndBytesAndPid& aOther) const {
      // Sort first by timestamps, and then by pid in rare cases with the same
      // timestamps.
      return mTimeStamp == aOther.mTimeStamp && mProcessId == aOther.mProcessId;
    }
    bool operator<(const TimeStampAndBytesAndPid& aOther) const {
      // Sort first by timestamps, and then by pid in rare cases with the same
      // timestamps.
      return mTimeStamp < aOther.mTimeStamp ||
             (MOZ_UNLIKELY(mTimeStamp == aOther.mTimeStamp) &&
              mProcessId < aOther.mProcessId);
    }
  };
  using TimeStampAndBytesAndPidArray = nsTArray<TimeStampAndBytesAndPid>;
  TimeStampAndBytesAndPidArray mReleasedChunksByTime;
};

// This singleton class tracks live ProfilerParent's (meaning there's a current
// connection with a child process).
// It also knows when the local profiler is running.
// And when both the profiler is running and at least one child is present, it
// creates a ProfileBufferGlobalController and forwards chunk updates to it.
class ProfilerParentTracker final {
 public:
  static void StartTracking(ProfilerParent* aParent);
  static void StopTracking(ProfilerParent* aParent);

  static void ProfilerStarted(uint32_t aEntries);
  static void ProfilerWillStopIfStarted();

  template <typename FuncType>
  static void Enumerate(FuncType&& aIterFunc);

  template <typename FuncType>
  static void ForChild(base::ProcessId aChildPid, FuncType&& aIterFunc);

  static void ForwardChunkManagerUpdate(
      base::ProcessId aProcessId,
      ProfileBufferControlledChunkManager::Update&& aUpdate);

  ProfilerParentTracker();
  ~ProfilerParentTracker();

 private:
  static void EnsureInstance();

  // List of parents for currently-connected child processes.
  nsTArray<ProfilerParent*> mProfilerParents;

  // If non-0, the parent profiler is running, with this limit (in number of
  // entries.) This is needed here, because the parent profiler may start
  // running before child processes are known (e.g., startup profiling).
  uint32_t mEntries = 0;

  // When the profiler is running and there is at least one parent-child
  // connection, this is the controller that should receive chunk updates.
  Maybe<ProfileBufferGlobalController> mMaybeController;

  // Singleton instance, created when first needed, destroyed at Firefox
  // shutdown.
  static UniquePtr<ProfilerParentTracker> sInstance;
};

ProfileBufferGlobalController::ProfileBufferGlobalController(
    ProfilerParentTracker& aTracker, size_t aMaximumBytes)
    : mTracker(aTracker), mMaximumBytes(aMaximumBytes) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  // This is the local chunk manager for this parent process, so updates can be
  // handled here.
  if (NS_WARN_IF(!mParentChunkManager)) {
    return;
  }
  mParentChunkManager->SetUpdateCallback(
      [parentProcessId = mParentProcessId](
          ProfileBufferControlledChunkManager::Update&& aUpdate) {
        MOZ_ASSERT(!aUpdate.IsNotUpdate(),
                   "Update callback should never be given a non-update");
        // Always dispatch the update, to reduce the time spent in the callback,
        // and to avoid potential re-entrancy issues.
        // Handled by the ProfilerParentTracker singleton, because the
        // Controller could be destroyed in the meantime.
        Unused << NS_DispatchToMainThread(NS_NewRunnableFunction(
            "ChunkManagerUpdate parent callback",
            [parentProcessId, update = std::move(aUpdate)]() mutable {
              ProfilerParentTracker::ForwardChunkManagerUpdate(
                  parentProcessId, std::move(update));
            }));
      });
}

ProfileBufferGlobalController ::~ProfileBufferGlobalController() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (mParentChunkManager) {
    // We haven't handled a final update, so the chunk manager is still valid.
    // Reset the callback in the chunk manager, this will immediately invoke the
    // callback with the final empty update.
    mParentChunkManager->SetUpdateCallback({});
  }
}

void ProfileBufferGlobalController::HandleChunkManagerUpdate(
    base::ProcessId aProcessId,
    ProfileBufferControlledChunkManager::Update&& aUpdate) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (!mParentChunkManager) {
    return;
  }

  MOZ_ASSERT(!aUpdate.IsNotUpdate(),
             "HandleChunkManagerUpdate should not be given a non-update");

  if (aUpdate.IsFinal()) {
    if (aProcessId == mParentProcessId) {
      // This was the final update in the parent process, we cannot keep the
      // chunk manager, and there's no point handling updates anymore.
      // Do some cleanup now, to free resources before we're destroyed.
      mParentChunkManager = nullptr;
      mUnreleasedTotalBytes = 0;
      mUnreleasedBytesByPid.Clear();
      mReleasedTotalBytes = 0;
      mReleasedChunksByTime.Clear();
      return;
    }

    // Final update in a child process, remove all traces of it.
    size_t index = mUnreleasedBytesByPid.BinaryIndexOf(aProcessId);
    if (index != PidAndBytesArray::NoIndex) {
      // We already have a value for this pid.
      PidAndBytes& pidAndBytes = mUnreleasedBytesByPid[index];
      mUnreleasedTotalBytes -= pidAndBytes.mBytes;
      mUnreleasedBytesByPid.RemoveElementAt(index);
    }

    size_t released = 0;
    mReleasedChunksByTime.RemoveElementsBy(
        [&released, aProcessId](const auto& chunk) {
          const bool match = chunk.mProcessId == aProcessId;
          if (match) {
            released += chunk.mBytes;
          }
          return match;
        });
    if (released != 0) {
      mReleasedTotalBytes -= released;
    }

    // Total can only have gone down, so there's no need to check the limit.
    return;
  }

  // Non-final update in parent or child process.

  size_t index = mUnreleasedBytesByPid.BinaryIndexOf(aProcessId);
  if (index != PidAndBytesArray::NoIndex) {
    // We already have a value for this pid.
    PidAndBytes& pidAndBytes = mUnreleasedBytesByPid[index];
    mUnreleasedTotalBytes =
        mUnreleasedTotalBytes - pidAndBytes.mBytes + aUpdate.UnreleasedBytes();
    pidAndBytes.mBytes = aUpdate.UnreleasedBytes();
  } else {
    // New pid.
    mUnreleasedBytesByPid.InsertElementSorted(
        PidAndBytes{aProcessId, aUpdate.UnreleasedBytes()});
    mUnreleasedTotalBytes += aUpdate.UnreleasedBytes();
  }

  size_t destroyedReleased = 0;
  if (!aUpdate.OldestDoneTimeStamp().IsNull()) {
    size_t i = 0;
    for (; i < mReleasedChunksByTime.Length(); ++i) {
      if (mReleasedChunksByTime[i].mTimeStamp >=
          aUpdate.OldestDoneTimeStamp()) {
        break;
      }
    }
    // Here, i is the index of the first item that's at or after
    // aUpdate.mOldestDoneTimeStamp, so chunks from aProcessId before that have
    // been destroyed.
    while (i != 0) {
      --i;
      const TimeStampAndBytesAndPid& item = mReleasedChunksByTime[i];
      if (item.mProcessId == aProcessId) {
        destroyedReleased += item.mBytes;
        mReleasedChunksByTime.RemoveElementAt(i);
      }
    }
  }

  size_t newlyReleased = 0;
  for (const ProfileBufferControlledChunkManager::ChunkMetadata& chunk :
       aUpdate.NewlyReleasedChunksRef()) {
    newlyReleased += chunk.mBufferBytes;
    mReleasedChunksByTime.InsertElementSorted(TimeStampAndBytesAndPid{
        chunk.mDoneTimeStamp, chunk.mBufferBytes, aProcessId});
  }

  mReleasedTotalBytes = mReleasedTotalBytes - destroyedReleased + newlyReleased;

#ifdef DEBUG
  size_t totalReleased = 0;
  for (const TimeStampAndBytesAndPid& item : mReleasedChunksByTime) {
    totalReleased += item.mBytes;
  }
  MOZ_ASSERT(mReleasedTotalBytes == totalReleased);
#endif  // DEBUG

  std::vector<ProfileBufferControlledChunkManager::ChunkMetadata> toDestroy;
  while (mUnreleasedTotalBytes + mReleasedTotalBytes > mMaximumBytes &&
         !mReleasedChunksByTime.IsEmpty()) {
    // We have reached the global memory limit, and there *are* released chunks
    // that can be destroyed. Start with the first one, which is the oldest.
    const TimeStampAndBytesAndPid& oldest = mReleasedChunksByTime[0];
    mReleasedTotalBytes -= oldest.mBytes;
    if (oldest.mProcessId == mParentProcessId) {
      mParentChunkManager->DestroyChunksAtOrBefore(oldest.mTimeStamp);
    } else {
      ProfilerParentTracker::ForChild(
          oldest.mProcessId,
          [timestamp = oldest.mTimeStamp](ProfilerParent* profilerParent) {
            Unused << profilerParent->SendDestroyReleasedChunksAtOrBefore(
                timestamp);
          });
    }
    mReleasedChunksByTime.RemoveElementAt(0);
  }
}

UniquePtr<ProfilerParentTracker> ProfilerParentTracker::sInstance;

/* static */
void ProfilerParentTracker::EnsureInstance() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (sInstance) {
    return;
  }

  sInstance = MakeUnique<ProfilerParentTracker>();
  ClearOnShutdown(&sInstance);
}

/* static */
void ProfilerParentTracker::StartTracking(ProfilerParent* aProfilerParent) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  EnsureInstance();

  if (sInstance->mMaybeController.isNothing() && sInstance->mEntries != 0) {
    // There is no controller yet, but the profiler has started.
    // Since we're adding a ProfilerParent, it's a good time to start
    // controlling the global memory usage of the profiler.
    // (And this helps delay the Controller startup, because the parent profiler
    // can start *very* early in the process, when some resources like threads
    // are not ready yet.)
    sInstance->mMaybeController.emplace(*sInstance,
                                        size_t(sInstance->mEntries) * 8u);
  }

  sInstance->mProfilerParents.AppendElement(aProfilerParent);
}

/* static */
void ProfilerParentTracker::StopTracking(ProfilerParent* aParent) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  sInstance->mProfilerParents.RemoveElement(aParent);
}

/* static */
void ProfilerParentTracker::ProfilerStarted(uint32_t aEntries) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  EnsureInstance();

  sInstance->mEntries = aEntries;

  if (sInstance->mMaybeController.isNothing() &&
      !sInstance->mProfilerParents.IsEmpty()) {
    // We are already tracking child processes, so it's a good time to start
    // controlling the global memory usage of the profiler.
    sInstance->mMaybeController.emplace(*sInstance,
                                        size_t(sInstance->mEntries) * 8u);
  }
}

/* static */
void ProfilerParentTracker::ProfilerWillStopIfStarted() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  sInstance->mEntries = 0;
  sInstance->mMaybeController = Nothing{};
}

template <typename FuncType>
/* static */
void ProfilerParentTracker::Enumerate(FuncType&& aIterFunc) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  for (ProfilerParent* profilerParent : sInstance->mProfilerParents) {
    if (!profilerParent->mDestroyed) {
      aIterFunc(profilerParent);
    }
  }
}

template <typename FuncType>
/* static */
void ProfilerParentTracker::ForChild(base::ProcessId aChildPid,
                                     FuncType&& aIterFunc) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    return;
  }

  for (ProfilerParent* profilerParent : sInstance->mProfilerParents) {
    if (profilerParent->mChildPid == aChildPid) {
      if (!profilerParent->mDestroyed) {
        std::forward<FuncType>(aIterFunc)(profilerParent);
      }
      return;
    }
  }
}

/* static */
void ProfilerParentTracker::ForwardChunkManagerUpdate(
    base::ProcessId aProcessId,
    ProfileBufferControlledChunkManager::Update&& aUpdate) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  if (!sInstance || sInstance->mMaybeController.isNothing()) {
    return;
  }

  MOZ_ASSERT(!aUpdate.IsNotUpdate(),
             "No process should ever send a non-update");
  sInstance->mMaybeController->HandleChunkManagerUpdate(aProcessId,
                                                        std::move(aUpdate));
}

ProfilerParentTracker::ProfilerParentTracker() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(ProfilerParentTracker);
}

ProfilerParentTracker::~ProfilerParentTracker() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(ProfilerParentTracker);

  nsTArray<ProfilerParent*> parents;
  parents = mProfilerParents;
  // Close the channels of any profiler parents that haven't been destroyed.
  for (ProfilerParent* profilerParent : parents) {
    if (!profilerParent->mDestroyed) {
      // Keep the object alive until the call to Close() has completed.
      // Close() will trigger a call to DeallocPProfilerParent.
      RefPtr<ProfilerParent> actor = profilerParent;
      actor->Close();
    }
  }
}

/* static */
Endpoint<PProfilerChild> ProfilerParent::CreateForProcess(
    base::ProcessId aOtherPid) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  Endpoint<PProfilerParent> parent;
  Endpoint<PProfilerChild> child;
  nsresult rv = PProfiler::CreateEndpoints(base::GetCurrentProcId(), aOtherPid,
                                           &parent, &child);

  if (NS_FAILED(rv)) {
    MOZ_CRASH("Failed to create top level actor for PProfiler!");
  }

  RefPtr<ProfilerParent> actor = new ProfilerParent(aOtherPid);
  if (!parent.Bind(actor)) {
    MOZ_CRASH("Failed to bind parent actor for PProfiler!");
  }

  // mSelfRef will be cleared in DeallocPProfilerParent.
  actor->mSelfRef = actor;
  actor->Init();

  return child;
}

ProfilerParent::ProfilerParent(base::ProcessId aChildPid)
    : mChildPid(aChildPid), mDestroyed(false) {
  MOZ_COUNT_CTOR(ProfilerParent);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
}

void ProfilerParent::Init() {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  ProfilerParentTracker::StartTracking(this);

  // We propagated the profiler state from the parent process to the child
  // process through MOZ_PROFILER_STARTUP* environment variables.
  // However, the profiler state might have changed in this process since then,
  // and now that an active communication channel has been established with the
  // child process, it's a good time to sync up the two profilers again.

  int entries = 0;
  Maybe<double> duration = Nothing();
  double interval = 0;
  mozilla::Vector<const char*> filters;
  uint32_t features;
  profiler_get_start_params(&entries, &duration, &interval, &features,
                            &filters);

  if (entries != 0) {
    ProfilerInitParams ipcParams;
    ipcParams.enabled() = true;
    ipcParams.entries() = entries;
    ipcParams.duration() = duration;
    ipcParams.interval() = interval;
    ipcParams.features() = features;

    for (uint32_t i = 0; i < filters.length(); ++i) {
      ipcParams.filters().AppendElement(filters[i]);
    }

    Unused << SendEnsureStarted(ipcParams);
    RequestChunkManagerUpdate();
  } else {
    Unused << SendStop();
  }
}

ProfilerParent::~ProfilerParent() {
  MOZ_COUNT_DTOR(ProfilerParent);

  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  ProfilerParentTracker::StopTracking(this);
}

/* static */
nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>>
ProfilerParent::GatherProfiles() {
  if (!NS_IsMainThread()) {
    return nsTArray<RefPtr<ProfilerParent::SingleProcessProfilePromise>>();
  }

  nsTArray<RefPtr<SingleProcessProfilePromise>> results;
  ProfilerParentTracker::Enumerate([&](ProfilerParent* profilerParent) {
    results.AppendElement(profilerParent->SendGatherProfile());
  });
  return results;
}

// Magic value for ProfileBufferChunkManagerUpdate::unreleasedBytes meaning
// that this is a final update from a child.
constexpr static uint64_t scUpdateUnreleasedBytesFINAL = uint64_t(-1);

/* static */
ProfileBufferChunkManagerUpdate ProfilerParent::MakeFinalUpdate() {
  return ProfileBufferChunkManagerUpdate{
      uint64_t(scUpdateUnreleasedBytesFINAL), 0, TimeStamp{},
      nsTArray<ProfileBufferChunkMetadata>{}};
}

void ProfilerParent::RequestChunkManagerUpdate() {
  if (mDestroyed) {
    return;
  }

  RefPtr<AwaitNextChunkManagerUpdatePromise> updatePromise =
      SendAwaitNextChunkManagerUpdate();
  updatePromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr<ProfilerParent>(this)](
          const ProfileBufferChunkManagerUpdate& aUpdate) {
        if (aUpdate.unreleasedBytes() == scUpdateUnreleasedBytesFINAL) {
          // Special value meaning it's the final update from that child.
          ProfilerParentTracker::ForwardChunkManagerUpdate(
              self->mChildPid,
              ProfileBufferControlledChunkManager::Update(nullptr));
        } else {
          // Not the final update, translate it.
          std::vector<ProfileBufferControlledChunkManager::ChunkMetadata>
              chunks;
          if (!aUpdate.newlyReleasedChunks().IsEmpty()) {
            chunks.reserve(aUpdate.newlyReleasedChunks().Length());
            for (const ProfileBufferChunkMetadata& chunk :
                 aUpdate.newlyReleasedChunks()) {
              chunks.emplace_back(chunk.doneTimeStamp(), chunk.bufferBytes());
            }
          }
          // Let the tracker handle it.
          ProfilerParentTracker::ForwardChunkManagerUpdate(
              self->mChildPid,
              ProfileBufferControlledChunkManager::Update(
                  aUpdate.unreleasedBytes(), aUpdate.releasedBytes(),
                  aUpdate.oldestDoneTimeStamp(), std::move(chunks)));
          // This was not a final update, so start a new request.
          self->RequestChunkManagerUpdate();
        }
      },
      [self = RefPtr<ProfilerParent>(this)](
          mozilla::ipc::ResponseRejectReason aReason) {
        // Rejection could be for a number of reasons, assume the child will
        // not respond anymore, so we pretend we received a final update.
        ProfilerParentTracker::ForwardChunkManagerUpdate(
            self->mChildPid,
            ProfileBufferControlledChunkManager::Update(nullptr));
      });
}

/* static */
void ProfilerParent::ProfilerStarted(nsIProfilerStartParams* aParams) {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerInitParams ipcParams;
  double duration;
  ipcParams.enabled() = true;
  aParams->GetEntries(&ipcParams.entries());
  aParams->GetDuration(&duration);
  if (duration > 0.0) {
    ipcParams.duration() = Some(duration);
  } else {
    ipcParams.duration() = Nothing();
  }
  aParams->GetInterval(&ipcParams.interval());
  aParams->GetFeatures(&ipcParams.features());
  ipcParams.filters() = aParams->GetFilters();

  ProfilerParentTracker::ProfilerStarted(ipcParams.entries());
  ProfilerParentTracker::Enumerate([&](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendStart(ipcParams);
    profilerParent->RequestChunkManagerUpdate();
  });
}

/* static */
void ProfilerParent::ProfilerWillStopIfStarted() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::ProfilerWillStopIfStarted();
}

/* static */
void ProfilerParent::ProfilerStopped() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendStop();
  });
}

/* static */
void ProfilerParent::ProfilerPaused() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendPause();
  });
}

/* static */
void ProfilerParent::ProfilerResumed() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendResume();
  });
}

/* static */
void ProfilerParent::ClearAllPages() {
  if (!NS_IsMainThread()) {
    return;
  }

  ProfilerParentTracker::Enumerate([](ProfilerParent* profilerParent) {
    Unused << profilerParent->SendClearAllPages();
  });
}

void ProfilerParent::ActorDestroy(ActorDestroyReason aActorDestroyReason) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());
  mDestroyed = true;
}

void ProfilerParent::ActorDealloc() { mSelfRef = nullptr; }

}  // namespace mozilla
