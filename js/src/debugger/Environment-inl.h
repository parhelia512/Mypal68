/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef debugger_Environment_inl_h
#define debugger_Environment_inl_h

#include "debugger/Environment.h"  // for DebuggerEnvironment

#include "jstypes.h"            // for JS_PUBLIC_API
#include "NamespaceImports.h"   // for Value
#include "debugger/Debugger.h"  // for Debugger

#include "debugger/Debugger-inl.h"  // for Debugger::fromJSObject

class JS_PUBLIC_API JSObject;

// The Debugger.Environment.prototype object also has a class of
// DebuggerEnvironment::class_ so we differentiate instances from the prototype
// based on the presence of an owner debugger.
inline bool js::DebuggerEnvironment::isInstance() const {
  return !getReservedSlot(OWNER_SLOT).isUndefined();
}

inline js::Debugger* js::DebuggerEnvironment::owner() const {
  MOZ_ASSERT(isInstance());
  JSObject* dbgobj = &getReservedSlot(OWNER_SLOT).toObject();
  return Debugger::fromJSObject(dbgobj);
}

#endif /* debugger_Environment_inl_h */
