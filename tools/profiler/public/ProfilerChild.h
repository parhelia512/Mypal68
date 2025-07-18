/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerChild_h
#define ProfilerChild_h

#include "mozilla/PProfilerChild.h"
#include "mozilla/ProfileBufferControlledChunkManager.h"
#include "mozilla/RefPtr.h"

class nsIThread;

namespace mozilla {

// The ProfilerChild actor is created in all processes except for the main
// process. The corresponding ProfilerParent actor is created in the main
// process, and it will notify us about profiler state changes and request
// profiles from us.
class ProfilerChild final : public PProfilerChild,
                            public mozilla::ipc::IShmemAllocator {
  NS_INLINE_DECL_REFCOUNTING(ProfilerChild)

  ProfilerChild();

  // Collects and returns a profile.
  // This method can be used to grab a profile just before PProfiler is torn
  // down. The collected profile should then be sent through a different
  // message channel that is guaranteed to stay open long enough.
  nsCString GrabShutdownProfile();

  void Destroy();

 private:
  virtual ~ProfilerChild();

  mozilla::ipc::IPCResult RecvStart(const ProfilerInitParams& params) override;
  mozilla::ipc::IPCResult RecvEnsureStarted(
      const ProfilerInitParams& params) override;
  mozilla::ipc::IPCResult RecvStop() override;
  mozilla::ipc::IPCResult RecvPause() override;
  mozilla::ipc::IPCResult RecvResume() override;
  mozilla::ipc::IPCResult RecvAwaitNextChunkManagerUpdate(
      AwaitNextChunkManagerUpdateResolver&& aResolve) override;
  mozilla::ipc::IPCResult RecvDestroyReleasedChunksAtOrBefore(
      const TimeStamp& aTimeStamp) override;
  mozilla::ipc::IPCResult RecvGatherProfile(
      GatherProfileResolver&& aResolve) override;
  mozilla::ipc::IPCResult RecvClearAllPages() override;

  void ActorDestroy(ActorDestroyReason aActorDestroyReason) override;

  FORWARD_SHMEM_ALLOCATOR_TO(PProfilerChild)

  void SetupChunkManager();
  void ResetChunkManager();
  void ResolveChunkUpdate(
      PProfilerChild::AwaitNextChunkManagerUpdateResolver& aResolve);
  void ChunkManagerUpdateCallback(
      ProfileBufferControlledChunkManager::Update&& aUpdate);

  nsCOMPtr<nsIThread> mThread;
  bool mDestroyed;

  ProfileBufferControlledChunkManager* mChunkManager = nullptr;
  AwaitNextChunkManagerUpdateResolver mAwaitNextChunkManagerUpdateResolver;
  ProfileBufferControlledChunkManager::Update mChunkManagerUpdate;
};

}  // namespace mozilla

#endif  // ProfilerChild_h
