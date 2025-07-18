/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/MediaCapabilities.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/StorageManager.h"
#include "mozilla/dom/WorkerNavigator.h"
#include "mozilla/dom/WorkerNavigatorBinding.h"
#include "mozilla/dom/network/Connection.h"
#include "mozilla/StaticPrefs_privacy.h"

#include "nsProxyRelease.h"
#include "nsRFPService.h"
#include "RuntimeService.h"

#include "mozilla/dom/Document.h"

#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"

#include "mozilla/dom/Navigator.h"

#ifdef MOZ_WEBGPU
#include "mozilla/webgpu/Instance.h"
#endif

namespace mozilla {
namespace dom {

using namespace workerinternals;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WorkerNavigator, mStorageManager,
                                      mConnection, mMediaCapabilities
#ifdef MOZ_WEBGPU
                                      , mWebGpu
#endif
                                      );

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WorkerNavigator, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WorkerNavigator, Release)

WorkerNavigator::WorkerNavigator(const NavigatorProperties& aProperties,
                                 bool aOnline)
    : mProperties(aProperties), mOnline(aOnline) {}

WorkerNavigator::~WorkerNavigator() = default;

/* static */
already_AddRefed<WorkerNavigator> WorkerNavigator::Create(bool aOnLine) {
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  const RuntimeService::NavigatorProperties& properties =
      rts->GetNavigatorProperties();

  RefPtr<WorkerNavigator> navigator = new WorkerNavigator(properties, aOnLine);

  return navigator.forget();
}

JSObject* WorkerNavigator::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return WorkerNavigator_Binding::Wrap(aCx, this, aGivenProto);
}

void WorkerNavigator::SetLanguages(const nsTArray<nsString>& aLanguages) {
  WorkerNavigator_Binding::ClearCachedLanguagesValue(this);
  mProperties.mLanguages = aLanguages;
}

void WorkerNavigator::GetAppName(nsString& aAppName,
                                 CallerType aCallerType) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if ((!mProperties.mAppNameOverridden.IsEmpty() ||
       StaticPrefs::privacy_resistFingerprinting()) &&
      !workerPrivate->UsesSystemPrincipal()) {
    // We will spoof this value when 'privacy.resistFingerprinting' is true.
    // See nsRFPService.h for spoofed value.
    aAppName = StaticPrefs::privacy_resistFingerprinting()
                   ? NS_LITERAL_STRING_FROM_CSTRING(SPOOFED_APPNAME)
                   : mProperties.mAppNameOverridden;
  } else {
    aAppName = mProperties.mAppName;
  }
}

void WorkerNavigator::GetAppVersion(nsString& aAppVersion,
                                    CallerType aCallerType,
                                    ErrorResult& aRv) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if ((!mProperties.mAppVersionOverridden.IsEmpty() ||
       StaticPrefs::privacy_resistFingerprinting()) &&
      !workerPrivate->UsesSystemPrincipal()) {
    // We will spoof this value when 'privacy.resistFingerprinting' is true.
    // See nsRFPService.h for spoofed value.
    aAppVersion = StaticPrefs::privacy_resistFingerprinting()
                      ? NS_LITERAL_STRING_FROM_CSTRING(SPOOFED_APPVERSION)
                      : mProperties.mAppVersionOverridden;
  } else {
    aAppVersion = mProperties.mAppVersion;
  }
}

void WorkerNavigator::GetPlatform(nsString& aPlatform, CallerType aCallerType,
                                  ErrorResult& aRv) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if ((!mProperties.mPlatformOverridden.IsEmpty() ||
       StaticPrefs::privacy_resistFingerprinting()) &&
      !workerPrivate->UsesSystemPrincipal()) {
    // We will spoof this value when 'privacy.resistFingerprinting' is true.
    // See nsRFPService.h for spoofed value.
    aPlatform = StaticPrefs::privacy_resistFingerprinting()
                    ? NS_LITERAL_STRING_FROM_CSTRING(SPOOFED_PLATFORM)
                    : mProperties.mPlatformOverridden;
  } else {
    aPlatform = mProperties.mPlatform;
  }
}

namespace {

class GetUserAgentRunnable final : public WorkerMainThreadRunnable {
  nsString& mUA;

 public:
  GetUserAgentRunnable(WorkerPrivate* aWorkerPrivate, nsString& aUA)
      : WorkerMainThreadRunnable(aWorkerPrivate, "UserAgent getter"_ns),
        mUA(aUA) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  virtual bool MainThreadRun() override {
    AssertIsOnMainThread();

    nsCOMPtr<nsPIDOMWindowInner> window = mWorkerPrivate->GetWindow();

    bool isCallerChrome = mWorkerPrivate->UsesSystemPrincipal();
    nsresult rv = dom::Navigator::GetUserAgent(
        window, mWorkerPrivate->GetPrincipal(), isCallerChrome, mUA);
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to retrieve user-agent from the worker thread.");
    }

    return true;
  }
};

}  // namespace

void WorkerNavigator::GetUserAgent(nsString& aUserAgent, CallerType aCallerType,
                                   ErrorResult& aRv) const {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<GetUserAgentRunnable> runnable =
      new GetUserAgentRunnable(workerPrivate, aUserAgent);

  runnable->Dispatch(Canceling, aRv);
}

uint64_t WorkerNavigator::HardwareConcurrency() const {
  RuntimeService* rts = RuntimeService::GetService();
  MOZ_ASSERT(rts);

  return rts->ClampedHardwareConcurrency();
}

StorageManager* WorkerNavigator::Storage() {
  if (!mStorageManager) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    RefPtr<nsIGlobalObject> global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mStorageManager = new StorageManager(global);
  }

  return mStorageManager;
}

network::Connection* WorkerNavigator::GetConnection(ErrorResult& aRv) {
  if (!mConnection) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    mConnection = network::Connection::CreateForWorker(workerPrivate, aRv);
  }

  return mConnection;
}

dom::MediaCapabilities* WorkerNavigator::MediaCapabilities() {
  if (!mMediaCapabilities) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsIGlobalObject* global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mMediaCapabilities = new dom::MediaCapabilities(global);
  }
  return mMediaCapabilities;
}

#ifdef MOZ_WEBGPU
webgpu::Instance* WorkerNavigator::Gpu() {
  if (!mWebGpu) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    nsIGlobalObject* global = workerPrivate->GlobalScope();
    MOZ_ASSERT(global);

    mWebGpu = webgpu::Instance::Create(global);
  }
  return mWebGpu;
}
#endif

}  // namespace dom
}  // namespace mozilla
