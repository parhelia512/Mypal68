/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/mips-shared/Lowering-mips-shared.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;

LAllocation LIRGeneratorMIPSShared::useByteOpRegister(MDefinition* mir) {
  return useRegister(mir);
}

LAllocation LIRGeneratorMIPSShared::useByteOpRegisterAtStart(MDefinition* mir) {
  return useRegisterAtStart(mir);
}

LAllocation LIRGeneratorMIPSShared::useByteOpRegisterOrNonDoubleConstant(
    MDefinition* mir) {
  return useRegisterOrNonDoubleConstant(mir);
}

LDefinition LIRGeneratorMIPSShared::tempByteOpRegister() { return temp(); }

// x = !y
void LIRGeneratorMIPSShared::lowerForALU(LInstructionHelper<1, 1, 0>* ins,
                                         MDefinition* mir, MDefinition* input) {
  ins->setOperand(0, useRegister(input));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

// z = x+y
void LIRGeneratorMIPSShared::lowerForALU(LInstructionHelper<1, 2, 0>* ins,
                                         MDefinition* mir, MDefinition* lhs,
                                         MDefinition* rhs) {
  ins->setOperand(0, useRegister(lhs));
  ins->setOperand(1, useRegisterOrConstant(rhs));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

void LIRGeneratorMIPSShared::lowerForALUInt64(
    LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  ins->setInt64Operand(INT64_PIECES, lhs != rhs
                                         ? useInt64OrConstant(rhs)
                                         : useInt64OrConstantAtStart(rhs));
  defineInt64ReuseInput(ins, mir, 0);
}

void LIRGeneratorMIPSShared::lowerForMulInt64(LMulI64* ins, MMul* mir,
                                              MDefinition* lhs,
                                              MDefinition* rhs) {
  bool needsTemp = false;
  bool cannotAliasRhs = false;
  bool reuseInput = true;

#ifdef JS_CODEGEN_MIPS32
  needsTemp = true;
  cannotAliasRhs = true;
  if (rhs->isConstant()) {
    int64_t constant = rhs->toConstant()->toInt64();
    int32_t shift = mozilla::FloorLog2(constant);
    // See special cases in CodeGeneratorMIPSShared::visitMulI64
    if (constant >= -1 && constant <= 2) {
      needsTemp = false;
    }
    if (int64_t(1) << shift == constant) {
      needsTemp = false;
    }
    if (mozilla::IsPowerOfTwo(static_cast<uint32_t>(constant + 1)) ||
        mozilla::IsPowerOfTwo(static_cast<uint32_t>(constant - 1)))
      reuseInput = false;
  }
#endif
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  ins->setInt64Operand(INT64_PIECES, (lhs != rhs || cannotAliasRhs)
                                         ? useInt64OrConstant(rhs)
                                         : useInt64OrConstantAtStart(rhs));

  if (needsTemp) {
    ins->setTemp(0, temp());
  }
  if (reuseInput) {
    defineInt64ReuseInput(ins, mir, 0);
  } else {
    defineInt64(ins, mir);
  }
}

template <size_t Temps>
void LIRGeneratorMIPSShared::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs) {
#ifdef JS_CODEGEN_MIPS32
  if (mir->isRotate()) {
    if (!rhs->isConstant()) {
      ins->setTemp(0, temp());
    }
    ins->setInt64Operand(0, useInt64Register(lhs));
  } else {
    ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
  }
#else
  ins->setInt64Operand(0, useInt64RegisterAtStart(lhs));
#endif

  static_assert(LShiftI64::Rhs == INT64_PIECES,
                "Assume Rhs is located at INT64_PIECES.");
  static_assert(LRotateI64::Count == INT64_PIECES,
                "Assume Count is located at INT64_PIECES.");

  ins->setOperand(INT64_PIECES, useRegisterOrConstant(rhs));

#ifdef JS_CODEGEN_MIPS32
  if (mir->isRotate()) {
    defineInt64(ins, mir);
  } else {
    defineInt64ReuseInput(ins, mir, 0);
  }
#else
  defineInt64ReuseInput(ins, mir, 0);
#endif
}

template void LIRGeneratorMIPSShared::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 0>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorMIPSShared::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, 1>* ins,
    MDefinition* mir, MDefinition* lhs, MDefinition* rhs);

