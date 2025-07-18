/* Copyright 2021 Mozilla Foundation
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

#ifndef wasm_exception_h
#define wasm_exception_h

#include "js/Class.h"  // JSClassOps, ClassSpec
#include "js/RefCounted.h"
#include "js/TypeDecls.h"
#include "vm/JSObject.h"
#include "vm/NativeObject.h"  // NativeObject

namespace js {
namespace wasm {

// Exception tags are used to uniquely identify exceptions. They are stored
// in a vector in Instances and used by both WebAssembly.Tag for import
// and export, and by WebAssembly.Exception for thrown exceptions.
//
// Since an exception tag is a (trivial) substructure of AtomicRefCounted, the
// RefPtr SharedTag can have many instances/modules referencing a single
// constant exception tag.
//
// It is possible that other proposals will start using tags as well, in which
// case it may be worth generalizing this representation for other kinds of
// tags.

struct ExceptionTag : AtomicRefCounted<ExceptionTag> {
  ExceptionTag() = default;
};
using SharedExceptionTag = RefPtr<ExceptionTag>;
using SharedExceptionTagVector =
    Vector<SharedExceptionTag, 0, SystemAllocPolicy>;

// WasmJSExceptionObject wraps a JS Value in order to provide a uniform
// method of handling JS thrown exceptions. Exceptions originating in Wasm
// are WebAssemby.Exception objects, whereas exceptions from JS are
// wrapped as WasmJSExceptionObject objects.

class WasmJSExceptionObject : public NativeObject {
  static const unsigned VALUE_SLOT = 0;

 public:
  static const unsigned RESERVED_SLOTS = 1;
  static const JSClass class_;
  const Value& value() const { return getFixedSlot(VALUE_SLOT); }

  static WasmJSExceptionObject* create(JSContext* cx, MutableHandleValue value);
};

}  // namespace wasm
}  // namespace js

#endif  // wasm_exception_h
