/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/CodeGenerator-x64.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/CodeGenerator.h"
#include "jit/MIR.h"
#include "js/ScalarType.h"  // js::Scalar::Type

#include "jit/MacroAssembler-inl.h"
#include "jit/shared/CodeGenerator-shared-inl.h"
#include "vm/JSScript-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

CodeGeneratorX64::CodeGeneratorX64(MIRGenerator* gen, LIRGraph* graph,
                                   MacroAssembler* masm)
    : CodeGeneratorX86Shared(gen, graph, masm) {}

ValueOperand CodeGeneratorX64::ToValue(LInstruction* ins, size_t pos) {
  return ValueOperand(ToRegister(ins->getOperand(pos)));
}

ValueOperand CodeGeneratorX64::ToTempValue(LInstruction* ins, size_t pos) {
  return ValueOperand(ToRegister(ins->getTemp(pos)));
}

Operand CodeGeneratorX64::ToOperand64(const LInt64Allocation& a64) {
  const LAllocation& a = a64.value();
  MOZ_ASSERT(!a.isFloatReg());
  if (a.isGeneralReg()) {
    return Operand(a.toGeneralReg()->reg());
  }
  return Operand(ToAddress(a));
}

FrameSizeClass FrameSizeClass::FromDepth(uint32_t frameDepth) {
  return FrameSizeClass::None();
}

FrameSizeClass FrameSizeClass::ClassLimit() { return FrameSizeClass(0); }

uint32_t FrameSizeClass::frameSize() const {
  MOZ_CRASH("x64 does not use frame size classes");
}

void CodeGenerator::visitValue(LValue* value) {
  ValueOperand result = ToOutValue(value);
  masm.moveValue(value->value(), result);
}

void CodeGenerator::visitBox(LBox* box) {
  const LAllocation* in = box->getOperand(0);
  ValueOperand result = ToOutValue(box);

  masm.moveValue(TypedOrValueRegister(box->type(), ToAnyRegister(in)), result);
}

void CodeGenerator::visitUnbox(LUnbox* unbox) {
  MUnbox* mir = unbox->mir();

  Register result = ToRegister(unbox->output());

  if (mir->fallible()) {
    const ValueOperand value = ToValue(unbox, LUnbox::Input);
    Label bail;
    switch (mir->type()) {
      case MIRType::Int32:
        masm.fallibleUnboxInt32(value, result, &bail);
        break;
      case MIRType::Boolean:
        masm.fallibleUnboxBoolean(value, result, &bail);
        break;
      case MIRType::Object:
        masm.fallibleUnboxObject(value, result, &bail);
        break;
      case MIRType::String:
        masm.fallibleUnboxString(value, result, &bail);
        break;
      case MIRType::Symbol:
        masm.fallibleUnboxSymbol(value, result, &bail);
        break;
      case MIRType::BigInt:
        masm.fallibleUnboxBigInt(value, result, &bail);
        break;
      default:
        MOZ_CRASH("Given MIRType cannot be unboxed.");
    }
    bailoutFrom(&bail, unbox->snapshot());
    return;
  }

  // Infallible unbox.

  Operand input = ToOperand(unbox->getOperand(LUnbox::Input));

#ifdef DEBUG
  // Assert the types match.
  JSValueTag tag = MIRTypeToTag(mir->type());
  Label ok;
  masm.splitTag(input, ScratchReg);
  masm.branch32(Assembler::Equal, ScratchReg, Imm32(tag), &ok);
  masm.assumeUnreachable("Infallible unbox type mismatch");
  masm.bind(&ok);
#endif

  switch (mir->type()) {
    case MIRType::Int32:
      masm.unboxInt32(input, result);
      break;
    case MIRType::Boolean:
      masm.unboxBoolean(input, result);
      break;
    case MIRType::Object:
      masm.unboxObject(input, result);
      break;
    case MIRType::String:
      masm.unboxString(input, result);
      break;
    case MIRType::Symbol:
      masm.unboxSymbol(input, result);
      break;
    case MIRType::BigInt:
      masm.unboxBigInt(input, result);
      break;
    default:
      MOZ_CRASH("Given MIRType cannot be unboxed.");
  }
}

void CodeGenerator::visitCompareI64(LCompareI64* lir) {
  MCompare* mir = lir->mir();
  MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
             mir->compareType() == MCompare::Compare_UInt64);

  const LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
  const LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
  Register lhsReg = ToRegister64(lhs).reg;
  Register output = ToRegister(lir->output());

  if (IsConstant(rhs)) {
    masm.cmpPtr(lhsReg, ImmWord(ToInt64(rhs)));
  } else {
    masm.cmpPtr(lhsReg, ToOperand64(rhs));
  }

  bool isSigned = mir->compareType() == MCompare::Compare_Int64;
  masm.emitSet(JSOpToCondition(lir->jsop(), isSigned), output);
}

void CodeGenerator::visitCompareI64AndBranch(LCompareI64AndBranch* lir) {
  MCompare* mir = lir->cmpMir();
  MOZ_ASSERT(mir->compareType() == MCompare::Compare_Int64 ||
             mir->compareType() == MCompare::Compare_UInt64);

  LInt64Allocation lhs = lir->getInt64Operand(LCompareI64::Lhs);
  LInt64Allocation rhs = lir->getInt64Operand(LCompareI64::Rhs);
  Register lhsReg = ToRegister64(lhs).reg;

  if (IsConstant(rhs)) {
    masm.cmpPtr(lhsReg, ImmWord(ToInt64(rhs)));
  } else {
    masm.cmpPtr(lhsReg, ToOperand64(rhs));
  }

  bool isSigned = mir->compareType() == MCompare::Compare_Int64;
  emitBranch(JSOpToCondition(lir->jsop(), isSigned), lir->ifTrue(),
             lir->ifFalse());
}

void CodeGenerator::visitDivOrModI64(LDivOrModI64* lir) {
  Register lhs = ToRegister(lir->lhs());
  Register rhs = ToRegister(lir->rhs());
  Register output = ToRegister(lir->output());

  MOZ_ASSERT_IF(lhs != rhs, rhs != rax);
  MOZ_ASSERT(rhs != rdx);
  MOZ_ASSERT_IF(output == rax, ToRegister(lir->remainder()) == rdx);
  MOZ_ASSERT_IF(output == rdx, ToRegister(lir->remainder()) == rax);

  Label done;

  // Put the lhs in rax.
  if (lhs != rax) {
    masm.mov(lhs, rax);
  }

  // Handle divide by zero.
  if (lir->canBeDivideByZero()) {
    Label nonZero;
    masm.branchTestPtr(Assembler::NonZero, rhs, rhs, &nonZero);
    masm.wasmTrap(wasm::Trap::IntegerDivideByZero, lir->bytecodeOffset());
    masm.bind(&nonZero);
  }

  // Handle an integer overflow exception from INT64_MIN / -1.
  if (lir->canBeNegativeOverflow()) {
    Label notOverflow;
    masm.branchPtr(Assembler::NotEqual, lhs, ImmWord(INT64_MIN), &notOverflow);
    masm.branchPtr(Assembler::NotEqual, rhs, ImmWord(-1), &notOverflow);
    if (lir->mir()->isMod()) {
      masm.xorl(output, output);
    } else {
      masm.wasmTrap(wasm::Trap::IntegerOverflow, lir->bytecodeOffset());
    }
    masm.jump(&done);
    masm.bind(&notOverflow);
  }

  // Sign extend the lhs into rdx to make rdx:rax.
  masm.cqo();
  masm.idivq(rhs);

  masm.bind(&done);
}

void CodeGenerator::visitUDivOrModI64(LUDivOrModI64* lir) {
  Register lhs = ToRegister(lir->lhs());
  Register rhs = ToRegister(lir->rhs());

  DebugOnly<Register> output = ToRegister(lir->output());
  MOZ_ASSERT_IF(lhs != rhs, rhs != rax);
  MOZ_ASSERT(rhs != rdx);
  MOZ_ASSERT_IF(output.value == rax, ToRegister(lir->remainder()) == rdx);
  MOZ_ASSERT_IF(output.value == rdx, ToRegister(lir->remainder()) == rax);

  // Put the lhs in rax.
  if (lhs != rax) {
    masm.mov(lhs, rax);
  }

  Label done;

  // Prevent divide by zero.
  if (lir->canBeDivideByZero()) {
    Label nonZero;
    masm.branchTestPtr(Assembler::NonZero, rhs, rhs, &nonZero);
    masm.wasmTrap(wasm::Trap::IntegerDivideByZero, lir->bytecodeOffset());
    masm.bind(&nonZero);
  }

  // Zero extend the lhs into rdx to make (rdx:rax).
  masm.xorl(rdx, rdx);
  masm.udivq(rhs);

  masm.bind(&done);
}

void CodeGeneratorX64::emitBigIntDiv(LBigIntDiv* ins, Register dividend,
                                     Register divisor, Register output,
                                     Label* fail) {
  // Callers handle division by zero and integer overflow.

  MOZ_ASSERT(dividend == rax);
  MOZ_ASSERT(output == rdx);

  // Sign extend the lhs into rdx to make rdx:rax.
  masm.cqo();

  masm.idivq(divisor);

  // Create and return the result.
  masm.newGCBigInt(output, divisor, fail, bigIntsCanBeInNursery());
  masm.initializeBigInt(output, dividend);
}

void CodeGeneratorX64::emitBigIntMod(LBigIntMod* ins, Register dividend,
                                     Register divisor, Register output,
                                     Label* fail) {
  // Callers handle division by zero and integer overflow.

  MOZ_ASSERT(dividend == rax);
  MOZ_ASSERT(output == rdx);

  // Sign extend the lhs into rdx to make rdx:rax.
  masm.cqo();

  masm.idivq(divisor);

  // Move the remainder from rdx.
  masm.movq(output, dividend);

  // Create and return the result.
  masm.newGCBigInt(output, divisor, fail, bigIntsCanBeInNursery());
  masm.initializeBigInt(output, dividend);
}

void CodeGenerator::visitAtomicLoad64(LAtomicLoad64* lir) {
  Register elements = ToRegister(lir->elements());
  Register temp = ToRegister(lir->temp());
  Register64 temp64 = ToRegister64(lir->temp64());
  Register out = ToRegister(lir->output());

  const MLoadUnboxedScalar* mir = lir->mir();

  Scalar::Type storageType = mir->storageType();

  auto sync = Synchronization::Load();

  masm.memoryBarrierBefore(sync);
  if (lir->index()->isConstant()) {
    Address source =
        ToAddress(elements, lir->index(), storageType, mir->offsetAdjustment());
    masm.load64(source, temp64);
  } else {
    BaseIndex source(elements, ToRegister(lir->index()),
                     ScaleFromScalarType(storageType), mir->offsetAdjustment());
    masm.load64(source, temp64);
  }
  masm.memoryBarrierAfter(sync);

  emitCreateBigInt(lir, storageType, temp64, out, temp);
}

void CodeGenerator::visitAtomicStore64(LAtomicStore64* lir) {
  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());

  Scalar::Type writeType = lir->mir()->writeType();

  masm.loadBigInt64(value, temp1);

  auto sync = Synchronization::Store();

  masm.memoryBarrierBefore(sync);
  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), writeType);
    masm.store64(temp1, dest);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(writeType));
    masm.store64(temp1, dest);
  }
  masm.memoryBarrierAfter(sync);
}

void CodeGenerator::visitCompareExchangeTypedArrayElement64(
    LCompareExchangeTypedArrayElement64* lir) {
  Register elements = ToRegister(lir->elements());
  Register oldval = ToRegister(lir->oldval());
  Register newval = ToRegister(lir->newval());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register64 temp2 = ToRegister64(lir->temp2());
  Register out = ToRegister(lir->output());

  MOZ_ASSERT(temp1.reg == rax);

  Scalar::Type arrayType = lir->mir()->arrayType();

  masm.loadBigInt64(oldval, temp1);
  masm.loadBigInt64(newval, temp2);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.compareExchange64(Synchronization::Full(), dest, temp1, temp2, temp1);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.compareExchange64(Synchronization::Full(), dest, temp1, temp2, temp1);
  }

  emitCreateBigInt(lir, arrayType, temp1, out, temp2.reg);
}

void CodeGenerator::visitAtomicExchangeTypedArrayElement64(
    LAtomicExchangeTypedArrayElement64* lir) {
  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register temp2 = ToRegister(lir->temp2());
  Register out = ToRegister(lir->output());

  Scalar::Type arrayType = lir->mir()->arrayType();

  masm.loadBigInt64(value, temp1);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicExchange64(Synchronization::Full(), dest, temp1, temp1);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicExchange64(Synchronization::Full(), dest, temp1, temp1);
  }

  emitCreateBigInt(lir, arrayType, temp1, out, temp2);
}

void CodeGenerator::visitAtomicTypedArrayElementBinop64(
    LAtomicTypedArrayElementBinop64* lir) {
  MOZ_ASSERT(!lir->mir()->isForEffect());

  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());
  Register64 temp2 = ToRegister64(lir->temp2());
  Register out = ToRegister(lir->output());

  Scalar::Type arrayType = lir->mir()->arrayType();
  AtomicOp atomicOp = lir->mir()->operation();

  masm.loadBigInt64(value, temp1);

  Register64 fetchTemp = Register64(out);
  Register64 fetchOut = temp2;
  Register createTemp = temp1.reg;

  // Add and Sub don't need |fetchTemp| and can save a `mov` when the value and
  // output register are equal to each other.
  if (atomicOp == AtomicFetchAddOp || atomicOp == AtomicFetchSubOp) {
    fetchTemp = Register64::Invalid();
    fetchOut = temp1;
    createTemp = temp2.reg;
  } else {
    MOZ_ASSERT(temp2.reg == rax);
  }

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicFetchOp64(Synchronization::Full(), atomicOp, temp1, dest,
                         fetchTemp, fetchOut);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicFetchOp64(Synchronization::Full(), atomicOp, temp1, dest,
                         fetchTemp, fetchOut);
  }

  emitCreateBigInt(lir, arrayType, fetchOut, out, createTemp);
}

void CodeGenerator::visitAtomicTypedArrayElementBinopForEffect64(
    LAtomicTypedArrayElementBinopForEffect64* lir) {
  MOZ_ASSERT(lir->mir()->isForEffect());

  Register elements = ToRegister(lir->elements());
  Register value = ToRegister(lir->value());
  Register64 temp1 = ToRegister64(lir->temp1());

  Scalar::Type arrayType = lir->mir()->arrayType();
  AtomicOp atomicOp = lir->mir()->operation();

  masm.loadBigInt64(value, temp1);

  if (lir->index()->isConstant()) {
    Address dest = ToAddress(elements, lir->index(), arrayType);
    masm.atomicEffectOp64(Synchronization::Full(), atomicOp, temp1, dest);
  } else {
    BaseIndex dest(elements, ToRegister(lir->index()),
                   ScaleFromScalarType(arrayType));
    masm.atomicEffectOp64(Synchronization::Full(), atomicOp, temp1, dest);
  }
}

void CodeGenerator::visitWasmRegisterResult(LWasmRegisterResult* lir) {
  if (JitOptions.spectreIndexMasking) {
    if (MWasmRegisterResult* mir = lir->mir()) {
      if (mir->type() == MIRType::Int32) {
        masm.movl(ToRegister(lir->output()), ToRegister(lir->output()));
      }
    }
  }
}

void CodeGenerator::visitWasmSelectI64(LWasmSelectI64* lir) {
  MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);

  Register cond = ToRegister(lir->condExpr());

  Operand falseExpr = ToOperandOrRegister64(lir->falseExpr());

  Register64 out = ToOutRegister64(lir);
  MOZ_ASSERT(ToRegister64(lir->trueExpr()) == out,
             "true expr is reused for input");

  masm.test32(cond, cond);
  masm.cmovzq(falseExpr, out.reg);
}

void CodeGenerator::visitWasmReinterpretFromI64(LWasmReinterpretFromI64* lir) {
  MOZ_ASSERT(lir->mir()->type() == MIRType::Double);
  MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Int64);
  masm.vmovq(ToRegister(lir->input()), ToFloatRegister(lir->output()));
}

void CodeGenerator::visitWasmReinterpretToI64(LWasmReinterpretToI64* lir) {
  MOZ_ASSERT(lir->mir()->type() == MIRType::Int64);
  MOZ_ASSERT(lir->mir()->input()->type() == MIRType::Double);
  masm.vmovq(ToFloatRegister(lir->input()), ToRegister(lir->output()));
}

void CodeGenerator::visitWasmUint32ToDouble(LWasmUint32ToDouble* lir) {
  masm.convertUInt32ToDouble(ToRegister(lir->input()),
                             ToFloatRegister(lir->output()));
}

void CodeGenerator::visitWasmUint32ToFloat32(LWasmUint32ToFloat32* lir) {
  masm.convertUInt32ToFloat32(ToRegister(lir->input()),
                              ToFloatRegister(lir->output()));
}

void CodeGeneratorX64::wasmStore(const wasm::MemoryAccessDesc& access,
                                 const LAllocation* value, Operand dstAddr) {
  if (value->isConstant()) {
    masm.memoryBarrierBefore(access.sync());

    const MConstant* mir = value->toConstant();
    Imm32 cst =
        Imm32(mir->type() == MIRType::Int32 ? mir->toInt32() : mir->toInt64());

    masm.append(access, masm.size());
    switch (access.type()) {
      case Scalar::Int8:
      case Scalar::Uint8:
        masm.movb(cst, dstAddr);
        break;
      case Scalar::Int16:
      case Scalar::Uint16:
        masm.movw(cst, dstAddr);
        break;
      case Scalar::Int32:
      case Scalar::Uint32:
        masm.movl(cst, dstAddr);
        break;
      case Scalar::Int64:
      case Scalar::Float32:
      case Scalar::Float64:
      case Scalar::Uint8Clamped:
      case Scalar::BigInt64:
      case Scalar::BigUint64:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("unexpected array type");
    }

    masm.memoryBarrierAfter(access.sync());
  } else {
    masm.wasmStore(access, ToAnyRegister(value), dstAddr);
  }
}

void CodeGenerator::visitWasmHeapBase(LWasmHeapBase* ins) {
  MOZ_ASSERT(ins->tlsPtr()->isBogus());
  masm.movePtr(HeapReg, ToRegister(ins->output()));
}

template <typename T>
void CodeGeneratorX64::emitWasmLoad(T* ins) {
  const MWasmLoad* mir = ins->mir();

  uint32_t offset = mir->access().offset();
  MOZ_ASSERT(offset < masm.wasmMaxOffsetGuardLimit());

  const LAllocation* ptr = ins->ptr();
  Operand srcAddr = ptr->isBogus()
                        ? Operand(HeapReg, offset)
                        : Operand(HeapReg, ToRegister(ptr), TimesOne, offset);

  if (mir->type() == MIRType::Int64) {
    masm.wasmLoadI64(mir->access(), srcAddr, ToOutRegister64(ins));
  } else {
    masm.wasmLoad(mir->access(), srcAddr, ToAnyRegister(ins->output()));
  }
}

