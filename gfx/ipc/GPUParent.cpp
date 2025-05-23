/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
#  include "WMF.h"
#endif
#include "GPUParent.h"
#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxPlatform.h"
#include "GLContextProvider.h"
#include "GPUProcessHost.h"
#include "GPUProcessManager.h"
#include "mozilla/Assertions.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/RemoteDecoderManagerChild.h"
#include "mozilla/RemoteDecoderManagerParent.h"
#include "mozilla/dom/MemoryReportRequest.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/ipc/CrashReporterClient.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/layers/APZInputBridgeParent.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/APZPublicUtils.h"  // for apz::InitializeGlobalState
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorManagerParent.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/ImageBridgeParent.h"
#include "mozilla/layers/LayerTreeOwnerTracker.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/layers/MemoryReportingMLGPU.h"
#include "mozilla/layers/VideoBridgeParent.h"
#ifdef MOZ_BUILD_WEBRENDER
#  include "mozilla/image/ImageMemoryReporter.h"
#  include "mozilla/webrender/RenderThread.h"
#  include "mozilla/webrender/WebRenderAPI.h"
#endif
#include "mozilla/HangDetails.h"
#include "nscore.h"
#include "nsDebugImpl.h"
#include "nsIGfxInfo.h"
#include "nsThreadManager.h"
#include "prenv.h"
#include "ProcessUtils.h"
#ifdef MOZ_VR
#  include "VRGPUChild.h"
#  include "VRManager.h"
#  include "VRManagerParent.h"
#endif
#include "VsyncBridgeParent.h"
#include "cairo.h"
#include "skia/include/core/SkGraphics.h"
#if defined(XP_WIN)
#  include "DeviceManagerD3D9.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/widget/WinCompositorWindowThread.h"
#  include "mozilla/WindowsVersion.h"
#  include <process.h>
#  include <dwrite.h>
#endif
#ifdef MOZ_WIDGET_GTK
#  include <gtk/gtk.h>
#  include "skia/include/ports/SkTypeface_cairo.h"
#endif
#ifdef MOZ_GECKO_PROFILER
#  include "ChildProfilerController.h"
#endif
#include "nsAppRunner.h"

namespace mozilla {
namespace gfx {

using namespace ipc;
using namespace layers;

static GPUParent* sGPUParent;

GPUParent::GPUParent() : mLaunchTime(TimeStamp::Now()) { sGPUParent = this; }

GPUParent::~GPUParent() { sGPUParent = nullptr; }

/* static */
GPUParent* GPUParent::GetSingleton() { return sGPUParent; }

bool GPUParent::Init(base::ProcessId aParentPid, const char* aParentBuildID,
                     MessageLoop* aIOLoop, UniquePtr<IPC::Channel> aChannel) {
  // Initialize the thread manager before starting IPC. Otherwise, messages
  // may be posted to the main thread and we won't be able to process them.
  if (NS_WARN_IF(NS_FAILED(nsThreadManager::get().Init()))) {
    return false;
  }

  // Now it's safe to start IPC.
  if (NS_WARN_IF(!Open(std::move(aChannel), aParentPid, aIOLoop))) {
    return false;
  }

  nsDebugImpl::SetMultiprocessMode("GPU");

  // This must be checked before any IPDL message, which may hit sentinel
  // errors due to parent and content processes having different
  // versions.
  MessageChannel* channel = GetIPCChannel();
  if (channel && !channel->SendBuildIDsMatchMessage(aParentBuildID)) {
    // We need to quit this process if the buildID doesn't match the parent's.
    // This can occur when an update occurred in the background.
    ProcessChild::QuickExit();
  }

  if (NS_FAILED(NS_InitMinimalXPCOM())) {
    return false;
  }

  // Init crash reporter support.
  CrashReporterClient::InitSingleton(this);

  gfxConfig::Init();
  gfxVars::Initialize();
  gfxPlatform::InitNullMetadata();
  // Ensure our Factory is initialised, mainly for gfx logging to work.
  gfxPlatform::InitMoz2DLogging();
  mlg::InitializeMemoryReporters();
#if defined(XP_WIN)
  DeviceManagerDx::Init();
  DeviceManagerD3D9::Init();
#endif

  CompositorThreadHolder::Start();
  APZThreadUtils::SetControllerThread(MessageLoop::current());
  apz::InitializeGlobalState();
  LayerTreeOwnerTracker::Initialize();
  mozilla::ipc::SetThisProcessName("GPU Process");
#ifdef XP_WIN
  wmf::MFStartup();
#endif
  return true;
}

void GPUParent::NotifyDeviceReset() {
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(NS_NewRunnableFunction(
        "gfx::GPUParent::NotifyDeviceReset",
        []() -> void { GPUParent::GetSingleton()->NotifyDeviceReset(); }));
    return;
  }

