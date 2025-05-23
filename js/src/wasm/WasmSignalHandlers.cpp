/* Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm/WasmSignalHandlers.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/ThreadLocal.h"

#include "threading/Thread.h"
#include "vm/JitActivation.h"  // js::jit::JitActivation
#include "vm/Realm.h"
#include "vm/Runtime.h"
#include "wasm/WasmInstance.h"

#if defined(XP_WIN)
#  include <winternl.h>  // must include before util/Windows.h's `#undef`s
#  include "util/Windows.h"
#elif defined(XP_DARWIN)
#  include <mach/exc.h>
#  include <mach/mach.h>
#else
#  include <signal.h>
#endif

using namespace js;
using namespace js::wasm;

using mozilla::DebugOnly;

// =============================================================================
// This following pile of macros and includes defines the ToRegisterState() and
// the ContextTo{PC,FP,SP,LR}() functions from the (highly) platform-specific
// CONTEXT struct which is provided to the signal handler.
// =============================================================================

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#  include <sys/ucontext.h>  // for ucontext_t, mcontext_t
#endif

#if defined(__x86_64__)
#  if defined(__DragonFly__)
#    include <machine/npx.h>  // for union savefpu
#  elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || \
      defined(__NetBSD__) || defined(__OpenBSD__)
#    include <machine/fpu.h>  // for struct savefpu/fxsave64
#  endif
#endif

#if defined(XP_WIN)
#  define EIP_sig(p) ((p)->Eip)
#  define EBP_sig(p) ((p)->Ebp)
#  define ESP_sig(p) ((p)->Esp)
#  define RIP_sig(p) ((p)->Rip)
#  define RSP_sig(p) ((p)->Rsp)
#  define RBP_sig(p) ((p)->Rbp)
#  define R11_sig(p) ((p)->R11)
#  define R13_sig(p) ((p)->R13)
#  define R14_sig(p) ((p)->R14)
#  define R15_sig(p) ((p)->R15)
#  define EPC_sig(p) ((p)->Pc)
#  define RFP_sig(p) ((p)->Fp)
#  define R31_sig(p) ((p)->Sp)
#  define RLR_sig(p) ((p)->Lr)
#elif defined(__OpenBSD__)
#  define EIP_sig(p) ((p)->sc_eip)
#  define EBP_sig(p) ((p)->sc_ebp)
#  define ESP_sig(p) ((p)->sc_esp)
#  define RIP_sig(p) ((p)->sc_rip)
#  define RSP_sig(p) ((p)->sc_rsp)
#  define RBP_sig(p) ((p)->sc_rbp)
#  define R11_sig(p) ((p)->sc_r11)
#  if defined(__arm__)
#    define R13_sig(p) ((p)->sc_usr_sp)
#    define R14_sig(p) ((p)->sc_usr_lr)
#    define R15_sig(p) ((p)->sc_pc)
#  else
#    define R13_sig(p) ((p)->sc_r13)
#    define R14_sig(p) ((p)->sc_r14)
#    define R15_sig(p) ((p)->sc_r15)
#  endif
#  if defined(__aarch64__)
#    define EPC_sig(p) ((p)->sc_elr)
#    define RFP_sig(p) ((p)->sc_x[29])
#    define RLR_sig(p) ((p)->sc_lr)
#    define R31_sig(p) ((p)->sc_sp)
#  endif
#  if defined(__mips__)
#    define EPC_sig(p) ((p)->sc_pc)
#    define RFP_sig(p) ((p)->sc_regs[30])
#  endif
#  if defined(__ppc64__) || defined(__PPC64__) || defined(__ppc64le__) || \
      defined(__PPC64LE__)
#    define R01_sig(p) ((p)->sc_frame.fixreg[1])
#    define R32_sig(p) ((p)->sc_frame.srr0)
#  endif
#elif defined(__linux__) || defined(__sun)
#  if defined(__linux__)
#    define EIP_sig(p) ((p)->uc_mcontext.gregs[REG_EIP])
#    define EBP_sig(p) ((p)->uc_mcontext.gregs[REG_EBP])
#    define ESP_sig(p) ((p)->uc_mcontext.gregs[REG_ESP])
#  else
#    define EIP_sig(p) ((p)->uc_mcontext.gregs[REG_PC])
#    define EBP_sig(p) ((p)->uc_mcontext.gregs[REG_EBP])
#    define ESP_sig(p) ((p)->uc_mcontext.gregs[REG_ESP])
#  endif
#  define RIP_sig(p) ((p)->uc_mcontext.gregs[REG_RIP])
#  define RSP_sig(p) ((p)->uc_mcontext.gregs[REG_RSP])
#  define RBP_sig(p) ((p)->uc_mcontext.gregs[REG_RBP])
#  if defined(__linux__) && defined(__arm__)
#    define R11_sig(p) ((p)->uc_mcontext.arm_fp)
#    define R13_sig(p) ((p)->uc_mcontext.arm_sp)
#    define R14_sig(p) ((p)->uc_mcontext.arm_lr)
#    define R15_sig(p) ((p)->uc_mcontext.arm_pc)
#  else
#    define R11_sig(p) ((p)->uc_mcontext.gregs[REG_R11])
#    define R13_sig(p) ((p)->uc_mcontext.gregs[REG_R13])
#    define R14_sig(p) ((p)->uc_mcontext.gregs[REG_R14])
#    define R15_sig(p) ((p)->uc_mcontext.gregs[REG_R15])
#  endif
#  if defined(__linux__) && defined(__aarch64__)
#    define EPC_sig(p) ((p)->uc_mcontext.pc)
#    define RFP_sig(p) ((p)->uc_mcontext.regs[29])
#    define RLR_sig(p) ((p)->uc_mcontext.regs[30])
#    define R31_sig(p) ((p)->uc_mcontext.regs[31])
#  endif
#  if defined(__linux__) && defined(__mips__)
#    define EPC_sig(p) ((p)->uc_mcontext.pc)
#    define RFP_sig(p) ((p)->uc_mcontext.gregs[30])
#    define RSP_sig(p) ((p)->uc_mcontext.gregs[29])
#    define R31_sig(p) ((p)->uc_mcontext.gregs[31])
#  endif
#  if defined(__linux__) && (defined(__sparc__) && defined(__arch64__))
#    define PC_sig(p) ((p)->uc_mcontext.mc_gregs[MC_PC])
#    define FP_sig(p) ((p)->uc_mcontext.mc_fp)
#    define SP_sig(p) ((p)->uc_mcontext.mc_i7)
#  endif
#  if defined(__linux__) && (defined(__ppc64__) || defined(__PPC64__) || \
                             defined(__ppc64le__) || defined(__PPC64LE__))
#    define R01_sig(p) ((p)->uc_mcontext.gp_regs[1])
#    define R32_sig(p) ((p)->uc_mcontext.gp_regs[32])
#  endif
#elif defined(__NetBSD__)
#  define EIP_sig(p) ((p)->uc_mcontext.__gregs[_REG_EIP])
#  define EBP_sig(p) ((p)->uc_mcontext.__gregs[_REG_EBP])
#  define ESP_sig(p) ((p)->uc_mcontext.__gregs[_REG_ESP])
#  define RIP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RIP])
#  define RSP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RSP])
#  define RBP_sig(p) ((p)->uc_mcontext.__gregs[_REG_RBP])
#  define R11_sig(p) ((p)->uc_mcontext.__gregs[_REG_R11])
#  define R13_sig(p) ((p)->uc_mcontext.__gregs[_REG_R13])
#  define R14_sig(p) ((p)->uc_mcontext.__gregs[_REG_R14])
#  define R15_sig(p) ((p)->uc_mcontext.__gregs[_REG_R15])
#  if defined(__aarch64__)
#    define EPC_sig(p) ((p)->uc_mcontext.__gregs[_REG_PC])
#    define RFP_sig(p) ((p)->uc_mcontext.__gregs[_REG_X29])
#    define RLR_sig(p) ((p)->uc_mcontext.__gregs[_REG_X30])
#    define R31_sig(p) ((p)->uc_mcontext.__gregs[_REG_SP])
#  endif
#  if defined(__mips__)
#    define EPC_sig(p) ((p)->uc_mcontext.__gregs[_REG_EPC])
#    define RFP_sig(p) ((p)->uc_mcontext.__gregs[_REG_S8])
#  endif
#  if defined(__ppc64__) || defined(__PPC64__) || defined(__ppc64le__) || \
      defined(__PPC64LE__)
#    define R01_sig(p) ((p)->uc_mcontext.__gregs[_REG_R1])
#    define R32_sig(p) ((p)->uc_mcontext.__gregs[_REG_PC])
#  endif
#elif defined(__DragonFly__) || defined(__FreeBSD__) || \
    defined(__FreeBSD_kernel__)
#  define EIP_sig(p) ((p)->uc_mcontext.mc_eip)
#  define EBP_sig(p) ((p)->uc_mcontext.mc_ebp)
#  define ESP_sig(p) ((p)->uc_mcontext.mc_esp)
#  define RIP_sig(p) ((p)->uc_mcontext.mc_rip)
#  define RSP_sig(p) ((p)->uc_mcontext.mc_rsp)
#  define RBP_sig(p) ((p)->uc_mcontext.mc_rbp)
#  if defined(__FreeBSD__) && defined(__arm__)
#    define R11_sig(p) ((p)->uc_mcontext.__gregs[_REG_R11])
#    define R13_sig(p) ((p)->uc_mcontext.__gregs[_REG_R13])
#    define R14_sig(p) ((p)->uc_mcontext.__gregs[_REG_R14])
#    define R15_sig(p) ((p)->uc_mcontext.__gregs[_REG_R15])
#  else
#    define R11_sig(p) ((p)->uc_mcontext.mc_r11)
#    define R13_sig(p) ((p)->uc_mcontext.mc_r13)
#    define R14_sig(p) ((p)->uc_mcontext.mc_r14)
#    define R15_sig(p) ((p)->uc_mcontext.mc_r15)
#  endif
#  if defined(__FreeBSD__) && defined(__aarch64__)
#    define EPC_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_elr)
#    define RFP_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_x[29])
#    define RLR_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_lr)
#    define R31_sig(p) ((p)->uc_mcontext.mc_gpregs.gp_sp)
#  endif
#  if defined(__FreeBSD__) && defined(__mips__)
#    define EPC_sig(p) ((p)->uc_mcontext.mc_pc)
#    define RFP_sig(p) ((p)->uc_mcontext.mc_regs[30])
#  endif
#  if defined(__FreeBSD__) && (defined(__ppc64__) || defined(__PPC64__) || \
                               defined(__ppc64le__) || defined(__PPC64LE__))
#    define R01_sig(p) ((p)->uc_mcontext.mc_gpr[1])
#    define R32_sig(p) ((p)->uc_mcontext.mc_srr0)
#  endif
#elif defined(XP_DARWIN)
#  define EIP_sig(p) ((p)->thread.uts.ts32.__eip)
#  define EBP_sig(p) ((p)->thread.uts.ts32.__ebp)
#  define ESP_sig(p) ((p)->thread.uts.ts32.__esp)
#  define RIP_sig(p) ((p)->thread.__rip)
#  define RBP_sig(p) ((p)->thread.__rbp)
#  define RSP_sig(p) ((p)->thread.__rsp)
#  define R11_sig(p) ((p)->thread.__r[11])
#  define R13_sig(p) ((p)->thread.__sp)
#  define R14_sig(p) ((p)->thread.__lr)
#  define R15_sig(p) ((p)->thread.__pc)
#else
#  error "Don't know how to read/write to the thread state via the mcontext_t."
#endif

// On ARM Linux, including Android, unaligned FP accesses that were not flagged
// as unaligned will tend to trap (with SIGBUS) and will need to be emulated.
//
// We can only perform this emulation if the system header files provide access
// to the FP registers.  In particular, <sys/user.h> must have definitions of
// `struct user_vfp` and `struct user_vfp_exc`, as it does on Android.
//
// Those definitions are however not present in the headers of every Linux
// distro - Raspbian is known to be a problem, for example.  However those
// distros are tier-3 platforms.
//
// If you run into compile problems on a tier-3 platform, you can disable the
// emulation here.

#if defined(__linux__) && defined(__arm__)
#  define WASM_EMULATE_ARM_UNALIGNED_FP_ACCESS
#endif

#ifdef WASM_EMULATE_ARM_UNALIGNED_FP_ACCESS
#  include <sys/user.h>
#endif

#if defined(ANDROID)
// Not all versions of the Android NDK define ucontext_t or mcontext_t.
// Detect this and provide custom but compatible definitions. Note that these
// follow the GLibc naming convention to access register values from
// mcontext_t.
//
// See: https://chromiumcodereview.appspot.com/10829122/
// See: http://code.google.com/p/android/issues/detail?id=34784
#  if !defined(__BIONIC_HAVE_UCONTEXT_T)
#    if defined(__arm__)

// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#      if !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#        include <asm/sigcontext.h>
#      endif

typedef struct sigcontext mcontext_t;

typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used so don't define them here.
} ucontext_t;

#    elif defined(__mips__)

typedef struct {
  uint32_t regmask;
  uint32_t status;
  uint64_t pc;
  uint64_t gregs[32];
  uint64_t fpregs[32];
  uint32_t acx;
  uint32_t fpc_csr;
  uint32_t fpc_eir;
  uint32_t used_math;
  uint32_t dsp;
  uint64_t mdhi;
  uint64_t mdlo;
  uint32_t hi1;
  uint32_t lo1;
  uint32_t hi2;
  uint32_t lo2;
  uint32_t hi3;
  uint32_t lo3;
} mcontext_t;

typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used so don't define them here.
} ucontext_t;

#    elif defined(__i386__)
// x86 version for Android.
typedef struct {
  uint32_t gregs[19];
  void* fpregs;
  uint32_t oldmask;
  uint32_t cr2;
} mcontext_t;

typedef uint32_t kernel_sigset_t[2];  // x86 kernel uses 64-bit signal masks
typedef struct ucontext {
  uint32_t uc_flags;
  struct ucontext* uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  // Other fields are not used by V8, don't define them here.
} ucontext_t;
enum { REG_EIP = 14 };
#    endif  // defined(__i386__)
#  endif    // !defined(__BIONIC_HAVE_UCONTEXT_T)
#endif      // defined(ANDROID)

#if defined(XP_DARWIN)
#  if defined(__x86_64__)
struct macos_x64_context {
  x86_thread_state64_t thread;
  x86_float_state64_t float_;
};
#    define CONTEXT macos_x64_context
#  elif defined(__i386__)
struct macos_x86_context {
  x86_thread_state_t thread;
  x86_float_state_t float_;
};
#    define CONTEXT macos_x86_context
#  elif defined(__arm__)
struct macos_arm_context {
  arm_thread_state_t thread;
  arm_neon_state_t float_;
};
#    define CONTEXT macos_arm_context
#  else
#    error Unsupported architecture
#  endif
#elif !defined(XP_WIN)
#  define CONTEXT ucontext_t
#endif

#if defined(_M_X64) || defined(__x86_64__)
#  define PC_sig(p) RIP_sig(p)
#  define FP_sig(p) RBP_sig(p)
#  define SP_sig(p) RSP_sig(p)
#elif defined(_M_IX86) || defined(__i386__)
#  define PC_sig(p) EIP_sig(p)
#  define FP_sig(p) EBP_sig(p)
#  define SP_sig(p) ESP_sig(p)
#elif defined(__arm__)
#  define FP_sig(p) R11_sig(p)
#  define SP_sig(p) R13_sig(p)
#  define LR_sig(p) R14_sig(p)
#  define PC_sig(p) R15_sig(p)
#elif defined(_M_ARM64) || defined(__aarch64__)
#  define PC_sig(p) EPC_sig(p)
#  define FP_sig(p) RFP_sig(p)
#  define SP_sig(p) R31_sig(p)
#  define LR_sig(p) RLR_sig(p)
#elif defined(__mips__)
#  define PC_sig(p) EPC_sig(p)
#  define FP_sig(p) RFP_sig(p)
#  define SP_sig(p) RSP_sig(p)
#  define LR_sig(p) R31_sig(p)
#elif defined(__ppc64__) || defined(__PPC64__) || defined(__ppc64le__) || \
    defined(__PPC64LE__)
#  define PC_sig(p) R32_sig(p)
#  define SP_sig(p) R01_sig(p)
#  define FP_sig(p) R01_sig(p)
#endif

static void SetContextPC(CONTEXT* context, uint8_t* pc) {
#ifdef PC_sig
  *reinterpret_cast<uint8_t**>(&PC_sig(context)) = pc;
#else
  MOZ_CRASH();
#endif
}

static uint8_t* ContextToPC(CONTEXT* context) {
#ifdef PC_sig
  return reinterpret_cast<uint8_t*>(PC_sig(context));
#else
  MOZ_CRASH();
#endif
}

static uint8_t* ContextToFP(CONTEXT* context) {
#ifdef FP_sig
  return reinterpret_cast<uint8_t*>(FP_sig(context));
#else
  MOZ_CRASH();
#endif
}

static uint8_t* ContextToSP(CONTEXT* context) {
#ifdef SP_sig
  return reinterpret_cast<uint8_t*>(SP_sig(context));
#else
  MOZ_CRASH();
#endif
}

#if defined(__arm__) || defined(__aarch64__) || defined(__mips__)
static uint8_t* ContextToLR(CONTEXT* context) {
#  ifdef LR_sig
  return reinterpret_cast<uint8_t*>(LR_sig(context));
#  else
  MOZ_CRASH();
#  endif
}
#endif

static JS::ProfilingFrameIterator::RegisterState ToRegisterState(
    CONTEXT* context) {
  JS::ProfilingFrameIterator::RegisterState state;
  state.fp = ContextToFP(context);
  state.pc = ContextToPC(context);
  state.sp = ContextToSP(context);
#if defined(__arm__) || defined(__aarch64__) || defined(__mips__)
  state.lr = ContextToLR(context);
#else
  state.lr = (void*)UINTPTR_MAX;
#endif
  return state;
}

// =============================================================================
// All signals/exceptions funnel down to this one trap-handling function which
// tests whether the pc is in a wasm module and, if so, whether there is
// actually a trap expected at this pc. These tests both avoid real bugs being
// silently converted to wasm traps and provides the trapping wasm bytecode
// offset we need to report in the error.
//
// Crashing inside wasm trap handling (due to a bug in trap handling or exposed
// during trap handling) must be reported like a normal crash, not cause the
// crash report to be lost. On Windows and non-Mach Unix, a crash during the
// handler reenters the handler, possibly repeatedly until exhausting the stack,
// and so we prevent recursion with the thread-local sAlreadyHandlingTrap. On
// Mach, the wasm exception handler has its own thread and is installed only on
// the thread-level debugging ports of JSRuntime threads, so a crash on
// exception handler thread will not recurse; it will bubble up to the
// process-level debugging ports (where Breakpad is installed).
// =============================================================================

static MOZ_THREAD_LOCAL(bool) sAlreadyHandlingTrap;

struct AutoHandlingTrap {
  AutoHandlingTrap() {
    MOZ_ASSERT(!sAlreadyHandlingTrap.get());
    sAlreadyHandlingTrap.set(true);
  }

  ~AutoHandlingTrap() {
    MOZ_ASSERT(sAlreadyHandlingTrap.get());
    sAlreadyHandlingTrap.set(false);
  }
};

#ifdef WASM_EMULATE_ARM_UNALIGNED_FP_ACCESS

// Code to handle SIGBUS for unaligned floating point accesses on 32-bit ARM.

static uintptr_t ReadGPR(CONTEXT* context, uint32_t rn) {
  switch (rn) {
    case 0:
      return context->uc_mcontext.arm_r0;
    case 1:
      return context->uc_mcontext.arm_r1;
    case 2:
      return context->uc_mcontext.arm_r2;
    case 3:
      return context->uc_mcontext.arm_r3;
    case 4:
      return context->uc_mcontext.arm_r4;
    case 5:
      return context->uc_mcontext.arm_r5;
    case 6:
      return context->uc_mcontext.arm_r6;
    case 7:
      return context->uc_mcontext.arm_r7;
    case 8:
      return context->uc_mcontext.arm_r8;
    case 9:
      return context->uc_mcontext.arm_r9;
    case 10:
      return context->uc_mcontext.arm_r10;
    case 11:
      return context->uc_mcontext.arm_fp;
    case 12:
      return context->uc_mcontext.arm_ip;
    case 13:
      return context->uc_mcontext.arm_sp;
    case 14:
      return context->uc_mcontext.arm_lr;
    case 15:
      return context->uc_mcontext.arm_pc;
    default:
      MOZ_CRASH();
  }
}

// Linux kernel data structures.
//
// The vfp_sigframe is a kernel type overlaid on the uc_regspace field of the
// ucontext_t if the first word of the uc_regspace is VFP_MAGIC.  (user_vfp and
// user_vfp_exc are defined in sys/user.h and are stable.)
//
// VFP_MAGIC appears to have been stable since a commit to Linux on 2010-04-11,
// when it was changed from being 0x56465001 on ARMv6 and earlier and 0x56465002
// on ARMv7 and later, to being 0x56465001 on all CPU versions.  This was in
// Kernel 2.6.34-rc5.
//
// My best interpretation of the Android commit history is that Android has had
// vfp_sigframe and VFP_MAGIC in this form since at least Android 3.4 / 2012;
// Firefox requires Android 4.0 at least and we're probably safe here.

struct vfp_sigframe {
  unsigned long magic;
  unsigned long size;
  struct user_vfp ufp;
  struct user_vfp_exc ufp_exc;
};

#  define VFP_MAGIC 0x56465001

static vfp_sigframe* GetVFPFrame(CONTEXT* context) {
  if (context->uc_regspace[0] != VFP_MAGIC) {
    return nullptr;
  }
  return (vfp_sigframe*)&context->uc_regspace;
}

static bool ReadFPR64(CONTEXT* context, uint32_t vd, double* val) {
  MOZ_ASSERT(vd < 32);
  vfp_sigframe* frame = GetVFPFrame(context);
  if (frame) {
    *val = ((double*)frame->ufp.fpregs)[vd];
    return true;
  }
  return false;
}

static bool WriteFPR64(CONTEXT* context, uint32_t vd, double val) {
  MOZ_ASSERT(vd < 32);
  vfp_sigframe* frame = GetVFPFrame(context);
  if (frame) {
    ((double*)frame->ufp.fpregs)[vd] = val;
    return true;
  }
  return false;
}

static bool ReadFPR32(CONTEXT* context, uint32_t vd, float* val) {
  MOZ_ASSERT(vd < 32);
  vfp_sigframe* frame = GetVFPFrame(context);
  if (frame) {
    *val = ((float*)frame->ufp.fpregs)[vd];
    return true;
  }
  return false;
}

static bool WriteFPR32(CONTEXT* context, uint32_t vd, float val) {
  MOZ_ASSERT(vd < 32);
  vfp_sigframe* frame = GetVFPFrame(context);
  if (frame) {
    ((float*)frame->ufp.fpregs)[vd] = val;
    return true;
  }
  return false;
}

static bool HandleUnalignedTrap(CONTEXT* context, uint8_t* pc,
                                Instance* instance) {
  // ARM only, no Thumb.
  MOZ_RELEASE_ASSERT(uintptr_t(pc) % 4 == 0);

  // wasmLoadImpl() and wasmStoreImpl() in MacroAssembler-arm.cpp emit plain,
  // unconditional VLDR and VSTR instructions that do not use the PC as the base
  // register.
  uint32_t instr = *(uint32_t*)pc;
  uint32_t masked = instr & 0x0F300E00;
  bool isVLDR = masked == 0x0D100A00;
  bool isVSTR = masked == 0x0D000A00;

  if (!isVLDR && !isVSTR) {
    // Three obvious cases if we don't get our expected instructions:
    // - masm is generating other FP access instructions than it should
    // - we're encountering a device that traps on new kinds of accesses,
    //   perhaps unaligned integer accesses
    // - general code generation bugs that lead to SIGBUS
#  ifdef ANDROID
    __android_log_print(ANDROID_LOG_ERROR, "WASM", "Bad SIGBUS instr %08x",
                        instr);
#  endif
#  ifdef DEBUG
    MOZ_CRASH("Unexpected instruction");
#  endif
    return false;
  }

  bool isUnconditional = (instr >> 28) == 0xE;
  bool isDouble = (instr & 0x00000100) != 0;
  bool isAdd = (instr & 0x00800000) != 0;
  uint32_t dBit = (instr >> 22) & 1;
  uint32_t offs = (instr & 0xFF) << 2;
  uint32_t rn = (instr >> 16) & 0xF;

  MOZ_RELEASE_ASSERT(isUnconditional);
  MOZ_RELEASE_ASSERT(rn != 15);

  uint8_t* p = (uint8_t*)ReadGPR(context, rn) + (isAdd ? offs : -offs);

  if (!instance->memoryAccessInBounds(
          p, isDouble ? sizeof(double) : sizeof(float))) {
    return false;
  }

  if (isDouble) {
    uint32_t vd = ((instr >> 12) & 0xF) | (dBit << 4);
    double val;
    if (isVLDR) {
      memcpy(&val, p, sizeof(val));
      if (WriteFPR64(context, vd, val)) {
        SetContextPC(context, pc + 4);
        return true;
      }
    } else {
      if (ReadFPR64(context, vd, &val)) {
        memcpy(p, &val, sizeof(val));
        SetContextPC(context, pc + 4);
        return true;
      }
    }
  } else {
    uint32_t vd = ((instr >> 11) & (0xF << 1)) | dBit;
    float val;
    if (isVLDR) {
      memcpy(&val, p, sizeof(val));
      if (WriteFPR32(context, vd, val)) {
        SetContextPC(context, pc + 4);
        return true;
      }
    } else {
      if (ReadFPR32(context, vd, &val)) {
        memcpy(p, &val, sizeof(val));
        SetContextPC(context, pc + 4);
        return true;
      }
    }
  }

#  ifdef DEBUG
  MOZ_CRASH(
      "SIGBUS handler could not access FP register, incompatible kernel?");
#  endif
  return false;
}
#else   // WASM_EMULATE_ARM_UNALIGNED_FP_ACCESS
static bool HandleUnalignedTrap(CONTEXT* context, uint8_t* pc,
                                Instance* instance) {
  return false;
}
#endif  // WASM_EMULATE_ARM_UNALIGNED_FP_ACCESS

[[nodiscard]] static bool HandleTrap(CONTEXT* context,
                                     bool isUnalignedSignal = false,
                                     JSContext* assertCx = nullptr) {
  MOZ_ASSERT(sAlreadyHandlingTrap.get());

  uint8_t* pc = ContextToPC(context);
  const CodeSegment* codeSegment = LookupCodeSegment(pc);
  if (!codeSegment || !codeSegment->isModule()) {
    return false;
  }

  const ModuleSegment& segment = *codeSegment->asModule();

  Trap trap;
  BytecodeOffset bytecode;
  if (!segment.code().lookupTrap(pc, &trap, &bytecode)) {
    return false;
  }

  // We have a safe, expected wasm trap, so fp is well-defined to be a Frame*.
  // For the first sanity check, the Trap::IndirectCallBadSig special case is
  // due to this trap occurring in the indirect call prologue, while fp points
  // to the caller's Frame which can be in a different Module. In any case,
  // though, the containing JSContext is the same.

  auto* frame = reinterpret_cast<Frame*>(ContextToFP(context));
  Instance* instance = GetNearestEffectiveTls(frame)->instance;
  MOZ_RELEASE_ASSERT(&instance->code() == &segment.code() ||
                     trap == Trap::IndirectCallBadSig);

  if (isUnalignedSignal) {
    if (trap != Trap::OutOfBounds) {
      return false;
    }
    if (HandleUnalignedTrap(context, pc, instance)) {
      return true;
    }
  }

  JSContext* cx =
      instance->realm()->runtimeFromAnyThread()->mainContextFromAnyThread();
  MOZ_RELEASE_ASSERT(!assertCx || cx == assertCx);

  // JitActivation::startWasmTrap() stores enough register state from the
  // point of the trap to allow stack unwinding or resumption, both of which
  // will call finishWasmTrap().
  jit::JitActivation* activation = cx->activation()->asJit();
  activation->startWasmTrap(trap, bytecode.offset(), ToRegisterState(context));
  SetContextPC(context, segment.trapCode());
  return true;
}

// =============================================================================
// The following platform-specific handlers funnel all signals/exceptions into
// the shared HandleTrap() above.
// =============================================================================

#if defined(XP_WIN)
// Obtained empirically from thread_local codegen on x86/x64/arm64.
// Compiled in all user binaries, so should be stable over time.
static const unsigned sThreadLocalArrayPointerIndex = 11;

static LONG WINAPI WasmTrapHandler(LPEXCEPTION_POINTERS exception) {
  // Make sure TLS is initialized before reading sAlreadyHandlingTrap.
  if (!NtCurrentTeb()->Reserved1[sThreadLocalArrayPointerIndex]) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (sAlreadyHandlingTrap.get()) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  AutoHandlingTrap aht;

  EXCEPTION_RECORD* record = exception->ExceptionRecord;
  if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
      record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (!HandleTrap(exception->ContextRecord, false, TlsContext.get())) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  return EXCEPTION_CONTINUE_EXECUTION;
}

#elif defined(XP_DARWIN)
// On OSX we are forced to use the lower-level Mach exception mechanism instead
// of Unix signals because breakpad uses Mach exceptions and would otherwise
// report a crash before wasm gets a chance to handle the exception.

// This definition was generated by mig (the Mach Interface Generator) for the
// routine 'exception_raise' (exc.defs).
#  pragma pack(4)
typedef struct {
  mach_msg_header_t Head;
  /* start of the kernel processed data */
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  /* end of the kernel processed data */
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  int64_t code[2];
} Request__mach_exception_raise_t;
#  pragma pack()

