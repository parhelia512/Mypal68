/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_CodeGenerator_shared_h
#define jit_shared_CodeGenerator_shared_h

#include "mozilla/Alignment.h"

#include <utility>

#include "jit/InlineScriptTree.h"
#include "jit/JitcodeMap.h"
#include "jit/LIR.h"
#include "jit/MacroAssembler.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/SafepointIndex.h"
#include "jit/Safepoints.h"
#include "jit/Snapshots.h"
#include "vm/TraceLoggingTypes.h"

namespace js {
namespace jit {

class OutOfLineCode;
class CodeGenerator;
class MacroAssembler;
class IonIC;

class OutOfLineTruncateSlow;

struct ReciprocalMulConstants {
  int64_t multiplier;
  int32_t shiftAmount;
};

class CodeGeneratorShared : public LElementVisitor {
  js::Vector<OutOfLineCode*, 0, SystemAllocPolicy> outOfLineCode_;

  MacroAssembler& ensureMasm(MacroAssembler* masm);
  mozilla::Maybe<IonHeapMacroAssembler> maybeMasm_;

  bool useWasmStackArgumentAbi_;

 public:
  MacroAssembler& masm;

 protected:
  MIRGenerator* gen;
  LIRGraph& graph;
  LBlock* current;
  SnapshotWriter snapshots_;
  RecoverWriter recovers_;
  mozilla::Maybe<TrampolinePtr> deoptTable_;
#ifdef DEBUG
  uint32_t pushedArgs_;
#endif
  uint32_t lastOsiPointOffset_;
  SafepointWriter safepoints_;
  Label invalidate_;
  CodeOffset invalidateEpilogueData_;

  // Label for the common return path.
  NonAssertingLabel returnLabel_;

  js::Vector<CodegenSafepointIndex, 0, SystemAllocPolicy> safepointIndices_;
  js::Vector<OsiIndex, 0, SystemAllocPolicy> osiIndices_;

  // Mapping from bailout table ID to an offset in the snapshot buffer.
  js::Vector<SnapshotOffset, 0, SystemAllocPolicy> bailouts_;

  // Allocated data space needed at runtime.
  js::Vector<uint8_t, 0, SystemAllocPolicy> runtimeData_;

  // Vector mapping each IC index to its offset in runtimeData_.
  js::Vector<uint32_t, 0, SystemAllocPolicy> icList_;

  // IC data we need at compile-time. Discarded after creating the IonScript.
  struct CompileTimeICInfo {
    CodeOffset icOffsetForJump;
    CodeOffset icOffsetForPush;
  };
  js::Vector<CompileTimeICInfo, 0, SystemAllocPolicy> icInfo_;

#ifdef JS_TRACE_LOGGING
  struct PatchableTLEvent {
    CodeOffset offset;
    const char* event;
    PatchableTLEvent(CodeOffset offset, const char* event)
        : offset(offset), event(event) {}
  };
  js::Vector<PatchableTLEvent, 0, SystemAllocPolicy> patchableTLEvents_;
  js::Vector<CodeOffset, 0, SystemAllocPolicy> patchableTLScripts_;
#endif

 protected:
  js::Vector<NativeToBytecode, 0, SystemAllocPolicy> nativeToBytecodeList_;
  uint8_t* nativeToBytecodeMap_;
  uint32_t nativeToBytecodeMapSize_;
  uint32_t nativeToBytecodeTableOffset_;
  uint32_t nativeToBytecodeNumRegions_;

  JSScript** nativeToBytecodeScriptList_;
  uint32_t nativeToBytecodeScriptListLength_;

  bool isProfilerInstrumentationEnabled() {
    return gen->isProfilerInstrumentationEnabled();
  }

  gc::InitialHeap initialStringHeap() const { return gen->initialStringHeap(); }
  gc::InitialHeap initialBigIntHeap() const { return gen->initialBigIntHeap(); }

 protected:
  // The offset of the first instruction of the OSR entry block from the
  // beginning of the code buffer.
  mozilla::Maybe<size_t> osrEntryOffset_ = {};

  TempAllocator& alloc() const { return graph.mir().alloc(); }

  void setOsrEntryOffset(size_t offset) { osrEntryOffset_.emplace(offset); }

  size_t getOsrEntryOffset() const {
    MOZ_RELEASE_ASSERT(osrEntryOffset_.isSome());
    return *osrEntryOffset_;
  }

