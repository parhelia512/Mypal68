/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_DOMSVGANIMATEDINTEGER_H_
#define DOM_SVG_DOMSVGANIMATEDINTEGER_H_

#include "nsWrapperCache.h"

#include "SVGElement.h"

namespace mozilla {
namespace dom {

class DOMSVGAnimatedInteger : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGAnimatedInteger)

  SVGElement* GetParentObject() const { return mSVGElement; }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) final;

  virtual int32_t BaseVal() = 0;
  virtual void SetBaseVal(int32_t aBaseVal) = 0;
  virtual int32_t AnimVal() = 0;

 protected:
  explicit DOMSVGAnimatedInteger(SVGElement* aSVGElement)
      : mSVGElement(aSVGElement) {}
  virtual ~DOMSVGAnimatedInteger() = default;

  RefPtr<SVGElement> mSVGElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_SVG_DOMSVGANIMATEDINTEGER_H_
