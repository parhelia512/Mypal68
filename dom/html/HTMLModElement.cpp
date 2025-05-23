/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLModElement.h"
#include "mozilla/dom/HTMLModElementBinding.h"
#include "nsStyleConsts.h"

NS_IMPL_NS_NEW_HTML_ELEMENT(Mod)

namespace mozilla {
namespace dom {

HTMLModElement::HTMLModElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLModElement::~HTMLModElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLModElement)

JSObject* HTMLModElement::WrapNode(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return HTMLModElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
