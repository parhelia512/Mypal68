/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Bailouts.h"
#include "jit/BaselineIC.h"
#include "jit/JitRuntime.h"
#include "vm/Realm.h"

using namespace js;
using namespace js::jit;

// This file includes stubs for generating the JIT trampolines when there is no
// JIT backend, and also includes implementations for assorted random things
// which can't be implemented in headers.

void JitRuntime::generateEnterJIT(JSContext*, MacroAssembler&) { MOZ_CRASH(); }
// static
mozilla::Maybe<::JS::ProfilingFrameIterator::RegisterState>
JitRuntime::getCppEntryRegisters(JitFrameLayout* frameStackAddress) {
  return mozilla::Nothing{};
}
void JitRuntime::generateInvalidator(MacroAssembler&, Label*) { MOZ_CRASH(); }
void JitRuntime::generateArgumentsRectifier(MacroAssembler&,
                                            ArgumentsRectifierKind kind) {
  MOZ_CRASH();
}
JitRuntime::BailoutTable JitRuntime::generateBailoutTable(MacroAssembler&,
                                                          Label*, uint32_t) {
  MOZ_CRASH();
}
void JitRuntime::generateBailoutHandler(MacroAssembler&, Label*) {
  MOZ_CRASH();
}
uint32_t JitRuntime::generatePreBarrier(JSContext*, MacroAssembler&, MIRType) {
  MOZ_CRASH();
}
void JitRuntime::generateExceptionTailStub(MacroAssembler&, Label*) {
  MOZ_CRASH();
}
void JitRuntime::generateBailoutTailStub(MacroAssembler&, Label*) {
  MOZ_CRASH();
}
void JitRuntime::generateProfilerExitFrameTailStub(MacroAssembler&, Label*) {
  MOZ_CRASH();
}

bool JitRuntime::generateVMWrapper(JSContext*, MacroAssembler&,
                                   const VMFunctionData&, DynFn, uint32_t*) {
  MOZ_CRASH();
}

FrameSizeClass FrameSizeClass::FromDepth(uint32_t) { MOZ_CRASH(); }
FrameSizeClass FrameSizeClass::ClassLimit() { MOZ_CRASH(); }
uint32_t FrameSizeClass::frameSize() const { MOZ_CRASH(); }

BailoutFrameInfo::BailoutFrameInfo(const JitActivationIterator& iter,
                                   BailoutStack* bailout) {
  MOZ_CRASH();
}

BailoutFrameInfo::BailoutFrameInfo(const JitActivationIterator& iter,
                                   InvalidationBailoutStack* bailout) {
  MOZ_CRASH();
}
