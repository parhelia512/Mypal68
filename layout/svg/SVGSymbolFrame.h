/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_SVG_SVGSYMBOLFRAME_H_
#define LAYOUT_SVG_SVGSYMBOLFRAME_H_

#include "SVGViewportFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

nsIFrame* NS_NewSVGSymbolFrame(mozilla::PresShell* aPresShell,
                               mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGSymbolFrame final : public SVGViewportFrame {
  friend nsIFrame* ::NS_NewSVGSymbolFrame(mozilla::PresShell* aPresShell,
                                          ComputedStyle* aStyle);

 protected:
  explicit SVGSymbolFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : SVGViewportFrame(aStyle, aPresContext, kClassID) {}

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGSymbolFrame)

#ifdef DEBUG
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;
#endif

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGSymbol"_ns, aResult);
  }
#endif
};

}  // namespace mozilla

#endif  // LAYOUT_SVG_SVGSYMBOLFRAME_H_