  typedef js::Vector<CodegenSafepointIndex, 8, SystemAllocPolicy>
      SafepointIndices;

 protected:
#ifdef CHECK_OSIPOINT_REGISTERS
  // See JitOptions.checkOsiPointRegisters. We set this here to avoid
  // races when enableOsiPointRegisterChecks is called while we're generating
  // code off-thread.
  bool checkOsiPointRegisters;
#endif

  // The initial size of the frame in bytes. These are bytes beyond the
  // constant header present for every Ion frame, used for pre-determined
  // spills.
  int32_t frameDepth_;

  // Frame class this frame's size falls into (see IonFrame.h).
  FrameSizeClass frameClass_;

  // For arguments to the current function.
  inline int32_t ArgToStackOffset(int32_t slot) const;

  inline int32_t SlotToStackOffset(int32_t slot) const;
  inline int32_t StackOffsetToSlot(int32_t offset) const;

  // For argument construction for calls. Argslots are Value-sized.
  inline int32_t StackOffsetOfPassedArg(int32_t slot) const;

  inline int32_t ToStackOffset(LAllocation a) const;
  inline int32_t ToStackOffset(const LAllocation* a) const;

  inline Address ToAddress(const LAllocation& a) const;
  inline Address ToAddress(const LAllocation* a) const;

  static inline Address ToAddress(Register elements, const LAllocation* index,
                                  Scalar::Type type,
                                  int32_t offsetAdjustment = 0);

  // Returns the offset from FP to address incoming stack arguments
  // when we use wasm stack argument abi (useWasmStackArgumentAbi()).
  inline int32_t ToFramePointerOffset(LAllocation a) const;
  inline int32_t ToFramePointerOffset(const LAllocation* a) const;

  uint32_t frameSize() const {
    return frameClass_ == FrameSizeClass::None() ? frameDepth_
                                                 : frameClass_.frameSize();
  }

 protected:
  bool addNativeToBytecodeEntry(const BytecodeSite* site);
  void dumpNativeToBytecodeEntries();
  void dumpNativeToBytecodeEntry(uint32_t idx);

  void setUseWasmStackArgumentAbi() { useWasmStackArgumentAbi_ = true; }

  bool useWasmStackArgumentAbi() const { return useWasmStackArgumentAbi_; }

 public:
  MIRGenerator& mirGen() const { return *gen; }

  // When appending to runtimeData_, the vector might realloc, leaving pointers
  // int the origianl vector stale and unusable. DataPtr acts like a pointer,
  // but allows safety in the face of potentially realloc'ing vector appends.
  friend class DataPtr;
  template <typename T>
  class DataPtr {
    CodeGeneratorShared* cg_;
    size_t index_;

    T* lookup() { return reinterpret_cast<T*>(&cg_->runtimeData_[index_]); }

   public:
    DataPtr(CodeGeneratorShared* cg, size_t index) : cg_(cg), index_(index) {}

    T* operator->() { return lookup(); }
    T* operator*() { return lookup(); }
  };

 protected:
  [[nodiscard]] bool allocateData(size_t size, size_t* offset) {
    MOZ_ASSERT(size % sizeof(void*) == 0);
    *offset = runtimeData_.length();
    masm.propagateOOM(runtimeData_.appendN(0, size));
    return !masm.oom();
  }

  template <typename T>
  inline size_t allocateIC(const T& cache) {
    static_assert(std::is_base_of_v<IonIC, T>, "T must inherit from IonIC");
    size_t index;
    masm.propagateOOM(
        allocateData(sizeof(mozilla::AlignedStorage2<T>), &index));
    masm.propagateOOM(icList_.append(index));
    masm.propagateOOM(icInfo_.append(CompileTimeICInfo()));
    if (masm.oom()) {
      return SIZE_MAX;
    }
    // Use the copy constructor on the allocated space.
    MOZ_ASSERT(index == icList_.back());
    new (&runtimeData_[index]) T(cache);
    return index;
  }

 protected:
  // Encodes an LSnapshot into the compressed snapshot buffer.
  void encode(LRecoverInfo* recover);
  void encode(LSnapshot* snapshot);
  void encodeAllocation(LSnapshot* snapshot, MDefinition* def,
                        uint32_t* startIndex);

