/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_chromeworkerscope_h__
#define mozilla_dom_workers_chromeworkerscope_h__

#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

bool DefineChromeWorkerFunctions(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_chromeworkerscope_h__
