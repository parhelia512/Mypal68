/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/ContentCompositorBridgeParent.h"

#include <stdint.h>  // for uint64_t

#include "LayerTransactionParent.h"   // for LayerTransactionParent
#include "apz/src/APZCTreeManager.h"  // for APZCTreeManager
#include "base/message_loop.h"        // for MessageLoop
#include "base/task.h"                // for CancelableTask, etc
#include "base/thread.h"              // for Thread
#include "gfxUtils.h"
#ifdef XP_WIN
#  include "mozilla/gfx/DeviceManagerDx.h"  // for DeviceManagerDx
#endif
#include "mozilla/ipc/Transport.h"           // for Transport
#include "mozilla/layers/AnimationHelper.h"  // for CompositorAnimationStorage
#include "mozilla/layers/APZCTreeManagerParent.h"  // for APZCTreeManagerParent
#include "mozilla/layers/APZUpdater.h"             // for APZUpdater
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/PLayerTransactionParent.h"
#include "mozilla/layers/RemoteContentController.h"
#ifdef MOZ_BUILD_WEBRENDER
#  include "mozilla/layers/WebRenderBridgeParent.h"
#  include "mozilla/layers/AsyncImagePipelineManager.h"
#endif
#include "mozilla/mozalloc.h"  // for operator new, etc
#include "nsDebug.h"           // for NS_ASSERTION, etc
#include "nsTArray.h"          // for nsTArray
#include "nsXULAppAPI.h"       // for XRE_GetIOMessageLoop
#include "mozilla/Unused.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#ifdef MOZ_GECKO_PROFILER
#  include "ProfilerMarkerPayload.h"
#endif

namespace mozilla {

namespace layers {

// defined in CompositorBridgeParent.cpp
typedef std::map<LayersId, CompositorBridgeParent::LayerTreeState> LayerTreeMap;
extern LayerTreeMap sIndirectLayerTrees;
extern StaticAutoPtr<mozilla::Monitor2> sIndirectLayerTreesLock;
void UpdateIndirectTree(LayersId aId, Layer* aRoot,
                        const TargetConfig& aTargetConfig);
void EraseLayerState(LayersId aId);

mozilla::ipc::IPCResult
ContentCompositorBridgeParent::RecvRequestNotifyAfterRemotePaint() {
  mNotifyAfterRemotePaint = true;
  return IPC_OK();
}

void ContentCompositorBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  mCanSend = false;

  // We must keep this object alive untill the code handling message
  // reception is finished on this thread.
  MessageLoop::current()->PostTask(NewRunnableMethod(
      "layers::ContentCompositorBridgeParent::DeferredDestroy", this,
      &ContentCompositorBridgeParent::DeferredDestroy));
}

PLayerTransactionParent*
ContentCompositorBridgeParent::AllocPLayerTransactionParent(
    const nsTArray<LayersBackend>&, const LayersId& aId) {
  MOZ_ASSERT(aId.IsValid());

  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aId, OtherPid())) {
    NS_ERROR(
        "Unexpected layers id in AllocPLayerTransactionParent; dropping "
        "message...");
    return nullptr;
  }

  Monitor2AutoLock lock(*sIndirectLayerTreesLock);

  CompositorBridgeParent::LayerTreeState* state = nullptr;
  LayerTreeMap::iterator itr = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() != itr) {
    state = &itr->second;
  }

  if (state && state->mLayerManager) {
    state->mContentCompositorBridgeParent = this;
    HostLayerManager* lm = state->mLayerManager;
    CompositorAnimationStorage* animStorage =
        state->mParent ? state->mParent->GetAnimationStorage() : nullptr;
    TimeDuration vsyncRate =
        state->mParent ? state->mParent->GetVsyncInterval() : TimeDuration();
    LayerTransactionParent* p =
        new LayerTransactionParent(lm, this, animStorage, aId, vsyncRate);
    p->AddIPDLReference();
    sIndirectLayerTrees[aId].mLayerTree = p;
    return p;
  }

  NS_WARNING("Created child without a matching parent?");
  LayerTransactionParent* p = new LayerTransactionParent(
      /* aManager */ nullptr, this, /* aAnimStorage */ nullptr, aId,
      TimeDuration());
  p->AddIPDLReference();
  return p;
}