  // Attempts to assign a BailoutId to a snapshot, if one isn't already set.
  // If the bailout table is full, this returns false, which is not a fatal
  // error (the code generator may use a slower bailout mechanism).
  bool assignBailoutId(LSnapshot* snapshot);

  // Encode all encountered safepoints in CG-order, and resolve |indices| for
  // safepoint offsets.
  bool encodeSafepoints();

  // Fixup offsets of native-to-bytecode map.
  bool createNativeToBytecodeScriptList(JSContext* cx);
  bool generateCompactNativeToBytecodeMap(JSContext* cx, JitCode* code);
  void verifyCompactNativeToBytecodeMap(JitCode* code);

  // Mark the safepoint on |ins| as corresponding to the current assembler
  // location. The location should be just after a call.
  void markSafepoint(LInstruction* ins);
  void markSafepointAt(uint32_t offset, LInstruction* ins);

  // Mark the OSI point |ins| as corresponding to the current
  // assembler location inside the |osiIndices_|. Return the assembler
  // location for the OSI point return location.
  uint32_t markOsiPoint(LOsiPoint* ins);

  // Ensure that there is enough room between the last OSI point and the
  // current instruction, such that:
  //  (1) Invalidation will not overwrite the current instruction, and
  //  (2) Overwriting the current instruction will not overwrite
  //      an invalidation marker.
  void ensureOsiSpace();

  OutOfLineCode* oolTruncateDouble(
      FloatRegister src, Register dest, MInstruction* mir,
      wasm::BytecodeOffset callOffset = wasm::BytecodeOffset(),
      bool preserveTls = false);
  void emitTruncateDouble(FloatRegister src, Register dest, MInstruction* mir);
  void emitTruncateFloat32(FloatRegister src, Register dest, MInstruction* mir);

  void emitPreBarrier(Register elements, const LAllocation* index);
  void emitPreBarrier(Address address);

  // We don't emit code for trivial blocks, so if we want to branch to the
  // given block, and it's trivial, return the ultimate block we should
  // actually branch directly to.
  MBasicBlock* skipTrivialBlocks(MBasicBlock* block) {
    while (block->lir()->isTrivial()) {
      LGoto* ins = block->lir()->rbegin()->toGoto();
      MOZ_ASSERT(ins->numSuccessors() == 1);
      block = ins->getSuccessor(0);
    }
    return block;
  }

  // Test whether the given block can be reached via fallthrough from the
  // current block.
  inline bool isNextBlock(LBlock* block) {
    uint32_t target = skipTrivialBlocks(block->mir())->id();
    uint32_t i = current->mir()->id() + 1;
    if (target < i) {
      return false;
    }
    // Trivial blocks can be crossed via fallthrough.
    for (; i != target; ++i) {
      if (!graph.getBlock(i)->isTrivial()) {
        return false;
      }
    }
    return true;
  }

 protected:
  // Save and restore all volatile registers to/from the stack, excluding the
  // specified register(s), before a function call made using callWithABI and
  // after storing the function call's return value to an output register.
  // (The only registers that don't need to be saved/restored are 1) the
  // temporary register used to store the return value of the function call,
  // if there is one [otherwise that stored value would be overwritten]; and
  // 2) temporary registers whose values aren't needed in the rest of the LIR
  // instruction [this is purely an optimization].  All other volatiles must
  // be saved and restored in case future LIR instructions need those values.)
  void saveVolatile(Register output) {
    LiveRegisterSet regs(RegisterSet::Volatile());
    regs.takeUnchecked(output);
    masm.PushRegsInMask(regs);
  }
  void restoreVolatile(Register output) {
    LiveRegisterSet regs(RegisterSet::Volatile());
    regs.takeUnchecked(output);
    masm.PopRegsInMask(regs);
  }
  void saveVolatile(FloatRegister output) {
    LiveRegisterSet regs(RegisterSet::Volatile());
    regs.takeUnchecked(output);
    masm.PushRegsInMask(regs);
  }
  void restoreVolatile(FloatRegister output) {
    LiveRegisterSet regs(RegisterSet::Volatile());
    regs.takeUnchecked(output);
    masm.PopRegsInMask(regs);
  }
  void saveVolatile(LiveRegisterSet temps) {
    masm.PushRegsInMask(LiveRegisterSet(RegisterSet::VolatileNot(temps.set())));
  }
  void restoreVolatile(LiveRegisterSet temps) {
    masm.PopRegsInMask(LiveRegisterSet(RegisterSet::VolatileNot(temps.set())));
  }
  void saveVolatile() {
    masm.PushRegsInMask(LiveRegisterSet(RegisterSet::Volatile()));
  }
  void restoreVolatile() {
    masm.PopRegsInMask(LiveRegisterSet(RegisterSet::Volatile()));
  }

