/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_String_h
#define builtin_String_h

#include "NamespaceImports.h"

#include "js/RootingAPI.h"
#include "js/Value.h"

namespace js {

class ArrayObject;
class GlobalObject;

/* Initialize the String class, returning its prototype object. */
extern JSObject* InitStringClass(JSContext* cx, Handle<GlobalObject*> global);

extern bool str_fromCharCode(JSContext* cx, unsigned argc, Value* vp);

extern bool str_fromCharCode_one_arg(JSContext* cx, HandleValue code,
                                     MutableHandleValue rval);

extern bool str_fromCodePoint(JSContext* cx, unsigned argc, Value* vp);

extern bool str_fromCodePoint_one_arg(JSContext* cx, HandleValue code,
                                      MutableHandleValue rval);

// String methods exposed so they can be installed in the self-hosting global.

extern bool str_includes(JSContext* cx, unsigned argc, Value* vp);

extern bool str_indexOf(JSContext* cx, unsigned argc, Value* vp);

extern bool str_startsWith(JSContext* cx, unsigned argc, Value* vp);

extern bool str_toString(JSContext* cx, unsigned argc, Value* vp);

extern bool str_charCodeAt_impl(JSContext* cx, HandleString string,
                                HandleValue index, MutableHandleValue res);

extern bool str_charCodeAt(JSContext* cx, unsigned argc, Value* vp);

extern bool str_endsWith(JSContext* cx, unsigned argc, Value* vp);

#if JS_HAS_INTL_API
/**
 * Returns the input string converted to lower case based on the language
 * specific case mappings for the input locale.
 *
 * Usage: lowerCase = intl_toLocaleLowerCase(string, locale)
 */
[[nodiscard]] extern bool intl_toLocaleLowerCase(JSContext* cx, unsigned argc,
                                                 Value* vp);

/**
 * Returns the input string converted to upper case based on the language
 * specific case mappings for the input locale.
 *
 * Usage: upperCase = intl_toLocaleUpperCase(string, locale)
 */
[[nodiscard]] extern bool intl_toLocaleUpperCase(JSContext* cx, unsigned argc,
                                                 Value* vp);
#endif

ArrayObject* StringSplitString(JSContext* cx, HandleString str,
                               HandleString sep, uint32_t limit);

JSString* StringFlatReplaceString(JSContext* cx, HandleString string,
                                  HandleString pattern,
                                  HandleString replacement);

JSString* str_replace_string_raw(JSContext* cx, HandleString string,
                                 HandleString pattern,
                                 HandleString replacement);

JSString* str_replaceAll_string_raw(JSContext* cx, HandleString string,
                                    HandleString pattern,
                                    HandleString replacement);

extern JSString* StringToLowerCase(JSContext* cx, HandleString string);

extern JSString* StringToUpperCase(JSContext* cx, HandleString string);

extern bool StringConstructor(JSContext* cx, unsigned argc, Value* vp);

extern bool FlatStringMatch(JSContext* cx, unsigned argc, Value* vp);

extern bool FlatStringSearch(JSContext* cx, unsigned argc, Value* vp);

} /* namespace js */

#endif /* builtin_String_h */
