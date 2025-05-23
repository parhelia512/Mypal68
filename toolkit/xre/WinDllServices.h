/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinDllServices_h
#define mozilla_WinDllServices_h

#include "mozilla/CombinedStacks.h"
#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/Maybe.h"
#include "mozilla/ModuleEvaluator_windows.h"
#include "mozilla/mozalloc.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

namespace mozilla {

// Holds the data that telemetry requests, and will be later converted to the
// telemetry payload.
class UntrustedModuleLoadTelemetryData {
 public:
  UntrustedModuleLoadTelemetryData() = default;
  // Moves allowed, no copies.
  UntrustedModuleLoadTelemetryData(UntrustedModuleLoadTelemetryData&&) =
      default;
  UntrustedModuleLoadTelemetryData(
      const UntrustedModuleLoadTelemetryData& aOther) = delete;

  Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy> mEvents;
  Telemetry::CombinedStacks mStacks;
  int mErrorModules = 0;
  Maybe<double> mXULLoadDurationMS;
};

class UntrustedModulesManager;

class DllServices : public mozilla::glue::DllServices {
 public:
  static DllServices* Get();

  static const char* kTopicDllLoadedMainThread;
  static const char* kTopicDllLoadedNonMainThread;

 private:
  DllServices();
  ~DllServices() = default;

  void NotifyDllLoad(const bool aIsMainThread,
                     const nsString& aDllName) override;

  void NotifyUntrustedModuleLoads(
      const Vector<glue::ModuleLoadEvent, 0, InfallibleAllocPolicy>& aEvents)
      override;

  UniquePtr<UntrustedModulesManager> mUntrustedModulesManager;
};

}  // namespace mozilla

#endif  // mozilla_WinDllServices_h
