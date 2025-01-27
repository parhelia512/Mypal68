/* Copyright 2020 Mozilla Foundation
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

#include "wasm/WasmContext.h"

#include "wasm/WasmTypes.h"

using namespace js;
using namespace wasm;

bool wasm::Context::ensureTypeContext(JSContext* cx) {
  if (typeContext) {
    return true;
  }
  typeContext =
      js::MakeUnique<TypeContext>(FeatureArgs::build(cx), TypeDefVector());
  return !!typeContext;
}

size_t wasm::Context::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return typeContext ? typeContext->sizeOfExcludingThis(mallocSizeOf) : 0;
}