// The full Mach message also includes a trailer.
struct ExceptionRequest {
  Request__mach_exception_raise_t body;
  mach_msg_trailer_t trailer;
};

static bool HandleMachException(const ExceptionRequest& request) {
  // Get the port of the JSContext's thread from the message.
  mach_port_t cxThread = request.body.thread.name;

  // Read out the JSRuntime thread's register state.
  CONTEXT context;
#  if defined(__x86_64__)
  unsigned int thread_state_count = x86_THREAD_STATE64_COUNT;
  unsigned int float_state_count = x86_FLOAT_STATE64_COUNT;
  int thread_state = x86_THREAD_STATE64;
  int float_state = x86_FLOAT_STATE64;
#  elif defined(__i386__)
  unsigned int thread_state_count = x86_THREAD_STATE_COUNT;
  unsigned int float_state_count = x86_FLOAT_STATE_COUNT;
  int thread_state = x86_THREAD_STATE;
  int float_state = x86_FLOAT_STATE;
#  elif defined(__arm__)
  unsigned int thread_state_count = ARM_THREAD_STATE_COUNT;
  unsigned int float_state_count = ARM_NEON_STATE_COUNT;
  int thread_state = ARM_THREAD_STATE;
  int float_state = ARM_NEON_STATE;
#  else
#    error Unsupported architecture
#  endif
  kern_return_t kret;
  kret = thread_get_state(cxThread, thread_state,
                          (thread_state_t)&context.thread, &thread_state_count);
  if (kret != KERN_SUCCESS) {
    return false;
  }
  kret = thread_get_state(cxThread, float_state,
                          (thread_state_t)&context.float_, &float_state_count);
  if (kret != KERN_SUCCESS) {
    return false;
  }

  if (request.body.exception != EXC_BAD_ACCESS &&
      request.body.exception != EXC_BAD_INSTRUCTION) {
    return false;
  }

  {
    AutoNoteSingleThreadedRegion anstr;
    AutoHandlingTrap aht;
    if (!HandleTrap(&context)) {
      return false;
    }
  }

  // Update the thread state with the new pc and register values.
  kret = thread_set_state(cxThread, float_state,
                          (thread_state_t)&context.float_, float_state_count);
  if (kret != KERN_SUCCESS) {
    return false;
  }
  kret = thread_set_state(cxThread, thread_state,
                          (thread_state_t)&context.thread, thread_state_count);
  if (kret != KERN_SUCCESS) {
    return false;
  }

  return true;
}

static mach_port_t sMachDebugPort = MACH_PORT_NULL;

static void MachExceptionHandlerThread() {
  // Taken from mach_exc in /usr/include/mach/mach_exc.defs.
  static const unsigned EXCEPTION_MSG_ID = 2405;

  while (true) {
    ExceptionRequest request;
    kern_return_t kret =
        mach_msg(&request.body.Head, MACH_RCV_MSG, 0, sizeof(request),
                 sMachDebugPort, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    // If we fail even receiving the message, we can't even send a reply!
    // Rather than hanging the faulting thread (hanging the browser), crash.
    if (kret != KERN_SUCCESS) {
      fprintf(stderr, "MachExceptionHandlerThread: mach_msg failed with %d\n",
              (int)kret);
      MOZ_CRASH();
    }

    if (request.body.Head.msgh_id != EXCEPTION_MSG_ID) {
      fprintf(stderr, "Unexpected msg header id %d\n",
              (int)request.body.Head.msgh_bits);
      MOZ_CRASH();
    }

    // Some thread just commited an EXC_BAD_ACCESS and has been suspended by
    // the kernel. The kernel is waiting for us to reply with instructions.
    // Our default is the "not handled" reply (by setting the RetCode field
    // of the reply to KERN_FAILURE) which tells the kernel to continue
    // searching at the process and system level. If this is an asm.js
    // expected exception, we handle it and return KERN_SUCCESS.
    bool handled = HandleMachException(request);
    kern_return_t replyCode = handled ? KERN_SUCCESS : KERN_FAILURE;

    // This magic incantation to send a reply back to the kernel was
    // derived from the exc_server generated by
    // 'mig -v /usr/include/mach/mach_exc.defs'.
    __Reply__exception_raise_t reply;
    reply.Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request.body.Head.msgh_bits), 0);
    reply.Head.msgh_size = sizeof(reply);
    reply.Head.msgh_remote_port = request.body.Head.msgh_remote_port;
    reply.Head.msgh_local_port = MACH_PORT_NULL;
    reply.Head.msgh_id = request.body.Head.msgh_id + 100;
    reply.NDR = NDR_record;
    reply.RetCode = replyCode;
    mach_msg(&reply.Head, MACH_SEND_MSG, sizeof(reply), 0, MACH_PORT_NULL,
             MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  }
}

