/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/SharedContext.h"

#include "mozilla/RefPtr.h"

#include "frontend/AbstractScopePtr.h"
#include "frontend/FunctionSyntaxKind.h"  // FunctionSyntaxKind
#include "frontend/ModuleSharedContext.h"
#include "vm/FunctionFlags.h"          // js::FunctionFlags
#include "vm/GeneratorAndAsyncKind.h"  // js::GeneratorKind, js::FunctionAsyncKind
#include "vm/JSScript.h"  // js::FillImmutableFlagsFromCompileOptionsForTopLevel, js::FillImmutableFlagsFromCompileOptionsForFunction
#include "vm/StencilEnums.h"  // ImmutableScriptFlagsEnum
#include "wasm/AsmJS.h"
#include "wasm/WasmModule.h"

#include "frontend/ParseContext-inl.h"
#include "vm/EnvironmentObject-inl.h"

namespace js {
namespace frontend {

SharedContext::SharedContext(JSContext* cx, Kind kind,
                             const JS::ReadOnlyCompileOptions& options,
                             Directives directives, SourceExtent extent)
    : cx_(cx),
      extent_(extent),
      allowNewTarget_(false),
      allowSuperProperty_(false),
      allowSuperCall_(false),
      allowArguments_(true),
      inWith_(false),
      inClass_(false),
      localStrict(false),
      hasExplicitUseStrict_(false),
      isScriptExtraFieldCopiedToStencil(false) {
  // Compute the script kind "input" flags.
  if (kind == Kind::FunctionBox) {
    setFlag(ImmutableFlags::IsFunction);
  } else if (kind == Kind::Module) {
    MOZ_ASSERT(!options.nonSyntacticScope);
    setFlag(ImmutableFlags::IsModule);
  } else if (kind == Kind::Eval) {
    setFlag(ImmutableFlags::IsForEval);
  } else {
    MOZ_ASSERT(kind == Kind::Global);
  }

  // Initialize the transitive "input" flags. These are applied to all
  // SharedContext in this compilation and generally cannot be determined from
  // the source text alone.
  if (isTopLevelContext()) {
    js::FillImmutableFlagsFromCompileOptionsForTopLevel(options,
                                                        immutableFlags_);
  } else {
    js::FillImmutableFlagsFromCompileOptionsForFunction(options,
                                                        immutableFlags_);
  }

  // Initialize the strict flag. This may be updated by the parser as we observe
  // further directives in the body.
  setFlag(ImmutableFlags::Strict, directives.strict());
}

GlobalSharedContext::GlobalSharedContext(
    JSContext* cx, ScopeKind scopeKind,
    const JS::ReadOnlyCompileOptions& options, Directives directives,
    SourceExtent extent)
    : SharedContext(cx, Kind::Global, options, directives, extent),
      scopeKind_(scopeKind),
      bindings(nullptr) {
  MOZ_ASSERT(scopeKind == ScopeKind::Global ||
             scopeKind == ScopeKind::NonSyntactic);
  MOZ_ASSERT(thisBinding_ == ThisBinding::Global);
}

EvalSharedContext::EvalSharedContext(JSContext* cx,
                                     CompilationState& compilationState,
                                     SourceExtent extent)
    : SharedContext(cx, Kind::Eval, compilationState.input.options,
                    compilationState.directives, extent),
      bindings(nullptr) {
  // Eval inherits syntax and binding rules from enclosing environment.
  allowNewTarget_ = compilationState.scopeContext.allowNewTarget;
  allowSuperProperty_ = compilationState.scopeContext.allowSuperProperty;
  allowSuperCall_ = compilationState.scopeContext.allowSuperCall;
  allowArguments_ = compilationState.scopeContext.allowArguments;
  thisBinding_ = compilationState.scopeContext.thisBinding;
  inWith_ = compilationState.scopeContext.inWith;
}

SuspendableContext::SuspendableContext(
    JSContext* cx, Kind kind, const JS::ReadOnlyCompileOptions& options,
    Directives directives, SourceExtent extent, bool isGenerator, bool isAsync)
    : SharedContext(cx, kind, options, directives, extent) {
  setFlag(ImmutableFlags::IsGenerator, isGenerator);
  setFlag(ImmutableFlags::IsAsync, isAsync);
}

FunctionBox::FunctionBox(JSContext* cx, SourceExtent extent,
                         CompilationState& compilationState,
                         Directives directives, GeneratorKind generatorKind,
                         FunctionAsyncKind asyncKind, bool isInitialCompilation,
                         TaggedParserAtomIndex atom, FunctionFlags flags,
                         ScriptIndex index)
    : SuspendableContext(cx, Kind::FunctionBox, compilationState.input.options,
                         directives, extent,
                         generatorKind == GeneratorKind::Generator,
                         asyncKind == FunctionAsyncKind::AsyncFunction),
      compilationState_(compilationState),
      atom_(atom),
      funcDataIndex_(index),
      flags_(FunctionFlags::clearMutableflags(flags)),
      emitBytecode(false),
      wasEmittedByEnclosingScript_(false),
      isAnnexB(false),
      useAsm(false),
      hasParameterExprs(false),
      hasDestructuringArgs(false),
      hasDuplicateParameters(false),
      hasExprBody_(false),
      allowReturn_(true),
      isFunctionFieldCopiedToStencil(false),
      isInitialCompilation(isInitialCompilation),
      isStandalone(false) {}

void FunctionBox::initFromLazyFunction(JSFunction* fun,
                                       ScopeContext& scopeContext,
                                       FunctionFlags flags,
                                       FunctionSyntaxKind kind) {
  initFromLazyFunctionShared(fun);
  initStandaloneOrLazy(scopeContext, flags, kind);
}

void FunctionBox::initFromLazyFunctionToSkip(JSFunction* fun) {
  initFromLazyFunctionShared(fun);
}

void FunctionBox::initFromLazyFunctionShared(JSFunction* fun) {
  BaseScript* lazy = fun->baseScript();
  immutableFlags_ = lazy->immutableFlags();
  extent_ = lazy->extent();
}

void FunctionBox::initFromScriptStencilExtra(const ScriptStencilExtra& extra) {
  immutableFlags_ = extra.immutableFlags;
  extent_ = extra.extent;
}

void FunctionBox::initWithEnclosingParseContext(ParseContext* enclosing,
                                                FunctionFlags flags,
                                                FunctionSyntaxKind kind) {
  SharedContext* sc = enclosing->sc();

  // HasModuleGoal and useAsm are inherited from enclosing context.
  useAsm = sc->isFunctionBox() && sc->asFunctionBox()->useAsmOrInsideUseAsm();
  setHasModuleGoal(sc->hasModuleGoal());

  // Arrow functions don't have their own `this` binding.
  if (flags.isArrow()) {
    allowNewTarget_ = sc->allowNewTarget();
    allowSuperProperty_ = sc->allowSuperProperty();
    allowSuperCall_ = sc->allowSuperCall();
    allowArguments_ = sc->allowArguments();
    thisBinding_ = sc->thisBinding();
  } else {
    if (IsConstructorKind(kind)) {
      // Record this function into the enclosing class statement so that
      // finishClassConstructor can final processing. Due to aborted syntax
      // parses (eg, because of asm.js), this may have already been set with an
      // early FunctionBox. In that case, the FunctionNode should still match.
      auto classStmt =
          enclosing->findInnermostStatement<ParseContext::ClassStatement>();
      MOZ_ASSERT(classStmt);
      MOZ_ASSERT(classStmt->constructorBox == nullptr ||
                 classStmt->constructorBox->functionNode == this->functionNode);
      classStmt->constructorBox = this;
    }

    allowNewTarget_ = true;
    allowSuperProperty_ = flags.allowSuperProperty();

    if (kind == FunctionSyntaxKind::DerivedClassConstructor) {
      setDerivedClassConstructor();
      allowSuperCall_ = true;
      thisBinding_ = ThisBinding::DerivedConstructor;
    } else {
      thisBinding_ = ThisBinding::Function;
    }

    if (kind == FunctionSyntaxKind::FieldInitializer ||
        kind == FunctionSyntaxKind::StaticClassBlock) {
      setSyntheticFunction();
      allowArguments_ = false;
      if (kind == FunctionSyntaxKind::StaticClassBlock) {
        allowSuperCall_ = false;
        allowReturn_ = false;
      }
    }
  }

  if (sc->inWith()) {
    inWith_ = true;
  } else {
    auto isWith = [](ParseContext::Statement* stmt) {
      return stmt->kind() == StatementKind::With;
    };

    inWith_ = enclosing->findInnermostStatement(isWith);
  }

  if (sc->inClass()) {
    inClass_ = true;
  } else {
    auto isClass = [](ParseContext::Statement* stmt) {
      return stmt->kind() == StatementKind::Class;
    };

    inClass_ = enclosing->findInnermostStatement(isClass);
  }
}

void FunctionBox::initStandalone(ScopeContext& scopeContext,
                                 FunctionFlags flags, FunctionSyntaxKind kind) {
  initStandaloneOrLazy(scopeContext, flags, kind);

  isStandalone = true;
}

void FunctionBox::initStandaloneOrLazy(ScopeContext& scopeContext,
                                       FunctionFlags flags,
                                       FunctionSyntaxKind kind) {
  if (flags.isArrow()) {
    allowNewTarget_ = scopeContext.allowNewTarget;
    allowSuperProperty_ = scopeContext.allowSuperProperty;
    allowSuperCall_ = scopeContext.allowSuperCall;
    allowArguments_ = scopeContext.allowArguments;
    thisBinding_ = scopeContext.thisBinding;
  } else {
    allowNewTarget_ = true;
    allowSuperProperty_ = flags.allowSuperProperty();

    if (kind == FunctionSyntaxKind::DerivedClassConstructor) {
      setDerivedClassConstructor();
      allowSuperCall_ = true;
      thisBinding_ = ThisBinding::DerivedConstructor;
    } else {
      thisBinding_ = ThisBinding::Function;
    }

    if (kind == FunctionSyntaxKind::FieldInitializer) {
      setSyntheticFunction();
      allowArguments_ = false;
    }
  }

  inWith_ = scopeContext.inWith;
  inClass_ = scopeContext.inClass;
}

void FunctionBox::setEnclosingScopeForInnerLazyFunction(ScopeIndex scopeIndex) {
  // For lazy functions inside a function which is being compiled, we cache
  // the incomplete scope object while compiling, and store it to the
  // BaseScript once the enclosing script successfully finishes compilation
  // in FunctionBox::finish.
  MOZ_ASSERT(enclosingScopeIndex_.isNothing());
  enclosingScopeIndex_ = mozilla::Some(scopeIndex);
  if (isFunctionFieldCopiedToStencil) {
    copyUpdatedEnclosingScopeIndex();
  }
}

bool FunctionBox::setAsmJSModule(const JS::WasmModule* module) {
  MOZ_ASSERT(!isFunctionFieldCopiedToStencil);

  MOZ_ASSERT(flags_.kind() == FunctionFlags::NormalFunction);

  // Update flags we will use to allocate the JSFunction.
  flags_.clearBaseScript();
  flags_.setIsExtended();
  flags_.setKind(FunctionFlags::AsmJS);

  if (!compilationState_.asmJS) {
    compilationState_.asmJS = cx_->new_<StencilAsmJSContainer>();
    if (!compilationState_.asmJS) {
      return false;
    }
  }

  if (!compilationState_.asmJS->moduleMap.putNew(index(), module)) {
    js::ReportOutOfMemory(cx_);
    return false;
  }
  return true;
}

ModuleSharedContext::ModuleSharedContext(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    ModuleBuilder& builder, SourceExtent extent)
    : SuspendableContext(cx, Kind::Module, options, Directives(true), extent,
                         /* isGenerator = */ false,
                         /* isAsync = */ false),
      bindings(nullptr),
      builder(builder) {
  thisBinding_ = ThisBinding::Module;
  setFlag(ImmutableFlags::HasModuleGoal);
}

ScriptStencil& FunctionBox::functionStencil() const {
  return compilationState_.scriptData[funcDataIndex_];
}

ScriptStencilExtra& FunctionBox::functionExtraStencil() const {
  return compilationState_.scriptExtra[funcDataIndex_];
}

void SharedContext::copyScriptExtraFields(ScriptStencilExtra& scriptExtra) {
  MOZ_ASSERT(!isScriptExtraFieldCopiedToStencil);

  scriptExtra.immutableFlags = immutableFlags_;
  scriptExtra.extent = extent_;

  isScriptExtraFieldCopiedToStencil = true;
}

void FunctionBox::finishScriptFlags() {
  MOZ_ASSERT(!isScriptExtraFieldCopiedToStencil);

  using ImmutableFlags = ImmutableScriptFlagsEnum;
  immutableFlags_.setFlag(ImmutableFlags::HasMappedArgsObj, hasMappedArgsObj());
}

void FunctionBox::copyFunctionFields(ScriptStencil& script) {
  MOZ_ASSERT(&script == &functionStencil());
  MOZ_ASSERT(!isFunctionFieldCopiedToStencil);

  if (atom_) {
    compilationState_.parserAtoms.markUsedByStencil(atom_);
    script.functionAtom = atom_;
  }
  script.functionFlags = flags_;
  if (enclosingScopeIndex_) {
    script.setLazyFunctionEnclosingScopeIndex(*enclosingScopeIndex_);
  }
  if (wasEmittedByEnclosingScript_) {
    script.setWasEmittedByEnclosingScript();
  }

  isFunctionFieldCopiedToStencil = true;
}

void FunctionBox::copyFunctionExtraFields(ScriptStencilExtra& scriptExtra) {
  if (useMemberInitializers()) {
    scriptExtra.setMemberInitializers(memberInitializers());
  }

  scriptExtra.nargs = nargs_;
}

void FunctionBox::copyUpdatedImmutableFlags() {
  if (isInitialCompilation) {
    ScriptStencilExtra& scriptExtra = functionExtraStencil();
    scriptExtra.immutableFlags = immutableFlags_;
  }
}

void FunctionBox::copyUpdatedExtent() {
  ScriptStencilExtra& scriptExtra = functionExtraStencil();
  scriptExtra.extent = extent_;
}

void FunctionBox::copyUpdatedMemberInitializers() {
  MOZ_ASSERT(useMemberInitializers());
  if (isInitialCompilation) {
    ScriptStencilExtra& scriptExtra = functionExtraStencil();
    scriptExtra.setMemberInitializers(memberInitializers());
  } else {
    // We are delazifying and the original PrivateScriptData has the member
    // initializer information already. See: JSScript::fullyInitFromStencil.
  }
}

void FunctionBox::copyUpdatedEnclosingScopeIndex() {
  ScriptStencil& script = functionStencil();
  if (enclosingScopeIndex_) {
    script.setLazyFunctionEnclosingScopeIndex(*enclosingScopeIndex_);
  }
}

void FunctionBox::copyUpdatedAtomAndFlags() {
  ScriptStencil& script = functionStencil();
  if (atom_) {
    compilationState_.parserAtoms.markUsedByStencil(atom_);
    script.functionAtom = atom_;
  }
  script.functionFlags = flags_;
}

void FunctionBox::copyUpdatedWasEmitted() {
  ScriptStencil& script = functionStencil();
  if (wasEmittedByEnclosingScript_) {
    script.setWasEmittedByEnclosingScript();
  }
}

}  // namespace frontend
}  // namespace js