bool ContentCompositorBridgeParent::DeallocPLayerTransactionParent(
    PLayerTransactionParent* aLayers) {
  LayerTransactionParent* slp = static_cast<LayerTransactionParent*>(aLayers);
  EraseLayerState(slp->GetId());
  static_cast<LayerTransactionParent*>(aLayers)->ReleaseIPDLReference();
  return true;
}

PAPZCTreeManagerParent*
ContentCompositorBridgeParent::AllocPAPZCTreeManagerParent(
    const LayersId& aLayersId) {
  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, OtherPid())) {
    NS_ERROR(
        "Unexpected layers id in AllocPAPZCTreeManagerParent; dropping "
        "message...");
    return nullptr;
  }

  Monitor2AutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state =
      sIndirectLayerTrees[aLayersId];

  // If the widget has shutdown its compositor, we may not have had a chance yet
  // to unmap our layers id, and we could get here without a parent compositor.
  // In this case return an empty APZCTM.
  if (!state.mParent) {
    // Note: we immediately call ClearTree since otherwise the APZCTM will
    // retain a reference to itself, through the checkerboard observer.
    LayersId dummyId{0};
    RefPtr<APZCTreeManager> temp = new APZCTreeManager(dummyId);
    RefPtr<APZUpdater> tempUpdater = new APZUpdater(temp
#ifdef MOZ_BUILD_WEBRENDER
                                                    ,
                                                    false
#endif
    );
    tempUpdater->ClearTree(
#ifdef MOZ_BUILD_WEBRENDER
        dummyId
#endif
    );
    return new APZCTreeManagerParent(aLayersId, temp, tempUpdater);
  }

  state.mParent->AllocateAPZCTreeManagerParent(lock, aLayersId, state);
  return state.mApzcTreeManagerParent;
}
bool ContentCompositorBridgeParent::DeallocPAPZCTreeManagerParent(
    PAPZCTreeManagerParent* aActor) {
  APZCTreeManagerParent* parent = static_cast<APZCTreeManagerParent*>(aActor);

  Monitor2AutoLock lock(*sIndirectLayerTreesLock);
  auto iter = sIndirectLayerTrees.find(parent->GetLayersId());
  if (iter != sIndirectLayerTrees.end()) {
    CompositorBridgeParent::LayerTreeState& state = iter->second;
    MOZ_ASSERT(state.mApzcTreeManagerParent == parent);
    state.mApzcTreeManagerParent = nullptr;
  }

  delete parent;

  return true;
}

PAPZParent* ContentCompositorBridgeParent::AllocPAPZParent(
    const LayersId& aLayersId) {
  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(aLayersId, OtherPid())) {
    NS_ERROR("Unexpected layers id in AllocPAPZParent; dropping message...");
    return nullptr;
  }

  RemoteContentController* controller = new RemoteContentController();

  // Increment the controller's refcount before we return it. This will keep the
  // controller alive until it is released by IPDL in DeallocPAPZParent.
  controller->AddRef();

  Monitor2AutoLock lock(*sIndirectLayerTreesLock);
  CompositorBridgeParent::LayerTreeState& state =
      sIndirectLayerTrees[aLayersId];
  MOZ_ASSERT(!state.mController);
  state.mController = controller;

  return controller;
}

bool ContentCompositorBridgeParent::DeallocPAPZParent(PAPZParent* aActor) {
  RemoteContentController* controller =
      static_cast<RemoteContentController*>(aActor);
  controller->Release();
  return true;
}

