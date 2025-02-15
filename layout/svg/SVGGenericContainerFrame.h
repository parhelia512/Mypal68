/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGGENERICCONTAINERFRAME_H_
#define LAYOUT_SVG_SVGGENERICCONTAINERFRAME_H_

#include "mozilla/Attributes.h"
#include "mozilla/SVGContainerFrame.h"
#include "gfxMatrix.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"

class nsAtom;
class nsIFrame;

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGGenericContainerFrame(mozilla::PresShell* aPresShell,
                                         mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGGenericContainerFrame final : public SVGDisplayContainerFrame {
  friend nsIFrame* ::NS_NewSVGGenericContainerFrame(
      mozilla::PresShell* aPresShell, ComputedStyle* aStyle);

 protected:
  explicit SVGGenericContainerFrame(ComputedStyle* aStyle,
                                    nsPresContext* aPresContext)
      : SVGDisplayContainerFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_FRAMEARENA_HELPERS(SVGGenericContainerFrame)

  // nsIFrame:
  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGGenericContainer"_ns, aResult);
  }
#endif

  // SVGContainerFrame methods:
  virtual gfxMatrix GetCanvasTM() override;
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGGENERICCONTAINERFRAME_H_
