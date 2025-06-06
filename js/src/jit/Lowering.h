/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Lowering_h
#define jit_Lowering_h

// This file declares the structures that are used for attaching LIR to a
// MIRGraph.

#include "jit/LIR.h"
#if defined(JS_CODEGEN_X86)
#  include "jit/x86/Lowering-x86.h"
#elif defined(JS_CODEGEN_X64)
#  include "jit/x64/Lowering-x64.h"
#elif defined(JS_CODEGEN_ARM)
#  include "jit/arm/Lowering-arm.h"
#elif defined(JS_CODEGEN_ARM64)
#  include "jit/arm64/Lowering-arm64.h"
#elif defined(JS_CODEGEN_MIPS32)
#  include "jit/mips32/Lowering-mips32.h"
#elif defined(JS_CODEGEN_MIPS64)
#  include "jit/mips64/Lowering-mips64.h"
#elif defined(JS_CODEGEN_NONE)
#  include "jit/none/Lowering-none.h"
#else
#  error "Unknown architecture!"
#endif

namespace js {
namespace jit {

class LIRGenerator final : public LIRGeneratorSpecific {
  void updateResumeState(MInstruction* ins);
  void updateResumeState(MBasicBlock* block);

  // The maximum depth, for framesizeclass determination.
  uint32_t maxargslots_;

 public:
  LIRGenerator(MIRGenerator* gen, MIRGraph& graph, LIRGraph& lirGraph)
      : LIRGeneratorSpecific(gen, graph, lirGraph), maxargslots_(0) {}

  [[nodiscard]] bool generate();

 private:
  LBoxAllocation useBoxFixedAtStart(MDefinition* mir, Register reg1,
                                    Register reg2) {
    return useBoxFixed(mir, reg1, reg2, /* useAtStart = */ true);
  }

  LBoxAllocation useBoxFixedAtStart(MDefinition* mir, ValueOperand op);
  LBoxAllocation useBoxAtStart(MDefinition* mir,
                               LUse::Policy policy = LUse::REGISTER);

  void lowerBitOp(JSOp op, MBinaryBitwiseInstruction* ins);
  void lowerShiftOp(JSOp op, MShiftInstruction* ins);
  void definePhis();

  [[nodiscard]] bool lowerCallArguments(MCall* call);

  friend class LIRGeneratorShared;
  void visitInstructionDispatch(MInstruction* ins);

  void visitReturnImpl(MDefinition* def, bool isGenerator = false);

  [[nodiscard]] bool visitInstruction(MInstruction* ins);
  [[nodiscard]] bool visitBlock(MBasicBlock* block);

#define MIR_OP(op) void visit##op(M##op* ins);
  MIR_OPCODE_LIST(MIR_OP)
#undef MIR_OP
};

}  // namespace jit
}  // namespace js

#endif /* jit_Lowering_h */
