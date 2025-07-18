/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "jsapi-tests/tests.h"

BEGIN_TEST(testFunctionProperties) {
  JS::RootedValue x(cx);
  EVAL("(function f() {})", &x);

  JS::RootedObject obj(cx, x.toObjectOrNull());

  JS::RootedValue y(cx);
  CHECK(JS_GetProperty(cx, obj, "arguments", &y));
  CHECK(y.isNull());

  CHECK(JS_GetProperty(cx, obj, "caller", &y));
  CHECK(y.isNull());

  return true;
}
END_TEST(testFunctionProperties)
