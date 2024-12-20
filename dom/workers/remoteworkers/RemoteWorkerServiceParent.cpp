/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerServiceParent.h"
#include "RemoteWorkerManager.h"

namespace mozilla {

using namespace ipc;

namespace dom {

RemoteWorkerServiceParent::RemoteWorkerServiceParent()
    : mManager(RemoteWorkerManager::GetOrCreate()) {}

RemoteWorkerServiceParent::~RemoteWorkerServiceParent() = default;

void RemoteWorkerServiceParent::Initialize() {
  AssertIsOnBackgroundThread();
  mManager->RegisterActor(this);
}

void RemoteWorkerServiceParent::ActorDestroy(IProtocol::ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  mManager->UnregisterActor(this);
}

}  // namespace dom
}  // namespace mozilla