void LIRGeneratorMIPSShared::lowerForCompareI64AndBranch(
    MTest* mir, MCompare* comp, JSOp op, MDefinition* left, MDefinition* right,
    MBasicBlock* ifTrue, MBasicBlock* ifFalse) {
  LCompareI64AndBranch* lir = new (alloc())
      LCompareI64AndBranch(comp, op, useInt64Register(left),
                           useInt64OrConstant(right), ifTrue, ifFalse);
  add(lir, mir);
}

void LIRGeneratorMIPSShared::lowerForFPU(LInstructionHelper<1, 1, 0>* ins,
                                         MDefinition* mir, MDefinition* input) {
  ins->setOperand(0, useRegister(input));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

template <size_t Temps>
void LIRGeneratorMIPSShared::lowerForFPU(LInstructionHelper<1, 2, Temps>* ins,
                                         MDefinition* mir, MDefinition* lhs,
                                         MDefinition* rhs) {
  ins->setOperand(0, useRegister(lhs));
  ins->setOperand(1, useRegister(rhs));
  define(
      ins, mir,
      LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

template void LIRGeneratorMIPSShared::lowerForFPU(
    LInstructionHelper<1, 2, 0>* ins, MDefinition* mir, MDefinition* lhs,
    MDefinition* rhs);
template void LIRGeneratorMIPSShared::lowerForFPU(
    LInstructionHelper<1, 2, 1>* ins, MDefinition* mir, MDefinition* lhs,
    MDefinition* rhs);

void LIRGeneratorMIPSShared::lowerForBitAndAndBranch(LBitAndAndBranch* baab,
                                                     MInstruction* mir,
                                                     MDefinition* lhs,
                                                     MDefinition* rhs) {
  baab->setOperand(0, useRegisterAtStart(lhs));
  baab->setOperand(1, useRegisterOrConstantAtStart(rhs));
  add(baab, mir);
}

void LIRGeneratorMIPSShared::lowerWasmBuiltinTruncateToInt32(
    MWasmBuiltinTruncateToInt32* ins) {
  MDefinition* opd = ins->input();
  MOZ_ASSERT(opd->type() == MIRType::Double || opd->type() == MIRType::Float32);

  if (opd->type() == MIRType::Double) {
    define(new (alloc()) LWasmBuiltinTruncateDToInt32(
               useRegister(opd), useFixed(ins->tls(), WasmTlsReg),
               LDefinition::BogusTemp()),
           ins);
    return;
  }

  define(new (alloc()) LWasmBuiltinTruncateFToInt32(
             useRegister(opd), useFixed(ins->tls(), WasmTlsReg),
             LDefinition::BogusTemp()),
         ins);
}

void LIRGeneratorMIPSShared::lowerForShift(LInstructionHelper<1, 2, 0>* ins,
                                           MDefinition* mir, MDefinition* lhs,
                                           MDefinition* rhs) {
  ins->setOperand(0, useRegister(lhs));
  ins->setOperand(1, useRegisterOrConstant(rhs));
  define(ins, mir);
}

void LIRGeneratorMIPSShared::lowerDivI(MDiv* div) {
  if (div->isUnsigned()) {
    lowerUDiv(div);
    return;
  }

  // Division instructions are slow. Division by constant denominators can be
  // rewritten to use other instructions.
  if (div->rhs()->isConstant()) {
    int32_t rhs = div->rhs()->toConstant()->toInt32();
    // Check for division by a positive power of two, which is an easy and
    // important case to optimize. Note that other optimizations are also
    // possible; division by negative powers of two can be optimized in a
    // similar manner as positive powers of two, and division by other
    // constants can be optimized by a reciprocal multiplication technique.
    int32_t shift = FloorLog2(rhs);
    if (rhs > 0 && 1 << shift == rhs) {
      LDivPowTwoI* lir =
          new (alloc()) LDivPowTwoI(useRegister(div->lhs()), shift, temp());
      if (div->fallible()) {
        assignSnapshot(lir, div->bailoutKind());
      }
      define(lir, div);
      return;
    }
  }

  LDivI* lir = new (alloc())
      LDivI(useRegister(div->lhs()), useRegister(div->rhs()), temp());
  if (div->fallible()) {
    assignSnapshot(lir, div->bailoutKind());
  }
  define(lir, div);
}

void LIRGeneratorMIPSShared::lowerNegI(MInstruction* ins, MDefinition* input,
                                       int32_t inputNo) {
  define(new (alloc()) LNegI(useRegisterAtStart(input)), ins);
}

void LIRGeneratorMIPSShared::lowerMulI(MMul* mul, MDefinition* lhs,
                                       MDefinition* rhs) {
  LMulI* lir = new (alloc()) LMulI;
  if (mul->fallible()) {
    assignSnapshot(lir, mul->bailoutKind());
  }

  lowerForALU(lir, mul, lhs, rhs);
}

void LIRGeneratorMIPSShared::lowerModI(MMod* mod) {
  if (mod->isUnsigned()) {
    lowerUMod(mod);
    return;
  }

  if (mod->rhs()->isConstant()) {
    int32_t rhs = mod->rhs()->toConstant()->toInt32();
    int32_t shift = FloorLog2(rhs);
    if (rhs > 0 && 1 << shift == rhs) {
      LModPowTwoI* lir =
          new (alloc()) LModPowTwoI(useRegister(mod->lhs()), shift);
      if (mod->fallible()) {
        assignSnapshot(lir, mod->bailoutKind());
      }
      define(lir, mod);
      return;
    } else if (shift < 31 && (1 << (shift + 1)) - 1 == rhs) {
      LModMaskI* lir = new (alloc())
          LModMaskI(useRegister(mod->lhs()), temp(LDefinition::GENERAL),
                    temp(LDefinition::GENERAL), shift + 1);
      if (mod->fallible()) {
        assignSnapshot(lir, mod->bailoutKind());
      }
      define(lir, mod);
      return;
    }
  }
  LModI* lir =
      new (alloc()) LModI(useRegister(mod->lhs()), useRegister(mod->rhs()),
                          temp(LDefinition::GENERAL));

  if (mod->fallible()) {
    assignSnapshot(lir, mod->bailoutKind());
  }
  define(lir, mod);
}

void LIRGenerator::visitPowHalf(MPowHalf* ins) {
  MDefinition* input = ins->input();
  MOZ_ASSERT(input->type() == MIRType::Double);
  LPowHalfD* lir = new (alloc()) LPowHalfD(useRegisterAtStart(input));
  defineReuseInput(lir, ins, 0);
}

LTableSwitch* LIRGeneratorMIPSShared::newLTableSwitch(
    const LAllocation& in, const LDefinition& inputCopy,
    MTableSwitch* tableswitch) {
  return new (alloc()) LTableSwitch(in, inputCopy, temp(), tableswitch);
}

LTableSwitchV* LIRGeneratorMIPSShared::newLTableSwitchV(
    MTableSwitch* tableswitch) {
  return new (alloc()) LTableSwitchV(useBox(tableswitch->getOperand(0)), temp(),
                                     tempDouble(), temp(), tableswitch);
}

void LIRGeneratorMIPSShared::lowerUrshD(MUrsh* mir) {
  MDefinition* lhs = mir->lhs();
  MDefinition* rhs = mir->rhs();

  MOZ_ASSERT(lhs->type() == MIRType::Int32);
  MOZ_ASSERT(rhs->type() == MIRType::Int32);

  LUrshD* lir = new (alloc())
      LUrshD(useRegister(lhs), useRegisterOrConstant(rhs), temp());
  define(lir, mir);
}

void LIRGeneratorMIPSShared::lowerPowOfTwoI(MPow* mir) {
  int32_t base = mir->input()->toConstant()->toInt32();
  MDefinition* power = mir->power();

  auto* lir = new (alloc()) LPowOfTwoI(base, useRegister(power));
  assignSnapshot(lir, mir->bailoutKind());
  define(lir, mir);
}

void LIRGeneratorMIPSShared::lowerBigIntLsh(MBigIntLsh* ins) {
  auto* lir = new (alloc()) LBigIntLsh(
      useRegister(ins->lhs()), useRegister(ins->rhs()), temp(), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGeneratorMIPSShared::lowerBigIntRsh(MBigIntRsh* ins) {
  auto* lir = new (alloc()) LBigIntRsh(
      useRegister(ins->lhs()), useRegister(ins->rhs()), temp(), temp(), temp());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGenerator::visitWasmNeg(MWasmNeg* ins) {
  if (ins->type() == MIRType::Int32) {
    define(new (alloc()) LNegI(useRegisterAtStart(ins->input())), ins);
  } else if (ins->type() == MIRType::Float32) {
    define(new (alloc()) LNegF(useRegisterAtStart(ins->input())), ins);
  } else {
    MOZ_ASSERT(ins->type() == MIRType::Double);
    define(new (alloc()) LNegD(useRegisterAtStart(ins->input())), ins);
  }
}

void LIRGenerator::visitWasmHeapBase(MWasmHeapBase* ins) {
  auto* lir = new (alloc()) LWasmHeapBase(LAllocation());
  define(lir, ins);
}

void LIRGenerator::visitWasmLoad(MWasmLoad* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  LAllocation ptr;
#ifdef JS_CODEGEN_MIPS32
  if (ins->type() == MIRType::Int64) {
    ptr = useRegister(base);
  } else {
    ptr = useRegisterAtStart(base);
  }
#else
  ptr = useRegisterAtStart(base);
#endif

  if (IsUnaligned(ins->access())) {
    if (ins->type() == MIRType::Int64) {
      auto* lir = new (alloc()) LWasmUnalignedLoadI64(ptr, temp());
      if (ins->access().offset()) {
        lir->setTemp(0, tempCopy(base, 0));
      }

      defineInt64(lir, ins);
      return;
    }

    auto* lir = new (alloc()) LWasmUnalignedLoad(ptr, temp());
    if (ins->access().offset()) {
      lir->setTemp(0, tempCopy(base, 0));
    }

    define(lir, ins);
    return;
  }

  if (ins->type() == MIRType::Int64) {
#ifdef JS_CODEGEN_MIPS32
    if (ins->access().isAtomic()) {
      auto* lir = new (alloc()) LWasmAtomicLoadI64(ptr);
      defineInt64(lir, ins);
      return;
    }
#endif
    auto* lir = new (alloc()) LWasmLoadI64(ptr);
    if (ins->access().offset()) {
      lir->setTemp(0, tempCopy(base, 0));
    }

    defineInt64(lir, ins);
    return;
  }

  auto* lir = new (alloc()) LWasmLoad(ptr);
  if (ins->access().offset()) {
    lir->setTemp(0, tempCopy(base, 0));
  }

  define(lir, ins);
}

void LIRGenerator::visitWasmStore(MWasmStore* ins) {
  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);

  MDefinition* value = ins->value();

  if (IsUnaligned(ins->access())) {
    LAllocation baseAlloc = useRegisterAtStart(base);
    if (ins->access().type() == Scalar::Int64) {
      LInt64Allocation valueAlloc = useInt64RegisterAtStart(value);
      auto* lir =
          new (alloc()) LWasmUnalignedStoreI64(baseAlloc, valueAlloc, temp());
      if (ins->access().offset()) {
        lir->setTemp(0, tempCopy(base, 0));
      }

      add(lir, ins);
      return;
    }

    LAllocation valueAlloc = useRegisterAtStart(value);
    auto* lir =
        new (alloc()) LWasmUnalignedStore(baseAlloc, valueAlloc, temp());
    if (ins->access().offset()) {
      lir->setTemp(0, tempCopy(base, 0));
    }

    add(lir, ins);
    return;
  }

  if (ins->access().type() == Scalar::Int64) {
#ifdef JS_CODEGEN_MIPS32
    if (ins->access().isAtomic()) {
      auto* lir = new (alloc()) LWasmAtomicStoreI64(
          useRegister(base), useInt64Register(value), temp());
      add(lir, ins);
      return;
    }
#endif

    LAllocation baseAlloc = useRegisterAtStart(base);
    LInt64Allocation valueAlloc = useInt64RegisterAtStart(value);
    auto* lir = new (alloc()) LWasmStoreI64(baseAlloc, valueAlloc);
    if (ins->access().offset()) {
      lir->setTemp(0, tempCopy(base, 0));
    }

    add(lir, ins);
    return;
  }

  LAllocation baseAlloc = useRegisterAtStart(base);
  LAllocation valueAlloc = useRegisterAtStart(value);
  auto* lir = new (alloc()) LWasmStore(baseAlloc, valueAlloc);
  if (ins->access().offset()) {
    lir->setTemp(0, tempCopy(base, 0));
  }

  add(lir, ins);
}

void LIRGeneratorMIPSShared::lowerUDiv(MDiv* div) {
  MDefinition* lhs = div->getOperand(0);
  MDefinition* rhs = div->getOperand(1);

  LUDivOrMod* lir = new (alloc()) LUDivOrMod;
  lir->setOperand(0, useRegister(lhs));
  lir->setOperand(1, useRegister(rhs));
  if (div->fallible()) {
    assignSnapshot(lir, div->bailoutKind());
  }

  define(lir, div);
}

void LIRGeneratorMIPSShared::lowerUMod(MMod* mod) {
  MDefinition* lhs = mod->getOperand(0);
  MDefinition* rhs = mod->getOperand(1);

  LUDivOrMod* lir = new (alloc()) LUDivOrMod;
  lir->setOperand(0, useRegister(lhs));
  lir->setOperand(1, useRegister(rhs));
  if (mod->fallible()) {
    assignSnapshot(lir, mod->bailoutKind());
  }

  define(lir, mod);
}

void LIRGenerator::visitWasmUnsignedToDouble(MWasmUnsignedToDouble* ins) {
  MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
  LWasmUint32ToDouble* lir =
      new (alloc()) LWasmUint32ToDouble(useRegisterAtStart(ins->input()));
  define(lir, ins);
}

void LIRGenerator::visitWasmUnsignedToFloat32(MWasmUnsignedToFloat32* ins) {
  MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
  LWasmUint32ToFloat32* lir =
      new (alloc()) LWasmUint32ToFloat32(useRegisterAtStart(ins->input()));
  define(lir, ins);
}

void LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap* ins) {
  MOZ_ASSERT(ins->access().offset() == 0);

  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);
  LAllocation baseAlloc;
  LAllocation limitAlloc;
  // For MIPS it is best to keep the 'base' in a register if a bounds check
  // is needed.
  if (base->isConstant() && !ins->needsBoundsCheck()) {
    // A bounds check is only skipped for a positive index.
    MOZ_ASSERT(base->toConstant()->toInt32() >= 0);
    baseAlloc = LAllocation(base->toConstant());
  } else {
    baseAlloc = useRegisterAtStart(base);
    if (ins->needsBoundsCheck()) {
      MDefinition* boundsCheckLimit = ins->boundsCheckLimit();
      MOZ_ASSERT(boundsCheckLimit->type() == MIRType::Int32);
      limitAlloc = useRegisterAtStart(boundsCheckLimit);
    }
  }

  define(new (alloc()) LAsmJSLoadHeap(baseAlloc, limitAlloc), ins);
}

void LIRGenerator::visitAsmJSStoreHeap(MAsmJSStoreHeap* ins) {
  MOZ_ASSERT(ins->access().offset() == 0);

  MDefinition* base = ins->base();
  MOZ_ASSERT(base->type() == MIRType::Int32);
  LAllocation baseAlloc;
  LAllocation limitAlloc;
  if (base->isConstant() && !ins->needsBoundsCheck()) {
    MOZ_ASSERT(base->toConstant()->toInt32() >= 0);
    baseAlloc = LAllocation(base->toConstant());
  } else {
    baseAlloc = useRegisterAtStart(base);
    if (ins->needsBoundsCheck()) {
      MDefinition* boundsCheckLimit = ins->boundsCheckLimit();
      MOZ_ASSERT(boundsCheckLimit->type() == MIRType::Int32);
      limitAlloc = useRegisterAtStart(boundsCheckLimit);
    }
  }

  add(new (alloc()) LAsmJSStoreHeap(baseAlloc, useRegisterAtStart(ins->value()),
                                    limitAlloc),
      ins);
}

void LIRGenerator::visitSubstr(MSubstr* ins) {
  LSubstr* lir = new (alloc())
      LSubstr(useRegister(ins->string()), useRegister(ins->begin()),
              useRegister(ins->length()), temp(), temp(), tempByteOpRegister());
  define(lir, ins);
  assignSafepoint(lir, ins);
}

void LIRGenerator::visitCompareExchangeTypedArrayElement(
    MCompareExchangeTypedArrayElement* ins) {
  MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::IntPtr);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->arrayType());

  const LAllocation newval = useRegister(ins->newval());
  const LAllocation oldval = useRegister(ins->oldval());

  if (Scalar::isBigIntType(ins->arrayType())) {
    LInt64Definition temp1 = tempInt64();
    LInt64Definition temp2 = tempInt64();

    auto* lir = new (alloc()) LCompareExchangeTypedArrayElement64(
        elements, index, oldval, newval, temp1, temp2);
    define(lir, ins);
    assignSafepoint(lir, ins);
    return;
  }

  // If the target is a floating register then we need a temp at the
  // CodeGenerator level for creating the result.

  LDefinition outTemp = LDefinition::BogusTemp();
  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
    outTemp = temp();
  }

  if (Scalar::byteSize(ins->arrayType()) < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LCompareExchangeTypedArrayElement* lir = new (alloc())
      LCompareExchangeTypedArrayElement(elements, index, oldval, newval,
                                        outTemp, valueTemp, offsetTemp,
                                        maskTemp);

  define(lir, ins);
}

void LIRGenerator::visitAtomicExchangeTypedArrayElement(
    MAtomicExchangeTypedArrayElement* ins) {
  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::IntPtr);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->arrayType());

  const LAllocation value = useRegister(ins->value());

  if (Scalar::isBigIntType(ins->arrayType())) {
    LInt64Definition temp1 = tempInt64();
    LDefinition temp2 = temp();

    auto* lir = new (alloc()) LAtomicExchangeTypedArrayElement64(
        elements, index, value, temp1, temp2);
    define(lir, ins);
    assignSafepoint(lir, ins);
    return;
  }

  // If the target is a floating register then we need a temp at the
  // CodeGenerator level for creating the result.

  MOZ_ASSERT(ins->arrayType() <= Scalar::Uint32);

  LDefinition outTemp = LDefinition::BogusTemp();
  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->arrayType() == Scalar::Uint32) {
    MOZ_ASSERT(ins->type() == MIRType::Double);
    outTemp = temp();
  }

  if (Scalar::byteSize(ins->arrayType()) < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LAtomicExchangeTypedArrayElement* lir =
      new (alloc()) LAtomicExchangeTypedArrayElement(
          elements, index, value, outTemp, valueTemp, offsetTemp, maskTemp);

  define(lir, ins);
}

