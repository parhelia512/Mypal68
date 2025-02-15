/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_QuotaManagerService_h
#define mozilla_dom_quota_QuotaManagerService_h

#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/HalBatteryInformation.h"
#include "mozilla/UniquePtr.h"
#include "nsIObserver.h"
#include "nsIQuotaManagerService.h"

#define QUOTAMANAGER_SERVICE_CONTRACTID \
  "@mozilla.org/dom/quota-manager-service;1"

namespace mozilla {
namespace ipc {

class PBackgroundChild;

}  // namespace ipc

namespace hal {
class BatteryInformation;
}

namespace dom {
namespace quota {

class QuotaChild;
class QuotaManager;

class QuotaManagerService final : public nsIQuotaManagerService,
                                  public nsIObserver,
                                  public hal::BatteryObserver {
  typedef mozilla::ipc::PBackgroundChild PBackgroundChild;

  class BackgroundCreateCallback;
  class PendingRequestInfo;
  class UsageRequestInfo;
  class RequestInfo;
  class IdleMaintenanceInfo;

  QuotaChild* mBackgroundActor;

  bool mBackgroundActorFailed;
  bool mIdleObserverRegistered;

 public:
  // Returns a non-owning reference.
  static QuotaManagerService* GetOrCreate();

  // Returns a non-owning reference.
  static QuotaManagerService* Get();

  // No one should call this but the factory.
  static already_AddRefed<QuotaManagerService> FactoryCreate();

  void ClearBackgroundActor();

  // Called when a process is being shot down. Aborts any running operations
  // for the given process.
  void AbortOperationsForProcess(ContentParentId aContentParentId);

 private:
  QuotaManagerService();
  ~QuotaManagerService();

  nsresult Init();

  void Destroy();

  nsresult EnsureBackgroundActor();

  nsresult InitiateRequest(PendingRequestInfo& aInfo);

  void PerformIdleMaintenance();

  void RemoveIdleObserver();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIQUOTAMANAGERSERVICE
  NS_DECL_NSIOBSERVER

  // BatteryObserver override
  void Notify(const hal::BatteryInformation& aBatteryInfo) override;
};

}  // namespace quota
}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_quota_QuotaManagerService_h */