void CodeGenerator::visitWasmLoad(LWasmLoad* ins) { emitWasmLoad(ins); }

void CodeGenerator::visitWasmLoadI64(LWasmLoadI64* ins) { emitWasmLoad(ins); }

template <typename T>
void CodeGeneratorX64::emitWasmStore(T* ins) {
  const MWasmStore* mir = ins->mir();
  const wasm::MemoryAccessDesc& access = mir->access();

  uint32_t offset = access.offset();
  MOZ_ASSERT(offset < masm.wasmMaxOffsetGuardLimit());

  const LAllocation* value = ins->getOperand(ins->ValueIndex);
  const LAllocation* ptr = ins->ptr();
  Operand dstAddr = ptr->isBogus()
                        ? Operand(HeapReg, offset)
                        : Operand(HeapReg, ToRegister(ptr), TimesOne, offset);

  wasmStore(access, value, dstAddr);
}

void CodeGenerator::visitWasmStore(LWasmStore* ins) { emitWasmStore(ins); }

void CodeGenerator::visitWasmStoreI64(LWasmStoreI64* ins) {
  emitWasmStore(ins);
}

void CodeGenerator::visitWasmCompareExchangeHeap(
    LWasmCompareExchangeHeap* ins) {
  MWasmCompareExchangeHeap* mir = ins->mir();

  Register ptr = ToRegister(ins->ptr());
  Register oldval = ToRegister(ins->oldValue());
  Register newval = ToRegister(ins->newValue());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  Scalar::Type accessType = mir->access().type();
  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());

  if (accessType == Scalar::Int64) {
    masm.wasmCompareExchange64(mir->access(), srcAddr, Register64(oldval),
                               Register64(newval), ToOutRegister64(ins));
  } else {
    masm.wasmCompareExchange(mir->access(), srcAddr, oldval, newval,
                             ToRegister(ins->output()));
  }
}

void CodeGenerator::visitWasmAtomicExchangeHeap(LWasmAtomicExchangeHeap* ins) {
  MWasmAtomicExchangeHeap* mir = ins->mir();

  Register ptr = ToRegister(ins->ptr());
  Register value = ToRegister(ins->value());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  Scalar::Type accessType = mir->access().type();

  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());

  if (accessType == Scalar::Int64) {
    masm.wasmAtomicExchange64(mir->access(), srcAddr, Register64(value),
                              ToOutRegister64(ins));
  } else {
    masm.wasmAtomicExchange(mir->access(), srcAddr, value,
                            ToRegister(ins->output()));
  }
}

void CodeGenerator::visitWasmAtomicBinopHeap(LWasmAtomicBinopHeap* ins) {
  MWasmAtomicBinopHeap* mir = ins->mir();
  MOZ_ASSERT(mir->hasUses());

  Register ptr = ToRegister(ins->ptr());
  const LAllocation* value = ins->value();
  Register temp =
      ins->temp()->isBogusTemp() ? InvalidReg : ToRegister(ins->temp());
  Register output = ToRegister(ins->output());
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  Scalar::Type accessType = mir->access().type();
  if (accessType == Scalar::Uint32) {
    accessType = Scalar::Int32;
  }

  AtomicOp op = mir->operation();
  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());

  if (accessType == Scalar::Int64) {
    Register64 val = Register64(ToRegister(value));
    Register64 out = Register64(output);
    Register64 tmp = Register64(temp);
    masm.wasmAtomicFetchOp64(mir->access(), op, val, srcAddr, tmp, out);
  } else if (value->isConstant()) {
    masm.wasmAtomicFetchOp(mir->access(), op, Imm32(ToInt32(value)), srcAddr,
                           temp, output);
  } else {
    masm.wasmAtomicFetchOp(mir->access(), op, ToRegister(value), srcAddr, temp,
                           output);
  }
}

void CodeGenerator::visitWasmAtomicBinopHeapForEffect(
    LWasmAtomicBinopHeapForEffect* ins) {
  MWasmAtomicBinopHeap* mir = ins->mir();
  MOZ_ASSERT(!mir->hasUses());

  Register ptr = ToRegister(ins->ptr());
  const LAllocation* value = ins->value();
  MOZ_ASSERT(ins->addrTemp()->isBogusTemp());

  Scalar::Type accessType = mir->access().type();
  AtomicOp op = mir->operation();

  BaseIndex srcAddr(HeapReg, ptr, TimesOne, mir->access().offset());

  if (accessType == Scalar::Int64) {
    Register64 val = Register64(ToRegister(value));
    masm.wasmAtomicEffectOp64(mir->access(), op, val, srcAddr);
  } else if (value->isConstant()) {
    Imm32 c(0);
    if (value->toConstant()->type() == MIRType::Int64) {
      c = Imm32(ToInt64(value));
    } else {
      c = Imm32(ToInt32(value));
    }
    masm.wasmAtomicEffectOp(mir->access(), op, c, srcAddr, InvalidReg);
  } else {
    masm.wasmAtomicEffectOp(mir->access(), op, ToRegister(value), srcAddr,
                            InvalidReg);
  }
}

void CodeGenerator::visitTruncateDToInt32(LTruncateDToInt32* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  Register output = ToRegister(ins->output());

  // On x64, branchTruncateDouble uses vcvttsd2sq. Unlike the x86
  // implementation, this should handle most doubles and we can just
  // call a stub if it fails.
  emitTruncateDouble(input, output, ins->mir());
}

void CodeGenerator::visitWasmBuiltinTruncateDToInt32(
    LWasmBuiltinTruncateDToInt32* lir) {
  FloatRegister input = ToFloatRegister(lir->getOperand(0));
  Register output = ToRegister(lir->getDef(0));

  emitTruncateDouble(input, output, lir->mir());
}

void CodeGenerator::visitWasmBuiltinTruncateFToInt32(
    LWasmBuiltinTruncateFToInt32* lir) {
  FloatRegister input = ToFloatRegister(lir->getOperand(0));
  Register output = ToRegister(lir->getDef(0));

  emitTruncateFloat32(input, output, lir->mir());
}

void CodeGenerator::visitTruncateFToInt32(LTruncateFToInt32* ins) {
  FloatRegister input = ToFloatRegister(ins->input());
  Register output = ToRegister(ins->output());

  // On x64, branchTruncateFloat32 uses vcvttss2sq. Unlike the x86
  // implementation, this should handle most floats and we can just
  // call a stub if it fails.
  emitTruncateFloat32(input, output, ins->mir());
}

