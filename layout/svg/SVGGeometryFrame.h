/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __SVGGEOMETRYFRAME_H__
#define __SVGGEOMETRYFRAME_H__

#include "mozilla/Attributes.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "gfxMatrix.h"
#include "gfxRect.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"
#include "nsLiteralString.h"
#include "nsQueryFrame.h"

namespace mozilla {

class PresShell;
class SVGGeometryFrame;
class SVGMarkerObserver;

namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace image {
struct imgDrawingParams;
}  // namespace image

}  // namespace mozilla

class gfxContext;
class nsAtom;
class nsIFrame;

struct nsRect;

nsIFrame* NS_NewSVGGeometryFrame(mozilla::PresShell* aPresShell,
                                 mozilla::ComputedStyle* aStyle);

namespace mozilla {

class SVGGeometryFrame : public nsIFrame, public ISVGDisplayableFrame {
  using DrawTarget = gfx::DrawTarget;

  friend nsIFrame* ::NS_NewSVGGeometryFrame(mozilla::PresShell* aPresShell,
                                            ComputedStyle* aStyle);

  friend class DisplaySVGGeometry;

 protected:
  SVGGeometryFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                   nsIFrame::ClassID aID = kClassID)
      : nsIFrame(aStyle, aPresContext, aID) {
    AddStateBits(NS_FRAME_SVG_LAYOUT | NS_FRAME_MAY_BE_TRANSFORMED);
  }

 public:
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS(SVGGeometryFrame)

  // nsIFrame interface:
  virtual void Init(nsIContent* aContent, nsContainerFrame* aParent,
                    nsIFrame* aPrevInFlow) override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override {
    if (aFlags & eSupportsContainLayoutAndPaint) {
      return false;
    }

    return nsIFrame::IsFrameOfType(aFlags & ~nsIFrame::eSVG);
  }

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

  virtual bool IsSVGTransformed(
      Matrix* aOwnTransforms = nullptr,
      Matrix* aFromParentTransforms = nullptr) const override;

#ifdef DEBUG_FRAME_DUMP
  virtual nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"SVGGeometry"_ns, aResult);
  }
#endif

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  // SVGGeometryFrame methods
  gfxMatrix GetCanvasTM();

 protected:
  // ISVGDisplayableFrame interface:
  virtual void PaintSVG(gfxContext& aContext, const gfxMatrix& aTransform,
                        imgDrawingParams& aImgParams,
                        const nsIntRect* aDirtyRect = nullptr) override;
  virtual nsIFrame* GetFrameForPoint(const gfxPoint& aPoint) override;
  virtual void ReflowSVG() override;
  virtual void NotifySVGChanged(uint32_t aFlags) override;
  virtual SVGBBox GetBBoxContribution(const Matrix& aToBBoxUserspace,
                                      uint32_t aFlags) override;
  virtual bool IsDisplayContainer() override { return false; }

  /**
   * This function returns a set of bit flags indicating which parts of the
   * element (fill, stroke, bounds) should intercept pointer events. It takes
   * into account the type of element and the value of the 'pointer-events'
   * property on the element.
   */
  virtual uint16_t GetHitTestFlags();

 private:
  enum { eRenderFill = 1, eRenderStroke = 2 };
  void Render(gfxContext* aContext, uint32_t aRenderComponents,
              const gfxMatrix& aTransform, imgDrawingParams& aImgParams);

  /**
   * @param aMatrix The transform that must be multiplied onto aContext to
   *   establish this frame's SVG user space.
   */
  void PaintMarkers(gfxContext& aContext, const gfxMatrix& aTransform,
                    imgDrawingParams& aImgParams);
};

//----------------------------------------------------------------------
// Display list item:

class DisplaySVGGeometry final : public nsPaintedDisplayItem {
  using imgDrawingParams = image::imgDrawingParams;

 public:
  DisplaySVGGeometry(nsDisplayListBuilder* aBuilder, SVGGeometryFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(DisplaySVGGeometry);
    MOZ_ASSERT(aFrame, "Must have a frame!");
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  MOZ_COUNTED_DTOR_OVERRIDE(DisplaySVGGeometry)
#endif

  NS_DISPLAY_DECL_NAME("DisplaySVGGeometry", TYPE_SVG_GEOMETRY)

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*>* aOutFrames) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayItemGenericImageGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;
};
}  // namespace mozilla

#endif  // __SVGGEOMETRYFRAME_H__
