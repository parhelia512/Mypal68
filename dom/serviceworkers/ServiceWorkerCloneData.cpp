/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerCloneData.h"

#include <utility>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "nsISerialEventTarget.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

ServiceWorkerCloneData::~ServiceWorkerCloneData() {
  RefPtr<ipc::SharedJSAllocatedData> sharedData = TakeSharedData();
  if (sharedData) {
    NS_ProxyRelease(__func__, mEventTarget, sharedData.forget());
  }
}

ServiceWorkerCloneData::ServiceWorkerCloneData()
    : mEventTarget(GetCurrentSerialEventTarget()) {
  MOZ_DIAGNOSTIC_ASSERT(mEventTarget);
}

}  // namespace dom
}  // namespace mozilla