void CodeGenerator::visitWrapInt64ToInt32(LWrapInt64ToInt32* lir) {
  const LAllocation* input = lir->getOperand(0);
  Register output = ToRegister(lir->output());

  if (lir->mir()->bottomHalf()) {
    masm.movl(ToOperand(input), output);
  } else {
    MOZ_CRASH("Not implemented.");
  }
}

void CodeGenerator::visitExtendInt32ToInt64(LExtendInt32ToInt64* lir) {
  const LAllocation* input = lir->getOperand(0);
  Register output = ToRegister(lir->output());

  if (lir->mir()->isUnsigned()) {
    masm.movl(ToOperand(input), output);
  } else {
    masm.movslq(ToOperand(input), output);
  }
}

void CodeGenerator::visitSignExtendInt64(LSignExtendInt64* ins) {
  Register64 input = ToRegister64(ins->getInt64Operand(0));
  Register64 output = ToOutRegister64(ins);
  switch (ins->mode()) {
    case MSignExtendInt64::Byte:
      masm.movsbq(Operand(input.reg), output.reg);
      break;
    case MSignExtendInt64::Half:
      masm.movswq(Operand(input.reg), output.reg);
      break;
    case MSignExtendInt64::Word:
      masm.movslq(Operand(input.reg), output.reg);
      break;
  }
}

void CodeGenerator::visitWasmTruncateToInt64(LWasmTruncateToInt64* lir) {
  FloatRegister input = ToFloatRegister(lir->input());
  Register64 output = ToOutRegister64(lir);

  MWasmTruncateToInt64* mir = lir->mir();
  MIRType inputType = mir->input()->type();

  MOZ_ASSERT(inputType == MIRType::Double || inputType == MIRType::Float32);

  auto* ool = new (alloc()) OutOfLineWasmTruncateCheck(mir, input, output);
  addOutOfLineCode(ool, mir);

  FloatRegister temp =
      mir->isUnsigned() ? ToFloatRegister(lir->temp()) : InvalidFloatReg;

  Label* oolEntry = ool->entry();
  Label* oolRejoin = ool->rejoin();
  bool isSaturating = mir->isSaturating();
  if (inputType == MIRType::Double) {
    if (mir->isUnsigned()) {
      masm.wasmTruncateDoubleToUInt64(input, output, isSaturating, oolEntry,
                                      oolRejoin, temp);
    } else {
      masm.wasmTruncateDoubleToInt64(input, output, isSaturating, oolEntry,
                                     oolRejoin, temp);
    }
  } else {
    if (mir->isUnsigned()) {
      masm.wasmTruncateFloat32ToUInt64(input, output, isSaturating, oolEntry,
                                       oolRejoin, temp);
    } else {
      masm.wasmTruncateFloat32ToInt64(input, output, isSaturating, oolEntry,
                                      oolRejoin, temp);
    }
  }
}

void CodeGenerator::visitInt64ToFloatingPoint(LInt64ToFloatingPoint* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  FloatRegister output = ToFloatRegister(lir->output());

  MInt64ToFloatingPoint* mir = lir->mir();
  bool isUnsigned = mir->isUnsigned();

  MIRType outputType = mir->type();
  MOZ_ASSERT(outputType == MIRType::Double || outputType == MIRType::Float32);
  MOZ_ASSERT(isUnsigned == !lir->getTemp(0)->isBogusTemp());

  if (outputType == MIRType::Double) {
    if (isUnsigned) {
      masm.convertUInt64ToDouble(input, output, ToRegister(lir->getTemp(0)));
    } else {
      masm.convertInt64ToDouble(input, output);
    }
  } else {
    if (isUnsigned) {
      masm.convertUInt64ToFloat32(input, output, ToRegister(lir->getTemp(0)));
    } else {
      masm.convertInt64ToFloat32(input, output);
    }
  }
}

void CodeGenerator::visitNotI64(LNotI64* lir) {
  masm.cmpq(Imm32(0), ToRegister(lir->input()));
  masm.emitSet(Assembler::Equal, ToRegister(lir->output()));
}

void CodeGenerator::visitClzI64(LClzI64* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  Register64 output = ToOutRegister64(lir);
  masm.clz64(input, output.reg);
}

void CodeGenerator::visitCtzI64(LCtzI64* lir) {
  Register64 input = ToRegister64(lir->getInt64Operand(0));
  Register64 output = ToOutRegister64(lir);
  masm.ctz64(input, output.reg);
}

void CodeGenerator::visitTestI64AndBranch(LTestI64AndBranch* lir) {
  Register input = ToRegister(lir->input());
  masm.testq(input, input);
  emitBranch(Assembler::NonZero, lir->ifTrue(), lir->ifFalse());
}

#ifdef ENABLE_WASM_SIMD

// These code generators are really x86-shared but some Masm interfaces
// are not yet available on x86.

void CodeGenerator::visitSimd128(LSimd128* ins) {
  const LDefinition* out = ins->getDef(0);
  masm.loadConstantSimd128(ins->getSimd128(), ToFloatRegister(out));
}

void CodeGenerator::visitWasmBitselectSimd128(LWasmBitselectSimd128* ins) {
  FloatRegister lhsDest = ToFloatRegister(ins->lhsDest());
  FloatRegister rhs = ToFloatRegister(ins->rhs());
  FloatRegister control = ToFloatRegister(ins->control());
  FloatRegister temp = ToFloatRegister(ins->temp());
  masm.bitwiseSelectSimd128(control, lhsDest, rhs, lhsDest, temp);
}