  // Reset and reinitialize the compositor devices
#ifdef XP_WIN
  if (!DeviceManagerDx::Get()->MaybeResetAndReacquireDevices()) {
    // If the device doesn't need to be reset then the device
    // has already been reset by a previous NotifyDeviceReset message.
    return;
  }
#endif

  // Notify the main process that there's been a device reset
  // and that they should reset their compositors and repaint
  GPUDeviceData data;
  RecvGetDeviceStatus(&data);
  Unused << SendNotifyDeviceReset(data);
}

already_AddRefed<PAPZInputBridgeParent> GPUParent::AllocPAPZInputBridgeParent(
    const LayersId& aLayersId) {
  return MakeAndAddRef<APZInputBridgeParent>(aLayersId);
}

mozilla::ipc::IPCResult GPUParent::RecvInit(
    nsTArray<GfxVarUpdate>&& vars, const DevicePrefs& devicePrefs,
    nsTArray<LayerTreeIdMapping>&& aMappings) {
  for (const auto& var : vars) {
    gfxVars::ApplyUpdate(var);
  }

  // Inherit device preferences.
  gfxConfig::Inherit(Feature::HW_COMPOSITING, devicePrefs.hwCompositing());
  gfxConfig::Inherit(Feature::D3D11_COMPOSITING,
                     devicePrefs.d3d11Compositing());
  gfxConfig::Inherit(Feature::D3D9_COMPOSITING, devicePrefs.d3d9Compositing());
  gfxConfig::Inherit(Feature::OPENGL_COMPOSITING, devicePrefs.oglCompositing());
  gfxConfig::Inherit(Feature::ADVANCED_LAYERS, devicePrefs.advancedLayers());
  gfxConfig::Inherit(Feature::DIRECT2D, devicePrefs.useD2D1());

#ifdef MOZ_BUILD_WEBRENDER
  {  // Let the crash reporter know if we've got WR enabled or not. For other
    // processes this happens in gfxPlatform::InitWebRenderConfig.
    ScopedGfxFeatureReporter reporter("WR",
                                      gfxPlatform::WebRenderPrefEnabled());
    if (gfxVars::UseWebRender()) {
      reporter.SetSuccessful();
    }
  }
#endif

  for (const LayerTreeIdMapping& map : aMappings) {
    LayerTreeOwnerTracker::Get()->Map(map.layersId(), map.ownerId());
  }

  // We bypass gfxPlatform::Init, so we must initialize any relevant libraries
  // here that would normally be initialized there.
  SkGraphics::Init();

#if defined(XP_WIN)
  if (gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING)) {
    DeviceManagerDx::Get()->CreateCompositorDevices();
  }
#  ifdef MOZ_BUILD_WEBRENDER
  if (gfxVars::UseWebRender()) {
    DeviceManagerDx::Get()->CreateDirectCompositionDevice();
    // Ensure to initialize GfxInfo
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    Unused << gfxInfo;

    Factory::EnsureDWriteFactory();
  }
#  endif
#endif

#if defined(MOZ_WIDGET_GTK)
  char* display_name = PR_GetEnv("MOZ_GDK_DISPLAY");
  if (!display_name) {
    bool waylandDisabled = true;
#  ifdef MOZ_WAYLAND
    waylandDisabled = IsWaylandDisabled();
#  endif
    if (waylandDisabled) {
      display_name = PR_GetEnv("DISPLAY");
    }
  }
  if (display_name) {
    int argc = 3;
    char option_name[] = "--display";
    char* argv[] = {// argv0 is unused because g_set_prgname() was called in
                    // XRE_InitChildProcess().
                    nullptr, option_name, display_name, nullptr};
    char** argvp = argv;
    gtk_init(&argc, &argvp);
  } else {
    gtk_init(nullptr, nullptr);
  }