void LIRGenerator::visitWasmCompareExchangeHeap(MWasmCompareExchangeHeap* ins) {
  MOZ_ASSERT(ins->base()->type() == MIRType::Int32);

  if (ins->access().type() == Scalar::Int64) {
    auto* lir = new (alloc()) LWasmCompareExchangeI64(
        useRegister(ins->base()), useInt64Register(ins->oldValue()),
        useInt64Register(ins->newValue()));
    defineInt64(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->access().byteSize() < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LWasmCompareExchangeHeap* lir = new (alloc()) LWasmCompareExchangeHeap(
      useRegister(ins->base()), useRegister(ins->oldValue()),
      useRegister(ins->newValue()), valueTemp, offsetTemp, maskTemp);

  define(lir, ins);
}

void LIRGenerator::visitWasmAtomicExchangeHeap(MWasmAtomicExchangeHeap* ins) {
  MOZ_ASSERT(ins->base()->type() == MIRType::Int32);

  if (ins->access().type() == Scalar::Int64) {
    auto* lir = new (alloc()) LWasmAtomicExchangeI64(
        useRegister(ins->base()), useInt64Register(ins->value()));
    defineInt64(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->access().byteSize() < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  LWasmAtomicExchangeHeap* lir = new (alloc()) LWasmAtomicExchangeHeap(
      useRegister(ins->base()), useRegister(ins->value()), valueTemp,
      offsetTemp, maskTemp);
  define(lir, ins);
}

void LIRGenerator::visitWasmAtomicBinopHeap(MWasmAtomicBinopHeap* ins) {
  MOZ_ASSERT(ins->base()->type() == MIRType::Int32);

  if (ins->access().type() == Scalar::Int64) {
    auto* lir = new (alloc()) LWasmAtomicBinopI64(
        useRegister(ins->base()), useInt64Register(ins->value()));
    lir->setTemp(0, temp());
#ifdef JS_CODEGEN_MIPS32
    lir->setTemp(1, temp());
#endif
    defineInt64(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (ins->access().byteSize() < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  if (!ins->hasUses()) {
    LWasmAtomicBinopHeapForEffect* lir = new (alloc())
        LWasmAtomicBinopHeapForEffect(useRegister(ins->base()),
                                      useRegister(ins->value()), valueTemp,
                                      offsetTemp, maskTemp);
    add(lir, ins);
    return;
  }

  LWasmAtomicBinopHeap* lir = new (alloc())
      LWasmAtomicBinopHeap(useRegister(ins->base()), useRegister(ins->value()),
                           valueTemp, offsetTemp, maskTemp);

  define(lir, ins);
}

void LIRGenerator::visitAtomicTypedArrayElementBinop(
    MAtomicTypedArrayElementBinop* ins) {
  MOZ_ASSERT(ins->arrayType() != Scalar::Uint8Clamped);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
  MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

  MOZ_ASSERT(ins->elements()->type() == MIRType::Elements);
  MOZ_ASSERT(ins->index()->type() == MIRType::IntPtr);

  const LUse elements = useRegister(ins->elements());
  const LAllocation index =
      useRegisterOrIndexConstant(ins->index(), ins->arrayType());
  const LAllocation value = useRegister(ins->value());

  if (Scalar::isBigIntType(ins->arrayType())) {
    LInt64Definition temp1 = tempInt64();
    LInt64Definition temp2 = tempInt64();

    // Case 1: the result of the operation is not used.
    //
    // We can omit allocating the result BigInt.

    if (ins->isForEffect()) {
      auto* lir = new (alloc()) LAtomicTypedArrayElementBinopForEffect64(
          elements, index, value, temp1, temp2);
      add(lir, ins);
      return;
    }

    // Case 2: the result of the operation is used.

    auto* lir = new (alloc())
        LAtomicTypedArrayElementBinop64(elements, index, value, temp1, temp2);
    define(lir, ins);
    assignSafepoint(lir, ins);
    return;
  }

  LDefinition valueTemp = LDefinition::BogusTemp();
  LDefinition offsetTemp = LDefinition::BogusTemp();
  LDefinition maskTemp = LDefinition::BogusTemp();

  if (Scalar::byteSize(ins->arrayType()) < 4) {
    valueTemp = temp();
    offsetTemp = temp();
    maskTemp = temp();
  }

  if (ins->isForEffect()) {
    LAtomicTypedArrayElementBinopForEffect* lir =
        new (alloc()) LAtomicTypedArrayElementBinopForEffect(
            elements, index, value, valueTemp, offsetTemp, maskTemp);
    add(lir, ins);
    return;
  }

  // For a Uint32Array with a known double result we need a temp for
  // the intermediate output.

  LDefinition outTemp = LDefinition::BogusTemp();

  if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
    outTemp = temp();
  }

  LAtomicTypedArrayElementBinop* lir =
      new (alloc()) LAtomicTypedArrayElementBinop(
          elements, index, value, outTemp, valueTemp, offsetTemp, maskTemp);
  define(lir, ins);
}

void LIRGenerator::visitCopySign(MCopySign* ins) {
  MDefinition* lhs = ins->lhs();
  MDefinition* rhs = ins->rhs();

  MOZ_ASSERT(IsFloatingPointType(lhs->type()));
  MOZ_ASSERT(lhs->type() == rhs->type());
  MOZ_ASSERT(lhs->type() == ins->type());

  LInstructionHelper<1, 2, 2>* lir;
  if (lhs->type() == MIRType::Double) {
    lir = new (alloc()) LCopySignD();
  } else {
    lir = new (alloc()) LCopySignF();
  }

  lir->setTemp(0, temp());
  lir->setTemp(1, temp());

  lir->setOperand(0, useRegisterAtStart(lhs));
  lir->setOperand(1, useRegister(rhs));
  defineReuseInput(lir, ins, 0);
}

void LIRGenerator::visitExtendInt32ToInt64(MExtendInt32ToInt64* ins) {
  defineInt64(
      new (alloc()) LExtendInt32ToInt64(useRegisterAtStart(ins->input())), ins);
}

void LIRGenerator::visitSignExtendInt64(MSignExtendInt64* ins) {
  defineInt64(new (alloc())
                  LSignExtendInt64(useInt64RegisterAtStart(ins->input())),
              ins);
}

#ifdef ENABLE_WASM_SIMD
void LIRGenerator::visitWasmBitselectSimd128(MWasmBitselectSimd128* ins) {
  MOZ_CRASH("bitselect NYI");
}

void LIRGenerator::visitWasmBinarySimd128(MWasmBinarySimd128* ins) {
  MOZ_CRASH("binary SIMD NYI");
}

bool MWasmBitselectSimd128::specializeConstantMaskAsShuffle(
    int8_t shuffle[16]) {
  return false;
}

bool MWasmBinarySimd128::specializeForConstantRhs() {
  // Probably many we want to do here
  return false;
}

void LIRGenerator::visitWasmBinarySimd128WithConstant(
    MWasmBinarySimd128WithConstant* ins) {
  MOZ_CRASH("binary SIMD with constant NYI");
}

void LIRGenerator::visitWasmShiftSimd128(MWasmShiftSimd128* ins) {
  MOZ_CRASH("shift SIMD NYI");
}

void LIRGenerator::visitWasmShuffleSimd128(MWasmShuffleSimd128* ins) {
  MOZ_CRASH("shuffle SIMD NYI");
}

void LIRGenerator::visitWasmReplaceLaneSimd128(MWasmReplaceLaneSimd128* ins) {
  MOZ_CRASH("replace-lane SIMD NYI");
}

void LIRGenerator::visitWasmScalarToSimd128(MWasmScalarToSimd128* ins) {
  MOZ_CRASH("scalar-to-SIMD NYI");
}

void LIRGenerator::visitWasmUnarySimd128(MWasmUnarySimd128* ins) {
  MOZ_CRASH("unary SIMD NYI");
}

void LIRGenerator::visitWasmReduceSimd128(MWasmReduceSimd128* ins) {
  MOZ_CRASH("reduce-SIMD NYI");
}
#endif