  // These functions have to be called before and after any callVM and before
  // any modifications of the stack.  Modification of the stack made after
  // these calls should update the framePushed variable, needed by the exit
  // frame produced by callVM.
  inline void saveLive(LInstruction* ins);
  inline void restoreLive(LInstruction* ins);
  inline void restoreLiveIgnore(LInstruction* ins, LiveRegisterSet reg);

  // Get/save/restore all registers that are both live and volatile.
  inline LiveRegisterSet liveVolatileRegs(LInstruction* ins);
  inline void saveLiveVolatile(LInstruction* ins);
  inline void restoreLiveVolatile(LInstruction* ins);

 public:
  template <typename T>
  void pushArg(const T& t) {
    masm.Push(t);
#ifdef DEBUG
    pushedArgs_++;
#endif
  }

  void pushArg(jsid id, Register temp) {
    masm.Push(id, temp);
#ifdef DEBUG
    pushedArgs_++;
#endif
  }

  template <typename T>
  CodeOffset pushArgWithPatch(const T& t) {
#ifdef DEBUG
    pushedArgs_++;
#endif
    return masm.PushWithPatch(t);
  }

  void storePointerResultTo(Register reg) { masm.storeCallPointerResult(reg); }

  void storeFloatResultTo(FloatRegister reg) { masm.storeCallFloatResult(reg); }

  template <typename T>
  void storeResultValueTo(const T& t) {
    masm.storeCallResultValue(t);
  }

 protected:
  void addIC(LInstruction* lir, size_t cacheIndex);

  ReciprocalMulConstants computeDivisionConstants(uint32_t d, int maxLog);

 protected:
  bool generatePrologue();
  bool generateEpilogue();

  void addOutOfLineCode(OutOfLineCode* code, const MInstruction* mir);
  void addOutOfLineCode(OutOfLineCode* code, const BytecodeSite* site);
  bool generateOutOfLineCode();

  Label* getJumpLabelForBranch(MBasicBlock* block);

  // Generate a jump to the start of the specified block. Use this in place of
  // jumping directly to mir->lir()->label(), or use getJumpLabelForBranch()
  // if a label to use directly is needed.
  void jumpToBlock(MBasicBlock* mir);

// This function is not used for MIPS. MIPS has branchToBlock.
#if !defined(JS_CODEGEN_MIPS32) && !defined(JS_CODEGEN_MIPS64)
  void jumpToBlock(MBasicBlock* mir, Assembler::Condition cond);
#endif

 private:
  void generateInvalidateEpilogue();

 public:
  CodeGeneratorShared(MIRGenerator* gen, LIRGraph* graph, MacroAssembler* masm);

 public:
  void visitOutOfLineTruncateSlow(OutOfLineTruncateSlow* ool);

  bool omitOverRecursedCheck() const;

#ifdef JS_TRACE_LOGGING
 protected:
  void emitTracelogScript(bool isStart);
  void emitTracelogTree(bool isStart, uint32_t textId);
  void emitTracelogTree(bool isStart, const char* text,
                        TraceLoggerTextId enabledTextId);
#endif

 public:
#ifdef JS_TRACE_LOGGING
  void emitTracelogScriptStart() { emitTracelogScript(/* isStart =*/true); }
  void emitTracelogScriptStop() { emitTracelogScript(/* isStart =*/false); }
  void emitTracelogStartEvent(uint32_t textId) {
    emitTracelogTree(/* isStart =*/true, textId);
  }
  void emitTracelogStopEvent(uint32_t textId) {
    emitTracelogTree(/* isStart =*/false, textId);
  }
  // Log an arbitrary text. The TraceloggerTextId is used to toggle the
  // logging on and off.
  // Note: the text is not copied and need to be kept alive until linking.
  void emitTracelogStartEvent(const char* text,
                              TraceLoggerTextId enabledTextId) {
    emitTracelogTree(/* isStart =*/true, text, enabledTextId);
  }
  void emitTracelogStopEvent(const char* text,
                             TraceLoggerTextId enabledTextId) {
    emitTracelogTree(/* isStart =*/false, text, enabledTextId);
  }
  void emitTracelogIonStart() {
    emitTracelogScriptStart();
    emitTracelogStartEvent(TraceLogger_IonMonkey);
  }
  void emitTracelogIonStop() {
    emitTracelogStopEvent(TraceLogger_IonMonkey);
    emitTracelogScriptStop();
  }
#else
  void emitTracelogScriptStart() {}
  void emitTracelogScriptStop() {}
  void emitTracelogStartEvent(uint32_t textId) {}
  void emitTracelogStopEvent(uint32_t textId) {}
  void emitTracelogStartEvent(const char* text,
                              TraceLoggerTextId enabledTextId) {}
  void emitTracelogStopEvent(const char* text,
                             TraceLoggerTextId enabledTextId) {}
  void emitTracelogIonStart() {}
  void emitTracelogIonStop() {}
#endif

