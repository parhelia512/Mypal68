/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyedStackCapturer.h"

#include "jsapi.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "mozilla/StackWalk.h"
#include "nsPrintfCString.h"
#include "ProcessedStack.h"

namespace mozilla {
namespace Telemetry {

void KeyedStackCapturer::Capture(const nsACString& aKey) {
}

NS_IMETHODIMP
KeyedStackCapturer::ReflectCapturedStacks(JSContext* cx,
                                          JS::MutableHandle<JS::Value> ret) {
  return NS_OK;
}

void KeyedStackCapturer::Clear() {
}

size_t KeyedStackCapturer::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  n += mStackInfos.SizeOfExcludingThis(aMallocSizeOf);
  n += mStacks.SizeOfExcludingThis();
  return n;
}

}  // namespace Telemetry
}  // namespace mozilla
