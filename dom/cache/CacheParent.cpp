/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/CacheParent.h"

#include "mozilla/dom/cache/CacheOpParent.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
namespace cache {

// Declared in ActorUtils.h
void DeallocPCacheParent(PCacheParent* aActor) { delete aActor; }

CacheParent::CacheParent(SafeRefPtr<cache::Manager> aManager, CacheId aCacheId)
    : mManager(std::move(aManager)), mCacheId(aCacheId) {
  MOZ_COUNT_CTOR(cache::CacheParent);
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  mManager->AddRefCacheId(mCacheId);
}

CacheParent::~CacheParent() {
  MOZ_COUNT_DTOR(cache::CacheParent);
  MOZ_DIAGNOSTIC_ASSERT(!mManager);
}

void CacheParent::ActorDestroy(ActorDestroyReason aReason) {
  MOZ_DIAGNOSTIC_ASSERT(mManager);
  mManager->ReleaseCacheId(mCacheId);
  mManager = nullptr;
}

PCacheOpParent* CacheParent::AllocPCacheOpParent(const CacheOpArgs& aOpArgs) {
  if (aOpArgs.type() != CacheOpArgs::TCacheMatchArgs &&
      aOpArgs.type() != CacheOpArgs::TCacheMatchAllArgs &&
      aOpArgs.type() != CacheOpArgs::TCachePutAllArgs &&
      aOpArgs.type() != CacheOpArgs::TCacheDeleteArgs &&
      aOpArgs.type() != CacheOpArgs::TCacheKeysArgs) {
    MOZ_CRASH("Invalid operation sent to Cache actor!");
  }

  return new CacheOpParent(Manager(), mCacheId, aOpArgs);
}

bool CacheParent::DeallocPCacheOpParent(PCacheOpParent* aActor) {
  delete aActor;
  return true;
}

mozilla::ipc::IPCResult CacheParent::RecvPCacheOpConstructor(
    PCacheOpParent* aActor, const CacheOpArgs& aOpArgs) {
  auto actor = static_cast<CacheOpParent*>(aActor);
  actor->Execute(mManager.clonePtr());
  return IPC_OK();
}

mozilla::ipc::IPCResult CacheParent::RecvTeardown() {
  if (!Send__delete__(this)) {
    // child process is gone, warn and allow actor to clean up normally
    NS_WARNING("Cache failed to send delete.");
  }
  return IPC_OK();
}

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