#else  // If not Windows or Mac, assume Unix

#  ifdef __mips__
static const uint32_t kWasmTrapSignal = SIGFPE;
#  else
static const uint32_t kWasmTrapSignal = SIGILL;
#  endif

static struct sigaction sPrevSEGVHandler;
static struct sigaction sPrevSIGBUSHandler;
static struct sigaction sPrevWasmTrapHandler;

static void WasmTrapHandler(int signum, siginfo_t* info, void* context) {
  if (!sAlreadyHandlingTrap.get()) {
    AutoHandlingTrap aht;
    MOZ_RELEASE_ASSERT(signum == SIGSEGV || signum == SIGBUS ||
                       signum == kWasmTrapSignal);
    if (HandleTrap((CONTEXT*)context, signum == SIGBUS, TlsContext.get())) {
      return;
    }
  }

  struct sigaction* previousSignal = nullptr;
  switch (signum) {
    case SIGSEGV:
      previousSignal = &sPrevSEGVHandler;
      break;
    case SIGBUS:
      previousSignal = &sPrevSIGBUSHandler;
      break;
    case kWasmTrapSignal:
      previousSignal = &sPrevWasmTrapHandler;
      break;
  }
  MOZ_ASSERT(previousSignal);

  // This signal is not for any asm.js code we expect, so we need to forward
  // the signal to the next handler. If there is no next handler (SIG_IGN or
  // SIG_DFL), then it's time to crash. To do this, we set the signal back to
  // its original disposition and return. This will cause the faulting op to
  // be re-executed which will crash in the normal way. The advantage of
  // doing this to calling _exit() is that we remove ourselves from the crash
  // stack which improves crash reports. If there is a next handler, call it.
  // It will either crash synchronously, fix up the instruction so that
  // execution can continue and return, or trigger a crash by returning the
  // signal to it's original disposition and returning.
  //
  // Note: the order of these tests matter.
  if (previousSignal->sa_flags & SA_SIGINFO) {
    previousSignal->sa_sigaction(signum, info, context);
  } else if (previousSignal->sa_handler == SIG_DFL ||
             previousSignal->sa_handler == SIG_IGN) {
    sigaction(signum, previousSignal, nullptr);
  } else {
    previousSignal->sa_handler(signum);
  }
}
#endif  // XP_WIN || XP_DARWIN || assume unix