#ifdef MOZ_BUILD_WEBRENDER
PWebRenderBridgeParent*
ContentCompositorBridgeParent::AllocPWebRenderBridgeParent(
    const wr::PipelineId& aPipelineId, const LayoutDeviceIntSize& aSize) {
  LayersId layersId = wr::AsLayersId(aPipelineId);
  // Check to see if this child process has access to this layer tree.
  if (!LayerTreeOwnerTracker::Get()->IsMapped(layersId, OtherPid())) {
    NS_ERROR(
        "Unexpected layers id in AllocPWebRenderBridgeParent; dropping "
        "message...");
    return nullptr;
  }

  RefPtr<CompositorBridgeParent> cbp = nullptr;
  RefPtr<WebRenderBridgeParent> root = nullptr;

  {  // scope lock
    Monitor2AutoLock lock(*sIndirectLayerTreesLock);
    MOZ_ASSERT(sIndirectLayerTrees.find(layersId) != sIndirectLayerTrees.end());
    MOZ_ASSERT(sIndirectLayerTrees[layersId].mWrBridge == nullptr);
    cbp = sIndirectLayerTrees[layersId].mParent;
    if (cbp) {
      root = sIndirectLayerTrees[cbp->RootLayerTreeId()].mWrBridge;
    }
  }

  nsTArray<RefPtr<wr::WebRenderAPI>> apis;
  bool cloneSuccess = false;
  if (root) {
    cloneSuccess = root->CloneWebRenderAPIs(apis);
  }

  if (!cloneSuccess) {
    // This could happen when this function is called after
    // CompositorBridgeParent destruction. This was observed during Tab move
    // between different windows.
    NS_WARNING(
        nsPrintfCString("Created child without a matching parent? root %p",
                        root.get())
            .get());
    WebRenderBridgeParent* parent =
        WebRenderBridgeParent::CreateDestroyed(aPipelineId);
    parent->AddRef();  // IPDL reference
    return parent;
  }

  RefPtr<AsyncImagePipelineManager> holder = root->AsyncImageManager();
  RefPtr<CompositorAnimationStorage> animStorage = cbp->GetAnimationStorage();
  WebRenderBridgeParent* parent = new WebRenderBridgeParent(
      this, aPipelineId, nullptr, root->CompositorScheduler(), std::move(apis),
      std::move(holder), std::move(animStorage), cbp->GetVsyncInterval());
  parent->AddRef();  // IPDL reference

  {  // scope lock
    Monitor2AutoLock lock(*sIndirectLayerTreesLock);
    sIndirectLayerTrees[layersId].mContentCompositorBridgeParent = this;
    sIndirectLayerTrees[layersId].mWrBridge = parent;
  }

  return parent;
}

bool ContentCompositorBridgeParent::DeallocPWebRenderBridgeParent(
    PWebRenderBridgeParent* aActor) {
  WebRenderBridgeParent* parent = static_cast<WebRenderBridgeParent*>(aActor);
  EraseLayerState(wr::AsLayersId(parent->PipelineId()));
  parent->Release();  // IPDL reference
  return true;
}
#endif  // MOZ_BUILD_WEBRENDER

mozilla::ipc::IPCResult ContentCompositorBridgeParent::RecvNotifyChildCreated(
    const LayersId& child, CompositorOptions* aOptions) {
  Monitor2AutoLock lock(*sIndirectLayerTreesLock);
  for (LayerTreeMap::iterator it = sIndirectLayerTrees.begin();
       it != sIndirectLayerTrees.end(); it++) {
    CompositorBridgeParent::LayerTreeState* lts = &it->second;
    if (lts->mParent && lts->mContentCompositorBridgeParent == this) {
      lts->mParent->NotifyChildCreated(child);
      *aOptions = lts->mParent->GetOptions();
      return IPC_OK();
    }
  }
  return IPC_FAIL_NO_REASON(this);
}

mozilla::ipc::IPCResult
ContentCompositorBridgeParent::RecvMapAndNotifyChildCreated(
    const LayersId& child, const base::ProcessId& pid,
    CompositorOptions* aOptions) {
  // This can only be called from the browser process, as the mapping
  // ensures proper window ownership of layer trees.
  return IPC_FAIL_NO_REASON(this);
}

mozilla::ipc::IPCResult ContentCompositorBridgeParent::RecvCheckContentOnlyTDR(
    const uint32_t& sequenceNum, bool* isContentOnlyTDR) {
  *isContentOnlyTDR = false;
#ifdef XP_WIN
  gfx::ContentDeviceData compositor;

  gfx::DeviceManagerDx* dm = gfx::DeviceManagerDx::Get();

  // Check that the D3D11 device sequence numbers match.
  gfx::D3D11DeviceStatus status;
  dm->ExportDeviceInfo(&status);

  if (sequenceNum == status.sequenceNumber() && !dm->HasDeviceReset()) {
    *isContentOnlyTDR = true;
  }

#endif
  return IPC_OK();
};

