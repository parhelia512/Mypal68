/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitScript_inl_h
#define jit_JitScript_inl_h

#include "jit/JitScript.h"

#include "mozilla/BinarySearch.h"

#include "jit/JitZone.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSScript.h"

#include "vm/JSContext-inl.h"

namespace js {
namespace jit {

inline AutoKeepJitScripts::AutoKeepJitScripts(JSContext* cx)
    : zone_(cx->zone()->jitZone()), prev_(zone_->keepJitScripts()) {
  zone_->setKeepJitScripts(true);
}

inline AutoKeepJitScripts::~AutoKeepJitScripts() {
  MOZ_ASSERT(zone_->keepJitScripts());
  zone_->setKeepJitScripts(prev_);
}

}  // namespace jit
}  // namespace js

inline bool JSScript::ensureHasJitScript(JSContext* cx,
                                         js::jit::AutoKeepJitScripts&) {
  if (hasJitScript()) {
    return true;
  }
  return createJitScript(cx);
}

#endif /* jit_JitScript_inl_h */