#if defined(ANDROID) && defined(MOZ_LINKER)
extern "C" MFBT_API bool IsSignalHandlingBroken();
#endif

struct InstallState {
  bool tried;
  bool success;
  InstallState() : tried(false), success(false) {}
};

static ExclusiveData<InstallState> sEagerInstallState(
    mutexid::WasmSignalInstallState);

void wasm::EnsureEagerProcessSignalHandlers() {
  auto eagerInstallState = sEagerInstallState.lock();
  if (eagerInstallState->tried) {
    return;
  }

  eagerInstallState->tried = true;
  MOZ_RELEASE_ASSERT(eagerInstallState->success == false);

#if defined(JS_CODEGEN_NONE)
  // If there is no JIT, then there should be no Wasm signal handlers.
  return;
#endif

#if defined(ANDROID) && defined(MOZ_LINKER)
  // Signal handling is broken on some android systems.
  if (IsSignalHandlingBroken()) {
    return;
  }
#endif

  sAlreadyHandlingTrap.infallibleInit();

  // Install whatever exception/signal handler is appropriate for the OS.
#if defined(XP_WIN)

#  if defined(MOZ_ASAN)
  // Under ASan we need to let the ASan runtime's ShadowExceptionHandler stay
  // in the first handler position. This requires some coordination with
  // MemoryProtectionExceptionHandler::isDisabled().
  const bool firstHandler = false;
#  else
  // Otherwise, WasmTrapHandler needs to go first, so that we can recover
  // from wasm faults and continue execution without triggering handlers
  // such as MemoryProtectionExceptionHandler that assume we are crashing.
  const bool firstHandler = true;
#  endif
  if (!AddVectoredExceptionHandler(firstHandler, WasmTrapHandler)) {
    // Windows has all sorts of random security knobs for disabling things
    // so make this a dynamic failure that disables wasm, not a MOZ_CRASH().
    return;
  }

#elif defined(XP_DARWIN)
  // All the Mach setup in EnsureLazyProcessSignalHandlers.
#else
  // SA_NODEFER allows us to reenter the signal handler if we crash while
  // handling the signal, and fall through to the Breakpad handler by testing
  // handlingSegFault.

  // Allow handling OOB with signals on all architectures
  struct sigaction faultHandler;
  faultHandler.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
  faultHandler.sa_sigaction = WasmTrapHandler;
  sigemptyset(&faultHandler.sa_mask);
  if (sigaction(SIGSEGV, &faultHandler, &sPrevSEGVHandler)) {
    MOZ_CRASH("unable to install segv handler");
  }

#  if defined(JS_CODEGEN_ARM)
  // On Arm Handle Unaligned Accesses
  struct sigaction busHandler;
  busHandler.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
  busHandler.sa_sigaction = WasmTrapHandler;
  sigemptyset(&busHandler.sa_mask);
  if (sigaction(SIGBUS, &busHandler, &sPrevSIGBUSHandler)) {
    MOZ_CRASH("unable to install sigbus handler");
  }
#  endif

  // Install a handler to handle the instructions that are emitted to implement
  // wasm traps.
  struct sigaction wasmTrapHandler;
  wasmTrapHandler.sa_flags = SA_SIGINFO | SA_NODEFER | SA_ONSTACK;
  wasmTrapHandler.sa_sigaction = WasmTrapHandler;
  sigemptyset(&wasmTrapHandler.sa_mask);
  if (sigaction(kWasmTrapSignal, &wasmTrapHandler, &sPrevWasmTrapHandler)) {
    MOZ_CRASH("unable to install wasm trap handler");
  }
#endif

  eagerInstallState->success = true;
}

