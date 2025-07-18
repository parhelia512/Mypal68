/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/Architecture-arm64.h"

#include <cstring>

#include "jit/arm64/vixl/Cpu-vixl.h"
#include "jit/FlushICache.h"  // js::jit::FlushICache
#include "jit/RegisterSets.h"

namespace js {
namespace jit {

Registers::Code Registers::FromName(const char* name) {
  // Check for some register aliases first.
  if (strcmp(name, "ip0") == 0) {
    return ip0;
  }
  if (strcmp(name, "ip1") == 0) {
    return ip1;
  }
  if (strcmp(name, "fp") == 0) {
    return fp;
  }

  for (uint32_t i = 0; i < Total; i++) {
    if (strcmp(GetName(i), name) == 0) {
      return Code(i);
    }
  }

  return Invalid;
}

FloatRegisters::Code FloatRegisters::FromName(const char* name) {
  for (size_t i = 0; i < Total; i++) {
    if (strcmp(GetName(i), name) == 0) {
      return Code(i);
    }
  }

  return Invalid;
}

FloatRegisterSet FloatRegister::ReduceSetForPush(const FloatRegisterSet& s) {
#ifdef ENABLE_WASM_SIMD
#  error "Needs more careful logic if SIMD is enabled"
#endif

  LiveFloatRegisterSet ret;
  for (FloatRegisterIterator iter(s); iter.more(); ++iter) {
    ret.addUnchecked(FromCode((*iter).encoding()));
  }
  return ret.set();
}

uint32_t FloatRegister::GetPushSizeInBytes(const FloatRegisterSet& s) {
#ifdef ENABLE_WASM_SIMD
#  error "Needs more careful logic if SIMD is enabled"
#endif

  return s.size() * sizeof(double);
}

uint32_t FloatRegister::getRegisterDumpOffsetInBytes() {
#ifdef ENABLE_WASM_SIMD
#  error "Needs more careful logic if SIMD is enabled"
#endif

  // Although registers are 128-bits wide, only the first 64 need saving per
  // ABI.
  return encoding() * sizeof(double);
}

uint32_t GetARM64Flags() { return 0; }

void FlushICache(void* code, size_t size) {
  vixl::CPU::EnsureIAndDCacheCoherency(code, size);
}

}  // namespace jit
}  // namespace js
