/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_remoteserviceworkerregistrationimpl_h__
#define mozilla_dom_remoteserviceworkerregistrationimpl_h__

#include "ServiceWorkerRegistration.h"

namespace mozilla {
namespace dom {

class ServiceWorkerRegistrationChild;

class RemoteServiceWorkerRegistrationImpl final
    : public ServiceWorkerRegistration::Inner {
  ServiceWorkerRegistrationChild* mActor;
  ServiceWorkerRegistration* mOuter;
  bool mShutdown;

  ~RemoteServiceWorkerRegistrationImpl();

  void Shutdown();

  // ServiceWorkerRegistration::Inner implementation
  void SetServiceWorkerRegistration(ServiceWorkerRegistration* aReg) override;

  void ClearServiceWorkerRegistration(ServiceWorkerRegistration* aReg) override;

  void Update(const nsCString& aNewestWorkerScriptUrl,
              ServiceWorkerRegistrationCallback&& aSuccessCB,
              ServiceWorkerFailureCallback&& aFailureCB) override;

  void Unregister(ServiceWorkerBoolCallback&& aSuccessCB,
                  ServiceWorkerFailureCallback&& aFailureCB) override;

 public:
  explicit RemoteServiceWorkerRegistrationImpl(
      const ServiceWorkerRegistrationDescriptor& aDescriptor);

  void RevokeActor(ServiceWorkerRegistrationChild* aActor);

  void UpdateState(const ServiceWorkerRegistrationDescriptor& aDescriptor);

  void FireUpdateFound();

  NS_INLINE_DECL_REFCOUNTING(RemoteServiceWorkerRegistrationImpl, override)
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_remoteserviceworkerregistrationimpl_h__