static ExclusiveData<InstallState> sLazyInstallState(
    mutexid::WasmSignalInstallState);

static bool EnsureLazyProcessSignalHandlers() {
  auto lazyInstallState = sLazyInstallState.lock();
  if (lazyInstallState->tried) {
    return lazyInstallState->success;
  }

  lazyInstallState->tried = true;
  MOZ_RELEASE_ASSERT(lazyInstallState->success == false);

#ifdef XP_DARWIN
  // Create the port that all JSContext threads will redirect their traps to.
  kern_return_t kret;
  kret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                            &sMachDebugPort);
  if (kret != KERN_SUCCESS) {
    return false;
  }
  kret = mach_port_insert_right(mach_task_self(), sMachDebugPort,
                                sMachDebugPort, MACH_MSG_TYPE_MAKE_SEND);
  if (kret != KERN_SUCCESS) {
    return false;
  }

  // Create the thread that will wait on and service sMachDebugPort.
  // It's not useful to destroy this thread on process shutdown so
  // immediately detach on successful start.
  Thread handlerThread;
  if (!handlerThread.init(MachExceptionHandlerThread)) {
    return false;
  }
  handlerThread.detach();
#endif

  lazyInstallState->success = true;
  return true;
}

bool wasm::EnsureFullSignalHandlers(JSContext* cx) {
  if (cx->wasm().triedToInstallSignalHandlers) {
    return cx->wasm().haveSignalHandlers;
  }

  cx->wasm().triedToInstallSignalHandlers = true;
  MOZ_RELEASE_ASSERT(!cx->wasm().haveSignalHandlers);

  {
    auto eagerInstallState = sEagerInstallState.lock();
    MOZ_RELEASE_ASSERT(eagerInstallState->tried);
    if (!eagerInstallState->success) {
      return false;
    }
  }

  if (!EnsureLazyProcessSignalHandlers()) {
    return false;
  }

#ifdef XP_DARWIN
  // In addition to the process-wide signal handler setup, OSX needs each
  // thread configured to send its exceptions to sMachDebugPort. While there
  // are also task-level (i.e. process-level) exception ports, those are
  // "claimed" by breakpad and chaining Mach exceptions is dark magic that we
  // avoid by instead intercepting exceptions at the thread level before they
  // propagate to the process-level. This works because there are no other
  // uses of thread-level exception ports.
  MOZ_RELEASE_ASSERT(sMachDebugPort != MACH_PORT_NULL);
  thread_port_t thisThread = mach_thread_self();
  kern_return_t kret = thread_set_exception_ports(
      thisThread, EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION,
      sMachDebugPort, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
      THREAD_STATE_NONE);
  mach_port_deallocate(mach_task_self(), thisThread);
  if (kret != KERN_SUCCESS) {
    return false;
  }
#endif

  cx->wasm().haveSignalHandlers = true;
  return true;
}