  bool isGlobalObject(JSObject* object);
};

// An out-of-line path is generated at the end of the function.
class OutOfLineCode : public TempObject {
  Label entry_;
  Label rejoin_;
  uint32_t framePushed_;
  const BytecodeSite* site_;

 public:
  OutOfLineCode() : framePushed_(0), site_() {}

  virtual void generate(CodeGeneratorShared* codegen) = 0;

  Label* entry() { return &entry_; }
  virtual void bind(MacroAssembler* masm) { masm->bind(entry()); }
  Label* rejoin() { return &rejoin_; }
  void setFramePushed(uint32_t framePushed) { framePushed_ = framePushed; }
  uint32_t framePushed() const { return framePushed_; }
  void setBytecodeSite(const BytecodeSite* site) { site_ = site; }
  const BytecodeSite* bytecodeSite() const { return site_; }
};

// For OOL paths that want a specific-typed code generator.
template <typename T>
class OutOfLineCodeBase : public OutOfLineCode {
 public:
  virtual void generate(CodeGeneratorShared* codegen) override {
    accept(static_cast<T*>(codegen));
  }

 public:
  virtual void accept(T* codegen) = 0;
};

template <class CodeGen>
class OutOfLineWasmTruncateCheckBase : public OutOfLineCodeBase<CodeGen> {
  MIRType fromType_;
  MIRType toType_;
  FloatRegister input_;
  Register output_;
  Register64 output64_;
  TruncFlags flags_;
  wasm::BytecodeOffset bytecodeOffset_;

 public:
  OutOfLineWasmTruncateCheckBase(MWasmTruncateToInt32* mir, FloatRegister input,
                                 Register output)
      : fromType_(mir->input()->type()),
        toType_(MIRType::Int32),
        input_(input),
        output_(output),
        output64_(Register64::Invalid()),
        flags_(mir->flags()),
        bytecodeOffset_(mir->bytecodeOffset()) {}

  OutOfLineWasmTruncateCheckBase(MWasmBuiltinTruncateToInt64* mir,
                                 FloatRegister input, Register64 output)
      : fromType_(mir->input()->type()),
        toType_(MIRType::Int64),
        input_(input),
        output_(Register::Invalid()),
        output64_(output),
        flags_(mir->flags()),
        bytecodeOffset_(mir->bytecodeOffset()) {}

  OutOfLineWasmTruncateCheckBase(MWasmTruncateToInt64* mir, FloatRegister input,
                                 Register64 output)
      : fromType_(mir->input()->type()),
        toType_(MIRType::Int64),
        input_(input),
        output_(Register::Invalid()),
        output64_(output),
        flags_(mir->flags()),
        bytecodeOffset_(mir->bytecodeOffset()) {}

  void accept(CodeGen* codegen) override {
    codegen->visitOutOfLineWasmTruncateCheck(this);
  }

  FloatRegister input() const { return input_; }
  Register output() const { return output_; }
  Register64 output64() const { return output64_; }
  MIRType toType() const { return toType_; }
  MIRType fromType() const { return fromType_; }
  bool isUnsigned() const { return flags_ & TRUNC_UNSIGNED; }
  bool isSaturating() const { return flags_ & TRUNC_SATURATING; }
  TruncFlags flags() const { return flags_; }
  wasm::BytecodeOffset bytecodeOffset() const { return bytecodeOffset_; }
};

}  // namespace jit
}  // namespace js

#endif /* jit_shared_CodeGenerator_shared_h */
