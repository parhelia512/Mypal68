/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
#  undef UNICODE
#  undef _UNICODE
#endif

#include "VTuneProfiler.h"

#include <memory>

#include "BaseProfiler.h"

using namespace std;

namespace mozilla {
namespace baseprofiler {

VTuneProfiler* VTuneProfiler::mInstance = nullptr;

void VTuneProfiler::Initialize() {
  // This is just a 'dirty trick' to find out if the ittnotify DLL was found.
  // If it wasn't this function always returns 0, otherwise it returns
  // incrementing numbers, if the library was found this wastes 2 events but
  // that should be okay.
  // TODO re-implement here if vtune is needed
  // __itt_event testEvent =
  //     __itt_event_create("Test event", strlen("Test event"));
  // testEvent = __itt_event_create("Test event 2", strlen("Test event 2"));

  // if (testEvent) {
  //   mInstance = new VTuneProfiler();
  // }
}

void VTuneProfiler::Shutdown() {}

void VTuneProfiler::TraceInternal(const char* aName, TracingKind aKind) {
  // TODO re-implement here if vtune is needed
  // string str(aName);

  // auto iter = mStrings.find(str);

  // __itt_event event;
  // if (iter != mStrings.end()) {
  //   event = iter->second;
  // } else {
  //   event = __itt_event_create(aName, str.length());
  //   mStrings.insert({str, event});
  // }

  // if (aKind == TRACING_INTERVAL_START || aKind == TRACING_EVENT) {
  //   // VTune will consider starts not matched with an end to be single point
  //   in
  //   // time events.
  //   __itt_event_start(event);
  // } else {
  //   __itt_event_end(event);
  // }
}

void VTuneProfiler::RegisterThreadInternal(const char* aName) {
  // TODO re-implement here if vtune is needed
  // string str(aName);

  // if (!str.compare("Main Thread (Base Profiler)")) {
  //   // Process main thread.
  //   switch (XRE_GetProcessType()) {
  //     case GeckoProcessType::GeckoProcessType_Default:
  //       __itt_thread_set_name("Main Process");
  //       break;
  //     case GeckoProcessType::GeckoProcessType_Content:
  //       __itt_thread_set_name("Content Process");
  //       break;
  //     case GeckoProcessType::GeckoProcessType_GMPlugin:
  //       __itt_thread_set_name("Plugin Process");
  //       break;
  //     case GeckoProcessType::GeckoProcessType_GPU:
  //       __itt_thread_set_name("GPU Process");
  //       break;
  //     default:
  //       __itt_thread_set_name("Unknown Process");
  //   }
  //   return;
  // }
  // __itt_thread_set_name(aName);
}

}  // namespace baseprofiler
}  // namespace mozilla