void CodeGenerator::visitWasmBinarySimd128(LWasmBinarySimd128* ins) {
  FloatRegister lhsDest = ToFloatRegister(ins->lhsDest());
  FloatRegister rhs = ToFloatRegister(ins->rhs());
  FloatRegister temp1 = ToTempFloatRegisterOrInvalid(ins->getTemp(0));
  FloatRegister temp2 = ToTempFloatRegisterOrInvalid(ins->getTemp(1));

  MOZ_ASSERT(ToFloatRegister(ins->output()) == lhsDest);

  switch (ins->simdOp()) {
    case wasm::SimdOp::V128And:
      masm.bitwiseAndSimd128(rhs, lhsDest);
      break;
    case wasm::SimdOp::V128Or:
      masm.bitwiseOrSimd128(rhs, lhsDest);
      break;
    case wasm::SimdOp::V128Xor:
      masm.bitwiseXorSimd128(rhs, lhsDest);
      break;
    case wasm::SimdOp::V128AndNot:
      // x86/x64 specific: The CPU provides ~A & B.  The operands were swapped
      // during lowering, and we'll compute A & ~B here as desired.
      masm.bitwiseNotAndSimd128(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16AvgrU:
      masm.averageInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8AvgrU:
      masm.averageInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16Add:
      masm.addInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16AddSaturateS:
      masm.addSatInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16AddSaturateU:
      masm.unsignedAddSatInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16Sub:
      masm.subInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16SubSaturateS:
      masm.subSatInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16SubSaturateU:
      masm.unsignedSubSatInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16MinS:
      masm.minInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16MinU:
      masm.unsignedMinInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16MaxS:
      masm.maxInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16MaxU:
      masm.unsignedMaxInt8x16(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8Add:
      masm.addInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8AddSaturateS:
      masm.addSatInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8AddSaturateU:
      masm.unsignedAddSatInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8Sub:
      masm.subInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8SubSaturateS:
      masm.subSatInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8SubSaturateU:
      masm.unsignedSubSatInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8Mul:
      masm.mulInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8MinS:
      masm.minInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8MinU:
      masm.unsignedMinInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8MaxS:
      masm.maxInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8MaxU:
      masm.unsignedMaxInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4Add:
      masm.addInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4Sub:
      masm.subInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4Mul:
      masm.mulInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4MinS:
      masm.minInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4MinU:
      masm.unsignedMinInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4MaxS:
      masm.maxInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4MaxU:
      masm.unsignedMaxInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I64x2Add:
      masm.addInt64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::I64x2Sub:
      masm.subInt64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Add:
      masm.addFloat32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Sub:
      masm.subFloat32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Mul:
      masm.mulFloat32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Div:
      masm.divFloat32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Min:
      masm.minFloat32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Max:
      masm.maxFloat32x4(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::F64x2Add:
      masm.addFloat64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Sub:
      masm.subFloat64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Mul:
      masm.mulFloat64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Div:
      masm.divFloat64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Min:
      masm.minFloat64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Max:
      masm.maxFloat64x2(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::V8x16Swizzle:
      masm.swizzleInt8x16(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I8x16NarrowSI16x8:
      masm.narrowInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16NarrowUI16x8:
      masm.unsignedNarrowInt16x8(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8NarrowSI32x4:
      masm.narrowInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8NarrowUI32x4:
      masm.unsignedNarrowInt32x4(rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16Eq:
      masm.compareInt8x16(Assembler::Equal, rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16Ne:
      masm.compareInt8x16(Assembler::NotEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16LtS:
      masm.compareInt8x16(Assembler::LessThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16GtS:
      masm.compareInt8x16(Assembler::GreaterThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16LeS:
      masm.compareInt8x16(Assembler::LessThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16GeS:
      masm.compareInt8x16(Assembler::GreaterThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I8x16LtU:
      masm.unsignedCompareInt8x16(Assembler::Below, rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I8x16GtU:
      masm.unsignedCompareInt8x16(Assembler::Above, rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I8x16LeU:
      masm.unsignedCompareInt8x16(Assembler::BelowOrEqual, rhs, lhsDest, temp1,
                                  temp2);
      break;
    case wasm::SimdOp::I8x16GeU:
      masm.unsignedCompareInt8x16(Assembler::AboveOrEqual, rhs, lhsDest, temp1,
                                  temp2);
      break;
    case wasm::SimdOp::I16x8Eq:
      masm.compareInt16x8(Assembler::Equal, rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8Ne:
      masm.compareInt16x8(Assembler::NotEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8LtS:
      masm.compareInt16x8(Assembler::LessThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8GtS:
      masm.compareInt16x8(Assembler::GreaterThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8LeS:
      masm.compareInt16x8(Assembler::LessThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8GeS:
      masm.compareInt16x8(Assembler::GreaterThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I16x8LtU:
      masm.unsignedCompareInt16x8(Assembler::Below, rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I16x8GtU:
      masm.unsignedCompareInt16x8(Assembler::Above, rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I16x8LeU:
      masm.unsignedCompareInt16x8(Assembler::BelowOrEqual, rhs, lhsDest, temp1,
                                  temp2);
      break;
    case wasm::SimdOp::I16x8GeU:
      masm.unsignedCompareInt16x8(Assembler::AboveOrEqual, rhs, lhsDest, temp1,
                                  temp2);
      break;
    case wasm::SimdOp::I32x4Eq:
      masm.compareInt32x4(Assembler::Equal, rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4Ne:
      masm.compareInt32x4(Assembler::NotEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4LtS:
      masm.compareInt32x4(Assembler::LessThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4GtS:
      masm.compareInt32x4(Assembler::GreaterThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4LeS:
      masm.compareInt32x4(Assembler::LessThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4GeS:
      masm.compareInt32x4(Assembler::GreaterThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::I32x4LtU:
      masm.unsignedCompareInt32x4(Assembler::Below, rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I32x4GtU:
      masm.unsignedCompareInt32x4(Assembler::Above, rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I32x4LeU:
      masm.unsignedCompareInt32x4(Assembler::BelowOrEqual, rhs, lhsDest, temp1,
                                  temp2);
      break;
    case wasm::SimdOp::I32x4GeU:
      masm.unsignedCompareInt32x4(Assembler::AboveOrEqual, rhs, lhsDest, temp1,
                                  temp2);
      break;
    case wasm::SimdOp::F32x4Eq:
      masm.compareFloat32x4(Assembler::Equal, rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Ne:
      masm.compareFloat32x4(Assembler::NotEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Lt:
      masm.compareFloat32x4(Assembler::LessThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Gt:
      masm.compareFloat32x4(Assembler::GreaterThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Le:
      masm.compareFloat32x4(Assembler::LessThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::F32x4Ge:
      masm.compareFloat32x4(Assembler::GreaterThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Eq:
      masm.compareFloat64x2(Assembler::Equal, rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Ne:
      masm.compareFloat64x2(Assembler::NotEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Lt:
      masm.compareFloat64x2(Assembler::LessThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Gt:
      masm.compareFloat64x2(Assembler::GreaterThan, rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Le:
      masm.compareFloat64x2(Assembler::LessThanOrEqual, rhs, lhsDest);
      break;
    case wasm::SimdOp::F64x2Ge:
      masm.compareFloat64x2(Assembler::GreaterThanOrEqual, rhs, lhsDest);
      break;
    default:
      MOZ_CRASH("Binary SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmI64x2Mul(LWasmI64x2Mul* ins) {
  FloatRegister lhsDest = ToFloatRegister(ins->lhsDest());
  FloatRegister rhs = ToFloatRegister(ins->rhs());
  Register64 temp = ToRegister64(ins->getInt64Temp(0));
  masm.mulInt64x2(rhs, lhsDest, temp);
}

void CodeGenerator::visitWasmVariableShiftSimd128(
    LWasmVariableShiftSimd128* ins) {
  FloatRegister lhsDest = ToFloatRegister(ins->lhsDest());
  Register rhs = ToRegister(ins->rhs());
  Register temp1 = ToTempRegisterOrInvalid(ins->getTemp(0));
  FloatRegister temp2 = ToTempFloatRegisterOrInvalid(ins->getTemp(1));

  MOZ_ASSERT(ToFloatRegister(ins->output()) == lhsDest);

  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16Shl:
      masm.leftShiftInt8x16(rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I8x16ShrS:
      masm.rightShiftInt8x16(rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I8x16ShrU:
      masm.unsignedRightShiftInt8x16(rhs, lhsDest, temp1, temp2);
      break;
    case wasm::SimdOp::I16x8Shl:
      masm.leftShiftInt16x8(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I16x8ShrS:
      masm.rightShiftInt16x8(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I16x8ShrU:
      masm.unsignedRightShiftInt16x8(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I32x4Shl:
      masm.leftShiftInt32x4(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I32x4ShrS:
      masm.rightShiftInt32x4(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I32x4ShrU:
      masm.unsignedRightShiftInt32x4(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I64x2Shl:
      masm.leftShiftInt64x2(rhs, lhsDest, temp1);
      break;
    case wasm::SimdOp::I64x2ShrS:
      masm.rightShiftInt64x2(rhs, lhsDest);
      break;
    case wasm::SimdOp::I64x2ShrU:
      masm.unsignedRightShiftInt64x2(rhs, lhsDest, temp1);
      break;
    default:
      MOZ_CRASH("Shift SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmShuffleSimd128(LWasmShuffleSimd128* ins) {
  FloatRegister lhsDest = ToFloatRegister(ins->lhsDest());
  FloatRegister rhs = ToFloatRegister(ins->rhs());
  SimdConstant control = ins->control();
  FloatRegister temp = ToFloatRegister(ins->temp());
  masm.shuffleInt8x16(reinterpret_cast<const uint8_t*>(control.asInt8x16()),
                      rhs, lhsDest, temp);
}

void CodeGenerator::visitWasmReplaceLaneSimd128(LWasmReplaceLaneSimd128* ins) {
  FloatRegister lhsDest = ToFloatRegister(ins->lhsDest());
  const LAllocation* rhs = ins->rhs();
  uint32_t laneIndex = ins->laneIndex();

  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16ReplaceLane:
      masm.replaceLaneInt8x16(laneIndex, ToRegister(rhs), lhsDest);
      break;
    case wasm::SimdOp::I16x8ReplaceLane:
      masm.replaceLaneInt16x8(laneIndex, ToRegister(rhs), lhsDest);
      break;
    case wasm::SimdOp::I32x4ReplaceLane:
      masm.replaceLaneInt32x4(laneIndex, ToRegister(rhs), lhsDest);
      break;
    case wasm::SimdOp::F32x4ReplaceLane:
      masm.replaceLaneFloat32x4(laneIndex, ToFloatRegister(rhs), lhsDest);
      break;
    case wasm::SimdOp::F64x2ReplaceLane:
      masm.replaceLaneFloat64x2(laneIndex, ToFloatRegister(rhs), lhsDest);
      break;
    default:
      MOZ_CRASH("ReplaceLane SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmReplaceInt64LaneSimd128(
    LWasmReplaceInt64LaneSimd128* ins) {
  MOZ_RELEASE_ASSERT(ins->simdOp() == wasm::SimdOp::I64x2ReplaceLane);
  masm.replaceLaneInt64x2(ins->laneIndex(), ToRegister64(ins->rhs()),
                          ToFloatRegister(ins->lhsDest()));
}

void CodeGenerator::visitWasmScalarToSimd128(LWasmScalarToSimd128* ins) {
  FloatRegister dest = ToFloatRegister(ins->output());

  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16Splat:
      masm.splatX16(ToRegister(ins->src()), dest);
      break;
    case wasm::SimdOp::I16x8Splat:
      masm.splatX8(ToRegister(ins->src()), dest);
      break;
    case wasm::SimdOp::I32x4Splat:
      masm.splatX4(ToRegister(ins->src()), dest);
      break;
    case wasm::SimdOp::F32x4Splat:
      masm.splatX4(ToFloatRegister(ins->src()), dest);
      break;
    case wasm::SimdOp::F64x2Splat:
      masm.splatX2(ToFloatRegister(ins->src()), dest);
      break;
    default:
      MOZ_CRASH("ScalarToSimd128 SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmInt64ToSimd128(LWasmInt64ToSimd128* ins) {
  Register64 src = ToRegister64(ins->src());
  FloatRegister dest = ToFloatRegister(ins->output());

  switch (ins->simdOp()) {
    case wasm::SimdOp::I64x2Splat:
      masm.splatX2(src, dest);
      break;
    case wasm::SimdOp::I16x8LoadS8x8:
      masm.moveGPR64ToDouble(src, dest);
      masm.widenLowInt8x16(dest, dest);
      break;
    case wasm::SimdOp::I16x8LoadU8x8:
      masm.moveGPR64ToDouble(src, dest);
      masm.unsignedWidenLowInt8x16(dest, dest);
      break;
    case wasm::SimdOp::I32x4LoadS16x4:
      masm.moveGPR64ToDouble(src, dest);
      masm.widenLowInt16x8(dest, dest);
      break;
    case wasm::SimdOp::I32x4LoadU16x4:
      masm.moveGPR64ToDouble(src, dest);
      masm.unsignedWidenLowInt16x8(dest, dest);
      break;
    case wasm::SimdOp::I64x2LoadS32x2:
      masm.moveGPR64ToDouble(src, dest);
      masm.widenLowInt32x4(dest, dest);
      break;
    case wasm::SimdOp::I64x2LoadU32x2:
      masm.moveGPR64ToDouble(src, dest);
      masm.unsignedWidenLowInt32x4(dest, dest);
      break;
    default:
      MOZ_CRASH("Int64ToSimd128 SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmUnarySimd128(LWasmUnarySimd128* ins) {
  FloatRegister src = ToFloatRegister(ins->src());
  FloatRegister dest = ToFloatRegister(ins->output());

  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16Neg:
      masm.negInt8x16(src, dest);
      break;
    case wasm::SimdOp::I16x8Neg:
      masm.negInt16x8(src, dest);
      break;
    case wasm::SimdOp::I16x8WidenLowSI8x16:
      masm.widenLowInt8x16(src, dest);
      break;
    case wasm::SimdOp::I16x8WidenHighSI8x16:
      masm.widenHighInt8x16(src, dest);
      break;
    case wasm::SimdOp::I16x8WidenLowUI8x16:
      masm.unsignedWidenLowInt8x16(src, dest);
      break;
    case wasm::SimdOp::I16x8WidenHighUI8x16:
      masm.unsignedWidenHighInt8x16(src, dest);
      break;
    case wasm::SimdOp::I32x4Neg:
      masm.negInt32x4(src, dest);
      break;
    case wasm::SimdOp::I32x4WidenLowSI16x8:
      masm.widenLowInt16x8(src, dest);
      break;
    case wasm::SimdOp::I32x4WidenHighSI16x8:
      masm.widenHighInt16x8(src, dest);
      break;
    case wasm::SimdOp::I32x4WidenLowUI16x8:
      masm.unsignedWidenLowInt16x8(src, dest);
      break;
    case wasm::SimdOp::I32x4WidenHighUI16x8:
      masm.unsignedWidenHighInt16x8(src, dest);
      break;
    case wasm::SimdOp::I32x4TruncSSatF32x4:
      masm.truncSatFloat32x4ToInt32x4(src, dest);
      break;
    case wasm::SimdOp::I32x4TruncUSatF32x4:
      masm.unsignedTruncSatFloat32x4ToInt32x4(src, dest,
                                              ToFloatRegister(ins->temp()));
      break;
    case wasm::SimdOp::I64x2Neg:
      masm.negInt64x2(src, dest);
      break;
    case wasm::SimdOp::F32x4Abs:
      masm.absFloat32x4(src, dest);
      break;
    case wasm::SimdOp::F32x4Neg:
      masm.negFloat32x4(src, dest);
      break;
    case wasm::SimdOp::F32x4Sqrt:
      masm.sqrtFloat32x4(src, dest);
      break;
    case wasm::SimdOp::F32x4ConvertSI32x4:
      masm.convertInt32x4ToFloat32x4(src, dest);
      break;
    case wasm::SimdOp::F32x4ConvertUI32x4:
      masm.unsignedConvertInt32x4ToFloat32x4(src, dest);
      break;
    case wasm::SimdOp::F64x2Abs:
      masm.absFloat64x2(src, dest);
      break;
    case wasm::SimdOp::F64x2Neg:
      masm.negFloat64x2(src, dest);
      break;
    case wasm::SimdOp::F64x2Sqrt:
      masm.sqrtFloat64x2(src, dest);
      break;
    case wasm::SimdOp::V128Not:
      masm.bitwiseNotSimd128(src, dest);
      break;
    case wasm::SimdOp::I8x16Abs:
      masm.absInt8x16(src, dest);
      break;
    case wasm::SimdOp::I16x8Abs:
      masm.absInt16x8(src, dest);
      break;
    case wasm::SimdOp::I32x4Abs:
      masm.absInt32x4(src, dest);
      break;
    default:
      MOZ_CRASH("Unary SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmReduceSimd128(LWasmReduceSimd128* ins) {
  FloatRegister src = ToFloatRegister(ins->src());
  const LDefinition* dest = ins->output();
  uint32_t imm = ins->imm();

  switch (ins->simdOp()) {
    case wasm::SimdOp::I8x16AnyTrue:
    case wasm::SimdOp::I16x8AnyTrue:
    case wasm::SimdOp::I32x4AnyTrue:
      masm.anyTrueSimd128(src, ToRegister(dest));
      break;
    case wasm::SimdOp::I8x16AllTrue:
      masm.allTrueInt8x16(src, ToRegister(dest));
      break;
    case wasm::SimdOp::I16x8AllTrue:
      masm.allTrueInt16x8(src, ToRegister(dest));
      break;
    case wasm::SimdOp::I32x4AllTrue:
      masm.allTrueInt32x4(src, ToRegister(dest));
      break;
    case wasm::SimdOp::I8x16ExtractLaneS:
      masm.extractLaneInt8x16(imm, src, ToRegister(dest));
      break;
    case wasm::SimdOp::I8x16ExtractLaneU:
      masm.unsignedExtractLaneInt8x16(imm, src, ToRegister(dest));
      break;
    case wasm::SimdOp::I16x8ExtractLaneS:
      masm.extractLaneInt16x8(imm, src, ToRegister(dest));
      break;
    case wasm::SimdOp::I16x8ExtractLaneU:
      masm.unsignedExtractLaneInt16x8(imm, src, ToRegister(dest));
      break;
    case wasm::SimdOp::I32x4ExtractLane:
      masm.extractLaneInt32x4(imm, src, ToRegister(dest));
      break;
    case wasm::SimdOp::F32x4ExtractLane:
      masm.extractLaneFloat32x4(imm, src, ToFloatRegister(dest));
      break;
    case wasm::SimdOp::F64x2ExtractLane:
      masm.extractLaneFloat64x2(imm, src, ToFloatRegister(dest));
      break;
    default:
      MOZ_CRASH("Reduce SimdOp not implemented");
  }
}

void CodeGenerator::visitWasmReduceSimd128ToInt64(
    LWasmReduceSimd128ToInt64* ins) {
  FloatRegister src = ToFloatRegister(ins->src());
  Register64 dest = ToOutRegister64(ins);
  uint32_t imm = ins->imm();

  switch (ins->simdOp()) {
    case wasm::SimdOp::I64x2ExtractLane:
      masm.extractLaneInt64x2(imm, src, dest);
      break;
    default:
      MOZ_CRASH("Reduce SimdOp not implemented");
  }
}

#endif  // ENABLE_WASM_SIMD