  // Ensure we have an FT library for font instantiation.
  // This would normally be set by gfxPlatform::Init().
  // Since we bypass that, we must do it here instead.
#  ifdef MOZ_BUILD_WEBRENDER
  if (gfxVars::UseWebRender()) {
    FT_Library library = Factory::NewFTLibrary();
    MOZ_ASSERT(library);
    Factory::SetFTLibrary(library);

    SkInitCairoFT(true);
  }
#  endif
#endif

  // Make sure to do this *after* we update gfxVars above.
#ifdef MOZ_BUILD_WEBRENDER
  if (gfxVars::UseWebRender()) {
    wr::RenderThread::Start();
    image::ImageMemoryReporter::InitForWebRender();
  }
#  ifdef XP_WIN
  else
#  endif
  {
    if (gfxVars::UseDoubleBufferingWithCompositor()) {
      // This is needed to avoid freezing the window on a device crash on double
      // buffering, see bug 1549674.
      widget::WinCompositorWindowThread::Start();
    }
  }
#endif

#ifdef MOZ_VR
  VRManager::ManagerInit();
#endif
  // Send a message to the UI process that we're done.
  GPUDeviceData data;
  RecvGetDeviceStatus(&data);
  Unused << SendInitComplete(data);

  Telemetry::AccumulateTimeDelta(Telemetry::GPU_PROCESS_INITIALIZATION_TIME_MS,
                                 mLaunchTime);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitCompositorManager(
    Endpoint<PCompositorManagerParent>&& aEndpoint) {
  CompositorManagerParent::Create(std::move(aEndpoint), /* aIsRoot */ true);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVsyncBridge(
    Endpoint<PVsyncBridgeParent>&& aVsyncEndpoint) {
  mVsyncBridge = VsyncBridgeParent::Start(std::move(aVsyncEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitImageBridge(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  ImageBridgeParent::CreateForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVideoBridge(
    Endpoint<PVideoBridgeParent>&& aEndpoint) {
  VideoBridgeParent::Open(std::move(aEndpoint), VideoBridgeSource::RddProcess);
  return IPC_OK();
}

#ifdef MOZ_VR
mozilla::ipc::IPCResult GPUParent::RecvInitVRManager(
    Endpoint<PVRManagerParent>&& aEndpoint) {
  VRManagerParent::CreateForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitVR(
    Endpoint<PVRGPUChild>&& aEndpoint) {
  gfx::VRGPUChild::InitForGPUProcess(std::move(aEndpoint));
  return IPC_OK();
}
#endif

mozilla::ipc::IPCResult GPUParent::RecvInitUiCompositorController(
    const LayersId& aRootLayerTreeId,
    Endpoint<PUiCompositorControllerParent>&& aEndpoint) {
  UiCompositorControllerParent::Start(aRootLayerTreeId, std::move(aEndpoint));
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvInitProfiler(
    Endpoint<PProfilerChild>&& aEndpoint) {
#ifdef MOZ_GECKO_PROFILER
  mProfilerController = ChildProfilerController::Create(std::move(aEndpoint));
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvUpdateVar(const GfxVarUpdate& aUpdate) {
  gfxVars::ApplyUpdate(aUpdate);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvPreferenceUpdate(const Pref& aPref) {
  Preferences::SetPreference(aPref);
  return IPC_OK();
}

static void CopyFeatureChange(Feature aFeature, Maybe<FeatureFailure>* aOut) {
  FeatureState& feature = gfxConfig::GetFeature(aFeature);
  if (feature.DisabledByDefault() || feature.IsEnabled()) {
    // No change:
    //   - Disabled-by-default means the parent process told us not to use this
    //   feature.
    //   - Enabled means we were told to use this feature, and we didn't
    //   discover anything
    //     that would prevent us from doing so.
    *aOut = Nothing();
    return;
  }

  MOZ_ASSERT(!feature.IsEnabled());

  nsCString message;
  message.AssignASCII(feature.GetFailureMessage());

  *aOut =
      Some(FeatureFailure(feature.GetValue(), message, feature.GetFailureId()));
}

mozilla::ipc::IPCResult GPUParent::RecvGetDeviceStatus(GPUDeviceData* aOut) {
  CopyFeatureChange(Feature::D3D11_COMPOSITING, &aOut->d3d11Compositing());
  CopyFeatureChange(Feature::D3D9_COMPOSITING, &aOut->d3d9Compositing());
  CopyFeatureChange(Feature::OPENGL_COMPOSITING, &aOut->oglCompositing());
  CopyFeatureChange(Feature::ADVANCED_LAYERS, &aOut->advancedLayers());

#if defined(XP_WIN)
  if (DeviceManagerDx* dm = DeviceManagerDx::Get()) {
    D3D11DeviceStatus deviceStatus;
    dm->ExportDeviceInfo(&deviceStatus);
    aOut->gpuDevice() = Some(deviceStatus);
  }
#else
  aOut->gpuDevice() = Nothing();
#endif

  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvSimulateDeviceReset(
    GPUDeviceData* aOut) {
#if defined(XP_WIN)
  DeviceManagerDx::Get()->ForceDeviceReset(
      ForcedDeviceResetReason::COMPOSITOR_UPDATED);
  DeviceManagerDx::Get()->MaybeResetAndReacquireDevices();
#  ifdef MOZ_BUILD_WEBRENDER
  if (gfxVars::UseWebRender()) {
    wr::RenderThread::Get()->SimulateDeviceReset();
  }
#  endif
#endif
  RecvGetDeviceStatus(aOut);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNewContentCompositorManager(
    Endpoint<PCompositorManagerParent>&& aEndpoint) {
  CompositorManagerParent::Create(std::move(aEndpoint), /* aIsRoot */ false);
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNewContentImageBridge(
    Endpoint<PImageBridgeParent>&& aEndpoint) {
  if (!ImageBridgeParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

#ifdef MOZ_VR
mozilla::ipc::IPCResult GPUParent::RecvNewContentVRManager(
    Endpoint<PVRManagerParent>&& aEndpoint) {
  if (!VRManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}
#endif

mozilla::ipc::IPCResult GPUParent::RecvNewContentRemoteDecoderManager(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  if (!RemoteDecoderManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvAddLayerTreeIdMapping(
    const LayerTreeIdMapping& aMapping) {
  LayerTreeOwnerTracker::Get()->Map(aMapping.layersId(), aMapping.ownerId());
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvRemoveLayerTreeIdMapping(
    const LayerTreeIdMapping& aMapping) {
  LayerTreeOwnerTracker::Get()->Unmap(aMapping.layersId(), aMapping.ownerId());
  CompositorBridgeParent::DeallocateLayerTreeId(aMapping.layersId());
  return IPC_OK();
}

mozilla::ipc::IPCResult GPUParent::RecvNotifyGpuObservers(
    const nsCString& aTopic) {
  nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
  MOZ_ASSERT(obsSvc);
  if (obsSvc) {
    obsSvc->NotifyObservers(nullptr, aTopic.get(), nullptr);
  }
  return IPC_OK();
}

/* static */
void GPUParent::GetGPUProcessName(nsACString& aStr) {
  auto processType = XRE_GetProcessType();
  unsigned pid = 0;
  if (processType == GeckoProcessType_GPU) {
    pid = getpid();
  } else {
    MOZ_DIAGNOSTIC_ASSERT(processType == GeckoProcessType_Default);
    pid = GPUProcessManager::Get()->GPUProcessPid();
  }

  nsPrintfCString processName("GPU (pid %u)", pid);
  aStr.Assign(processName);
}

mozilla::ipc::IPCResult GPUParent::RecvRequestMemoryReport(
    const uint32_t& aGeneration, const bool& aAnonymize,
    const bool& aMinimizeMemoryUsage, const Maybe<FileDescriptor>& aDMDFile) {
  nsAutoCString processName;
  GetGPUProcessName(processName);

  mozilla::dom::MemoryReportRequestClient::Start(
      aGeneration, aAnonymize, aMinimizeMemoryUsage, aDMDFile, processName,
      [&](const MemoryReport& aReport) {
        Unused << GetSingleton()->SendAddMemoryReport(aReport);
      },
      [&](const uint32_t& aGeneration) {
        return GetSingleton()->SendFinishMemoryReport(aGeneration);
      });
  return IPC_OK();
}

#ifdef MOZ_VR
mozilla::ipc::IPCResult GPUParent::RecvShutdownVR() {
  if (StaticPrefs::dom_vr_process_enabled_AtStartup()) {
    VRGPUChild::Shutdown();
  }
  return IPC_OK();
}
#endif

void GPUParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (AbnormalShutdown == aWhy) {
    NS_WARNING("Shutting down GPU process early due to a crash!");
    ProcessChild::QuickExit();
  }

#ifdef XP_WIN
  wmf::MFShutdown();
#endif

#ifndef NS_FREE_PERMANENT_DATA
  // No point in going through XPCOM shutdown because we don't keep persistent
  // state.
  ProcessChild::QuickExit();
#endif

#ifdef MOZ_GECKO_PROFILER
  if (mProfilerController) {
    mProfilerController->Shutdown();
    mProfilerController = nullptr;
  }
#endif

  if (mVsyncBridge) {
    mVsyncBridge->Shutdown();
    mVsyncBridge = nullptr;
  }
  RemoteDecoderManagerParent::ShutdownVideoBridge();
  CompositorThreadHolder::Shutdown();
#ifdef MOZ_BUILD_WEBRENDER
  // There is a case that RenderThread exists when gfxVars::UseWebRender() is
  // false. This could happen when WebRender was fallbacked to compositor.
  if (wr::RenderThread::Get()) {
    wr::RenderThread::ShutDown();
  }
#endif
#ifdef XP_WIN
  if (widget::WinCompositorWindowThread::Get()) {
    widget::WinCompositorWindowThread::ShutDown();
  }
#endif

#ifdef MOZ_BUILD_WEBRENDER
  image::ImageMemoryReporter::ShutdownForWebRender();
#endif

  // Shut down the default GL context provider.
  gl::GLContextProvider::Shutdown();

#if defined(XP_WIN)
  // The above shutdown calls operate on the available context providers on
  // most platforms.  Windows is a "special snowflake", though, and has three
  // context providers available, so we have to shut all of them down.
  // We should only support the default GL provider on Windows; then, this
  // could go away. Unfortunately, we currently support WGL (the default) for
  // WebGL on Optimus.
  gl::GLContextProviderEGL::Shutdown();
#endif

  Factory::ShutDown();

  // We bypass gfxPlatform shutdown, so we must shutdown any libraries here
  // that would normally be handled by it.
#ifdef NS_FREE_PERMANENT_DATA
  SkGraphics::PurgeFontCache();
  cairo_debug_reset_static_data();
#endif

#if defined(XP_WIN)
  DeviceManagerDx::Shutdown();
  DeviceManagerD3D9::Shutdown();
#endif
  LayerTreeOwnerTracker::Shutdown();
  gfxVars::Shutdown();
  gfxConfig::Shutdown();
  CrashReporterClient::DestroySingleton();
  XRE_ShutdownChildProcess();
}

}  // namespace gfx
}  // namespace mozilla
