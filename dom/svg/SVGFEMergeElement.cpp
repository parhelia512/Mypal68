/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEMergeElement.h"
#include "mozilla/dom/SVGFEMergeElementBinding.h"
#include "mozilla/dom/SVGFEMergeNodeElement.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMerge)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFEMergeElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEMergeElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGFEMergeElement::sStringInfo[1] = {
    {nsGkAtoms::result, kNameSpaceID_None, true}};

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEMergeElement)

FilterPrimitiveDescription SVGFEMergeElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  return FilterPrimitiveDescription(AsVariant(MergeAttributes()));
}

void SVGFEMergeElement::GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) {
  for (nsIContent* child = nsINode::GetFirstChild(); child;
       child = child->GetNextSibling()) {
    if (child->IsSVGElement(nsGkAtoms::feMergeNode)) {
      SVGFEMergeNodeElement* node = static_cast<SVGFEMergeNodeElement*>(child);
      aSources.AppendElement(SVGStringInfo(node->GetIn1(), node));
    }
  }
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::StringAttributesInfo SVGFEMergeElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace dom
}  // namespace mozilla