void ContentCompositorBridgeParent::ShadowLayersUpdated(
    LayerTransactionParent* aLayerTree, const TransactionInfo& aInfo,
    bool aHitTestUpdate) {
  LayersId id = aLayerTree->GetId();

  MOZ_ASSERT(id.IsValid());

  CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }
  MOZ_ASSERT(state->mParent);
  state->mParent->ScheduleRotationOnCompositorThread(aInfo.targetConfig(),
                                                     aInfo.isFirstPaint());

  Layer* shadowRoot = aLayerTree->GetRoot();
  if (shadowRoot) {
    CompositorBridgeParent::SetShadowProperties(shadowRoot);
  }
  UpdateIndirectTree(id, shadowRoot, aInfo.targetConfig());

  // Cache the plugin data for this remote layer tree
  state->mPluginData = aInfo.plugins();
  state->mUpdatedPluginDataAvailable = true;

  state->mParent->NotifyShadowTreeTransaction(
      id, aInfo.isFirstPaint(), aInfo.focusTarget(), aInfo.scheduleComposite(),
      aInfo.paintSequenceNumber(), aInfo.isRepeatTransaction(), aHitTestUpdate);

  // Send the 'remote paint ready' message to the content thread if it has
  // already asked.
  if (mNotifyAfterRemotePaint) {
    Unused << SendRemotePaintIsReady();
    mNotifyAfterRemotePaint = false;
  }

  if (aLayerTree->ShouldParentObserveEpoch()) {
    // Note that we send this through the window compositor, since this needs
    // to reach the widget owning the tab.
    Unused << state->mParent->SendObserveLayersUpdate(
        id, aLayerTree->GetChildEpoch(), true);
  }

  auto endTime = TimeStamp::Now();
#ifdef MOZ_GECKO_PROFILER
  if (profiler_can_accept_markers()) {
    class ContentBuildPayload : public ProfilerMarkerPayload {
     public:
      ContentBuildPayload(const mozilla::TimeStamp& aStartTime,
                          const mozilla::TimeStamp& aEndTime)
          : ProfilerMarkerPayload(aStartTime, aEndTime) {}
      mozilla::ProfileBufferEntryWriter::Length TagAndSerializationBytes()
          const override {
        return CommonPropsTagAndSerializationBytes();
      }
      void SerializeTagAndPayload(
          mozilla::ProfileBufferEntryWriter& aEntryWriter) const override {
        static const DeserializerTag tag = TagForDeserializer(Deserialize);
        SerializeTagAndCommonProps(tag, aEntryWriter);
      }
      void StreamPayload(SpliceableJSONWriter& aWriter,
                         const TimeStamp& aProcessStartTime,
                         UniqueStacks& aUniqueStacks) const override {
        StreamCommonProps("CONTENT_FULL_PAINT_TIME", aWriter, aProcessStartTime,
                          aUniqueStacks);
      }

     private:
      explicit ContentBuildPayload(CommonProps&& aCommonProps)
          : ProfilerMarkerPayload(std::move(aCommonProps)) {}
      static mozilla::UniquePtr<ProfilerMarkerPayload> Deserialize(
          mozilla::ProfileBufferEntryReader& aEntryReader) {
        ProfilerMarkerPayload::CommonProps props =
            DeserializeCommonProps(aEntryReader);
        return UniquePtr<ProfilerMarkerPayload>(
            new ContentBuildPayload(std::move(props)));
      }
    };
    AUTO_PROFILER_STATS(add_marker_with_ContentBuildPayload);
    profiler_add_marker_for_thread(
        profiler_current_thread_id(), JS::ProfilingCategoryPair::GRAPHICS,
        "CONTENT_FULL_PAINT_TIME",
        ContentBuildPayload(aInfo.transactionStart(), endTime));
  }