bool wasm::MemoryAccessTraps(const RegisterState& regs, uint8_t* addr,
                             uint32_t numBytes, uint8_t** newPC) {
  const wasm::CodeSegment* codeSegment = wasm::LookupCodeSegment(regs.pc);
  if (!codeSegment || !codeSegment->isModule()) {
    return false;
  }

  const wasm::ModuleSegment& segment = *codeSegment->asModule();

  Trap trap;
  BytecodeOffset bytecode;
  if (!segment.code().lookupTrap(regs.pc, &trap, &bytecode) ||
      trap != Trap::OutOfBounds) {
    return false;
  }

  Instance& instance =
      *GetNearestEffectiveTls(Frame::fromUntaggedWasmExitFP(regs.fp))->instance;
  MOZ_ASSERT(&instance.code() == &segment.code());

  if (!instance.memoryAccessInGuardRegion((uint8_t*)addr, numBytes)) {
    return false;
  }

  jit::JitActivation* activation = TlsContext.get()->activation()->asJit();
  activation->startWasmTrap(Trap::OutOfBounds, bytecode.offset(), regs);
  *newPC = segment.trapCode();
  return true;
}

bool wasm::HandleIllegalInstruction(const RegisterState& regs,
                                    uint8_t** newPC) {
  const wasm::CodeSegment* codeSegment = wasm::LookupCodeSegment(regs.pc);
  if (!codeSegment || !codeSegment->isModule()) {
    return false;
  }

  const wasm::ModuleSegment& segment = *codeSegment->asModule();

  Trap trap;
  BytecodeOffset bytecode;
  if (!segment.code().lookupTrap(regs.pc, &trap, &bytecode)) {
    return false;
  }

  jit::JitActivation* activation = TlsContext.get()->activation()->asJit();
  activation->startWasmTrap(trap, bytecode.offset(), regs);
  *newPC = segment.trapCode();
  return true;
}

#undef WASM_EMULATE_ARM_UNALIGNED_FP_ACCESS
