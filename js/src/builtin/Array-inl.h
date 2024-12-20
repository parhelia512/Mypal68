/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Array_inl_h
#define builtin_Array_inl_h

#include "builtin/Array.h"

#include "vm/ArgumentsObject.h"
#include "vm/JSObject.h"

#include "vm/ArgumentsObject-inl.h"

namespace js {

inline bool GetElement(JSContext* cx, HandleObject obj, uint32_t index,
                       MutableHandleValue vp) {
  if (obj->is<NativeObject>() &&
      index < obj->as<NativeObject>().getDenseInitializedLength()) {
    vp.set(obj->as<NativeObject>().getDenseElement(index));
    if (!vp.isMagic(JS_ELEMENTS_HOLE)) {
      return true;
    }
  }

  if (obj->is<ArgumentsObject>()) {
    if (obj->as<ArgumentsObject>().maybeGetElement(index, vp)) {
      return true;
    }
  }

  return GetElement(cx, obj, obj, index, vp);
}

}  // namespace js

#endif  // builtin_Array_inl_h