#endif
  Telemetry::Accumulate(
      Telemetry::CONTENT_FULL_PAINT_TIME,
      static_cast<uint32_t>(
          (endTime - aInfo.transactionStart()).ToMilliseconds()));

  RegisterPayloads(aLayerTree, aInfo.payload());

  aLayerTree->SetPendingTransactionId(
      aInfo.id(), aInfo.vsyncId(), aInfo.vsyncStart(), aInfo.refreshStart(),
      aInfo.transactionStart(), endTime, aInfo.containsSVG(), aInfo.url(),
      aInfo.fwdTime());
}

void ContentCompositorBridgeParent::DidCompositeLocked(
    LayersId aId, const VsyncId& aVsyncId, TimeStamp& aCompositeStart,
    TimeStamp& aCompositeEnd) {
  sIndirectLayerTreesLock->AssertCurrentThreadOwns();
  if (LayerTransactionParent* layerTree = sIndirectLayerTrees[aId].mLayerTree) {
    TransactionId transactionId =
        layerTree->FlushTransactionId(aVsyncId, aCompositeEnd);
    if (transactionId.IsValid()) {
      Unused << SendDidComposite(aId, transactionId, aCompositeStart,
                                 aCompositeEnd);
    }
#ifdef MOZ_BUILD_WEBRENDER
  } else if (sIndirectLayerTrees[aId].mWrBridge) {
    MOZ_ASSERT(false);  // this should never get called for a WR compositor
#endif
  }
}

void ContentCompositorBridgeParent::ScheduleComposite(
    LayerTransactionParent* aLayerTree) {
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id.IsValid());
  CompositorBridgeParent* parent;
  {  // scope lock
    Monitor2AutoLock lock(*sIndirectLayerTreesLock);
    parent = sIndirectLayerTrees[id].mParent;
  }
  if (parent) {
    parent->ScheduleComposite(aLayerTree);
  }
}

void ContentCompositorBridgeParent::NotifyClearCachedResources(
    LayerTransactionParent* aLayerTree) {
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id.IsValid());

  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(id);
  if (state && state->mParent) {
    // Note that we send this through the window compositor, since this needs
    // to reach the widget owning the tab.
    Unused << state->mParent->SendObserveLayersUpdate(
        id, aLayerTree->GetChildEpoch(), false);
  }
}

bool ContentCompositorBridgeParent::SetTestSampleTime(const LayersId& aId,
                                                      const TimeStamp& aTime) {
  MOZ_ASSERT(aId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aId);
  if (!state) {
    return false;
  }

  MOZ_ASSERT(state->mParent);
  return state->mParent->SetTestSampleTime(aId, aTime);
}

void ContentCompositorBridgeParent::LeaveTestMode(const LayersId& aId) {
  MOZ_ASSERT(aId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aId);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->LeaveTestMode(aId);
}

void ContentCompositorBridgeParent::ApplyAsyncProperties(
    LayerTransactionParent* aLayerTree, TransformsToSkip aSkip) {
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->ApplyAsyncProperties(aLayerTree, aSkip);
}

void ContentCompositorBridgeParent::SetTestAsyncScrollOffset(
    const LayersId& aLayersId, const ScrollableLayerGuid::ViewID& aScrollId,
    const CSSPoint& aPoint) {
  MOZ_ASSERT(aLayersId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->SetTestAsyncScrollOffset(aLayersId, aScrollId, aPoint);
}

void ContentCompositorBridgeParent::SetTestAsyncZoom(
    const LayersId& aLayersId, const ScrollableLayerGuid::ViewID& aScrollId,
    const LayerToParentLayerScale& aZoom) {
  MOZ_ASSERT(aLayersId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state) {
    return;
  }

  MOZ_ASSERT(state->mParent);
  state->mParent->SetTestAsyncZoom(aLayersId, aScrollId, aZoom);
}

void ContentCompositorBridgeParent::FlushApzRepaints(
    const LayersId& aLayersId) {
  MOZ_ASSERT(aLayersId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->FlushApzRepaints(aLayersId);
}

void ContentCompositorBridgeParent::GetAPZTestData(const LayersId& aLayersId,
                                                   APZTestData* aOutData) {
  MOZ_ASSERT(aLayersId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->GetAPZTestData(aLayersId, aOutData);
}

void ContentCompositorBridgeParent::SetConfirmedTargetAPZC(
    const LayersId& aLayersId, const uint64_t& aInputBlockId,
    const nsTArray<ScrollableLayerGuid>& aTargets) {
  MOZ_ASSERT(aLayersId.IsValid());
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->SetConfirmedTargetAPZC(aLayersId, aInputBlockId,
                                         std::move(aTargets));
}

AsyncCompositionManager* ContentCompositorBridgeParent::GetCompositionManager(
    LayerTransactionParent* aLayerTree) {
  LayersId id = aLayerTree->GetId();
  const CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state) {
    return nullptr;
  }

  MOZ_ASSERT(state->mParent);
  return state->mParent->GetCompositionManager(aLayerTree);
}

void ContentCompositorBridgeParent::DeferredDestroy() { mSelfRef = nullptr; }

ContentCompositorBridgeParent::~ContentCompositorBridgeParent() {
  MOZ_ASSERT(XRE_GetIOMessageLoop());
}

PTextureParent* ContentCompositorBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const LayersId& aId, const uint64_t& aSerial
#ifdef MOZ_BUILD_WEBRENDER
    ,
    const wr::MaybeExternalImageId& aExternalImageId
#endif
) {
  CompositorBridgeParent::LayerTreeState* state = nullptr;

  LayerTreeMap::iterator itr = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() != itr) {
    state = &itr->second;
  }

  TextureFlags flags = aFlags;

  LayersBackend actualBackend = LayersBackend::LAYERS_NONE;
  if (state && state->mLayerManager) {
    actualBackend = state->mLayerManager->GetBackendType();
  }

  if (!state) {
    // The compositor was recreated, and we're receiving layers updates for a
    // a layer manager that will soon be discarded or invalidated. We can't
    // return null because this will mess up deserialization later and we'll
    // kill the content process. Instead, we signal that the underlying
    // TextureHost should not attempt to access the compositor.
    flags |= TextureFlags::INVALID_COMPOSITOR;
  } else if (actualBackend != LayersBackend::LAYERS_NONE &&
             aLayersBackend != actualBackend) {
    gfxDevCrash(gfx::LogReason::PAllocTextureBackendMismatch)
        << "Texture backend is wrong";
  }

  return TextureHost::CreateIPDLActor(this, aSharedData, aReadLock,
                                      aLayersBackend, aFlags, aSerial
#ifdef MOZ_BUILD_WEBRENDER
                                      ,
                                      aExternalImageId
#endif
  );
}

bool ContentCompositorBridgeParent::DeallocPTextureParent(
    PTextureParent* actor) {
  return TextureHost::DestroyIPDLActor(actor);
}

bool ContentCompositorBridgeParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void ContentCompositorBridgeParent::UpdatePaintTime(
    LayerTransactionParent* aLayerTree, const TimeDuration& aPaintTime) {
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id.IsValid());

  CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->UpdatePaintTime(aLayerTree, aPaintTime);
}

void ContentCompositorBridgeParent::RegisterPayloads(
    LayerTransactionParent* aLayerTree,
    const nsTArray<CompositionPayload>& aPayload) {
  LayersId id = aLayerTree->GetId();
  MOZ_ASSERT(id.IsValid());

  CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(id);
  if (!state || !state->mParent) {
    return;
  }

  state->mParent->RegisterPayloads(aLayerTree, aPayload);
}

void ContentCompositorBridgeParent::ObserveLayersUpdate(
    LayersId aLayersId, LayersObserverEpoch aEpoch, bool aActive) {
  MOZ_ASSERT(aLayersId.IsValid());

  CompositorBridgeParent::LayerTreeState* state =
      CompositorBridgeParent::GetIndirectShadowTree(aLayersId);
  if (!state || !state->mParent) {
    return;
  }

  Unused << state->mParent->SendObserveLayersUpdate(aLayersId, aEpoch, aActive);
}

}  // namespace layers
}  // namespace mozilla
