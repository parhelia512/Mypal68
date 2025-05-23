/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayPortUtils.h"

#include "FrameMetrics.h"
#include "Layers.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/APZCCallbackHelper.h"
#include "mozilla/layers/APZPublicUtils.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/PAPZ.h"
#include "mozilla/layers/RepaintRequest.h"
#include "mozilla/StaticPrefs_layers.h"
#include "nsDeckFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsSubDocumentFrame.h"
#include "RetainedDisplayListBuilder.h"

#include <ostream>

namespace mozilla {

using gfx::gfxVars;
using gfx::IntSize;

using layers::APZCCallbackHelper;
using layers::FrameMetrics;
using layers::LayerManager;
using layers::RepaintRequest;
using layers::ScrollableLayerGuid;

typedef ScrollableLayerGuid::ViewID ViewID;

/* static */
DisplayPortMargins DisplayPortMargins::WithAdjustment(
    const ScreenMargin& aMargins, const CSSPoint& aVisualOffset,
    const CSSPoint& aLayoutOffset, const CSSToScreenScale2D& aScale) {
  return DisplayPortMargins{aMargins, aVisualOffset, aLayoutOffset, aScale};
}

/* static */
DisplayPortMargins DisplayPortMargins::WithNoAdjustment(
    const ScreenMargin& aMargins) {
  // Use values such that GetRelativeToLayoutViewport() will just return
  // mMargins.
  return WithAdjustment(aMargins, CSSPoint(), CSSPoint(),
                        CSSToScreenScale2D(1.0, 1.0));
}

ScreenMargin DisplayPortMargins::GetRelativeToLayoutViewport(
    ContentGeometryType aGeometryType,
    nsIScrollableFrame* aScrollableFrame) const {
  // APZ wants |mMargins| applied relative to the visual viewport.
  // The main-thread painting code applies margins relative to
  // the layout viewport. To get the main thread to paint the
  // area APZ wants, apply a translation between the two. The
  // magnitude of the translation depends on whether we are
  // applying the displayport to scrolled or fixed content.
  CSSPoint scrollDeltaCss =
      ComputeAsyncTranslation(aGeometryType, aScrollableFrame);
  ScreenPoint scrollDelta = scrollDeltaCss * mScale;
  ScreenMargin margins = mMargins;
  margins.left -= scrollDelta.x;
  margins.right += scrollDelta.x;
  margins.top -= scrollDelta.y;
  margins.bottom += scrollDelta.y;
  return margins;
}

std::ostream& operator<<(std::ostream& aOs,
                         const DisplayPortMargins& aMargins) {
  if (aMargins.mVisualOffset == CSSPoint() &&
      aMargins.mLayoutOffset == CSSPoint()) {
    aOs << aMargins.mMargins;
  } else {
    aOs << "{" << aMargins.mMargins << "," << aMargins.mVisualOffset << ","
        << aMargins.mLayoutOffset << "}";
  }
  return aOs;
}

CSSPoint DisplayPortMargins::ComputeAsyncTranslation(
    ContentGeometryType aGeometryType,
    nsIScrollableFrame* aScrollableFrame) const {
  // If we are applying the displayport to scrolled content, the
  // translation is the entire difference between the visual and
  // layout offsets.
  if (aGeometryType == ContentGeometryType::Scrolled) {
    return mVisualOffset - mLayoutOffset;
  }

  // If we are applying the displayport to fixed content, only
  // part of the difference between the visual and layout offsets
  // should be applied. This is because fixed content remains fixed
  // to the layout viewport, and some of the async delta between
  // the visual and layout offsets can drag the layout viewport
  // with it. We want only the remaining delta, i.e. the offset of
  // the visual viewport relative to the (async-scrolled) layout
  // viewport.
  if (!aScrollableFrame) {
    // Displayport on a non-scrolling frame for some reason.
    // There will be no divergence between the two viewports.
    return CSSPoint();
  }
  // Fixed content is always fixed to an RSF.
  MOZ_ASSERT(aScrollableFrame->IsRootScrollFrameOfDocument());
  nsIFrame* scrollFrame = do_QueryFrame(aScrollableFrame);
  if (!scrollFrame->PresShell()->IsVisualViewportSizeSet()) {
    // Zooming is disabled, so the layout viewport tracks the
    // visual viewport completely.
    return CSSPoint();
  }
  // Use KeepLayoutViewportEnclosingViewportVisual() to compute
  // an async layout viewport the way APZ would.
  const CSSRect visualViewport{
      mVisualOffset,
      // TODO: There are probably some edge cases here around async zooming
      // that are not currently being handled properly. For proper handling,
      // we'd likely need to save APZ's async zoom when populating
      // mVisualOffset, and using it to adjust the visual viewport size here.
      // Note that any incorrectness caused by this will only occur transiently
      // during async zooming.
      CSSSize::FromAppUnits(scrollFrame->PresShell()->GetVisualViewportSize())};
  const CSSRect scrollableRect = CSSRect::FromAppUnits(
      nsLayoutUtils::CalculateExpandedScrollableRect(scrollFrame));
  CSSRect asyncLayoutViewport{
      mLayoutOffset,
      CSSSize::FromAppUnits(aScrollableFrame->GetScrollPortRect().Size())};
  FrameMetrics::KeepLayoutViewportEnclosingVisualViewport(
      visualViewport, scrollableRect, /* out */ asyncLayoutViewport);
  return mVisualOffset - asyncLayoutViewport.TopLeft();
}

// Return the maximum displayport size, based on the LayerManager's maximum
// supported texture size. The result is in app units.
static nscoord GetMaxDisplayPortSize(nsIContent* aContent,
                                     nsPresContext* aFallbackPrescontext) {
  MOZ_ASSERT(!StaticPrefs::layers_enable_tiles_AtStartup(),
             "Do not clamp displayports if tiling is enabled");

  // Pick a safe maximum displayport size for sanity purposes. This is the
  // lowest maximum texture size on tileless-platforms (Windows, D3D10).
  // If the gfx.max-texture-size pref is set, further restrict the displayport
  // size to fit within that, because the compositor won't upload stuff larger
  // than this size.
  nscoord safeMaximum = aFallbackPrescontext
                            ? aFallbackPrescontext->DevPixelsToAppUnits(
                                  std::min(8192, gfxPlatform::MaxTextureSize()))
                            : nscoord_MAX;

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    return safeMaximum;
  }
  frame = nsLayoutUtils::GetDisplayRootFrame(frame);

  nsIWidget* widget = frame->GetNearestWidget();
  if (!widget) {
    return safeMaximum;
  }
  LayerManager* lm = widget->GetLayerManager();
  if (!lm) {
    return safeMaximum;
  }
  nsPresContext* presContext = frame->PresContext();

  int32_t maxSizeInDevPixels = lm->GetMaxTextureSize();
  if (maxSizeInDevPixels < 0 || maxSizeInDevPixels == INT_MAX) {
    return safeMaximum;
  }
  maxSizeInDevPixels =
      std::min(maxSizeInDevPixels, gfxPlatform::MaxTextureSize());
  return presContext->DevPixelsToAppUnits(maxSizeInDevPixels);
}

static nsRect ApplyRectMultiplier(nsRect aRect, float aMultiplier) {
  if (aMultiplier == 1.0f) {
    return aRect;
  }
  float newWidth = aRect.width * aMultiplier;
  float newHeight = aRect.height * aMultiplier;
  float newX = aRect.x - ((newWidth - aRect.width) / 2.0f);
  float newY = aRect.y - ((newHeight - aRect.height) / 2.0f);
  // Rounding doesn't matter too much here, do a round-in
  return nsRect(ceil(newX), ceil(newY), floor(newWidth), floor(newHeight));
}

static nsRect GetDisplayPortFromRectData(nsIContent* aContent,
                                         DisplayPortPropertyData* aRectData,
                                         float aMultiplier) {
  // In the case where the displayport is set as a rect, we assume it is
  // already aligned and clamped as necessary. The burden to do that is
  // on the setter of the displayport. In practice very few places set the
  // displayport directly as a rect (mostly tests). We still do need to
  // expand it by the multiplier though.
  return ApplyRectMultiplier(aRectData->mRect, aMultiplier);
}

static nsRect GetDisplayPortFromMarginsData(
    nsIContent* aContent, DisplayPortMarginsPropertyData* aMarginsData,
    float aMultiplier, const DisplayPortOptions& aOptions) {
  // In the case where the displayport is set via margins, we apply the margins
  // to a base rect. Then we align the expanded rect based on the alignment
  // requested, further expand the rect by the multiplier, and finally, clamp it
  // to the size of the scrollable rect.

  nsRect base;
  if (nsRect* baseData = static_cast<nsRect*>(
          aContent->GetProperty(nsGkAtoms::DisplayPortBase))) {
    base = *baseData;
  } else {
    // In theory we shouldn't get here, but we do sometimes (see bug 1212136).
    // Fall through for graceful handling.
  }

  nsIFrame* frame = nsLayoutUtils::GetScrollFrameFromContent(aContent);
  if (!frame) {
    // Turns out we can't really compute it. Oops. We still should return
    // something sane. Note that since we can't clamp the rect without a
    // frame, we don't apply the multiplier either as it can cause the result
    // to leak outside the scrollable area.
    NS_WARNING(
        "Attempting to get a displayport from a content with no primary "
        "frame!");
    return base;
  }

  bool isRoot = false;
  if (aContent->OwnerDoc()->GetRootElement() == aContent) {
    isRoot = true;
  }

  nsIScrollableFrame* scrollableFrame = frame->GetScrollTargetFrame();
  nsPoint scrollPos;
  if (scrollableFrame) {
    scrollPos = scrollableFrame->GetScrollPosition();
  }

  nsPresContext* presContext = frame->PresContext();
  int32_t auPerDevPixel = presContext->AppUnitsPerDevPixel();

  LayoutDeviceToScreenScale2D res(
      presContext->PresShell()->GetCumulativeResolution() *
      nsLayoutUtils::GetTransformToAncestorScale(frame));

  // Calculate the expanded scrollable rect, which we'll be clamping the
  // displayport to.
  nsRect expandedScrollableRect =
      nsLayoutUtils::CalculateExpandedScrollableRect(frame);

  // GetTransformToAncestorScale() can return 0. In this case, just return the
  // base rect (clamped to the expanded scrollable rect), as other calculations
  // would run into divisions by zero.
  if (res == LayoutDeviceToScreenScale2D(0, 0)) {
    // Make sure the displayport remains within the scrollable rect.
    return base.MoveInsideAndClamp(expandedScrollableRect - scrollPos);
  }

  // First convert the base rect to screen pixels
  LayoutDeviceToScreenScale2D parentRes = res;
  if (isRoot) {
    // the base rect for root scroll frames is specified in the parent document
    // coordinate space, so it doesn't include the local resolution.
    float localRes = presContext->PresShell()->GetResolution();
    parentRes.xScale /= localRes;
    parentRes.yScale /= localRes;
  }
  ScreenRect screenRect =
      LayoutDeviceRect::FromAppUnits(base, auPerDevPixel) * parentRes;

  // Note on the correctness of applying the alignment in Screen space:
  //   The correct space to apply the alignment in would be Layer space, but
  //   we don't necessarily know the scale to convert to Layer space at this
  //   point because Layout may not yet have chosen the resolution at which to
  //   render (it chooses that in FrameLayerBuilder, but this can be called
  //   during display list building). Therefore, we perform the alignment in
  //   Screen space, which basically assumes that Layout chose to render at
  //   screen resolution; since this is what Layout does most of the time,
  //   this is a good approximation. A proper solution would involve moving
  //   the choosing of the resolution to display-list building time.
  ScreenSize alignment;

  PresShell* presShell = presContext->PresShell();
  MOZ_ASSERT(presShell);

#ifdef MOZ_BUILD_WEBRENDER
  bool useWebRender = gfxVars::UseWebRender();
#endif

  ScreenMargin margins = aMarginsData->mMargins.GetRelativeToLayoutViewport(
      aOptions.mGeometryType, scrollableFrame);

  if (presShell->IsDisplayportSuppressed()) {
    alignment = ScreenSize(1, 1);
#ifdef MOZ_BUILD_WEBRENDER
  } else if (useWebRender) {
    // Moving the displayport is relatively expensive with WR so we use a larger
    // alignment that causes the displayport to move less frequently. The
    // alignment scales up with the size of the base rect so larger scrollframes
    // use a larger alignment, but we clamp the alignment to a power of two
    // between 128 and 1024 (inclusive).
    // This naturally also increases the size of the displayport compared to
    // always using a 128 alignment, so the displayport multipliers are also
    // correspondingly smaller when WR is enabled to prevent the displayport
    // from becoming too big.
    IntSize multiplier =
        layers::apz::GetDisplayportAlignmentMultiplier(screenRect.Size());
    alignment = ScreenSize(128 * multiplier.width, 128 * multiplier.height);
#endif
  } else if (StaticPrefs::layers_enable_tiles_AtStartup()) {
    // Don't align to tiles if they are too large, because we could expand
    // the displayport by a lot which can take more paint time. It's a tradeoff
    // though because if we don't align to tiles we have more waste on upload.
    IntSize tileSize = gfxVars::TileSize();
    alignment = ScreenSize(std::min(256, tileSize.width),
                           std::min(256, tileSize.height));
  } else {
    // If we're not drawing with tiles then we need to be careful about not
    // hitting the max texture size and we only need 1 draw call per layer
    // so we can align to a smaller multiple.
    alignment = ScreenSize(128, 128);
  }

  // Avoid division by zero.
  if (alignment.width == 0) {
    alignment.width = 128;
  }
  if (alignment.height == 0) {
    alignment.height = 128;
  }

  if (StaticPrefs::layers_enable_tiles_AtStartup()
#ifdef MOZ_BUILD_WEBRENDER
      || useWebRender
#endif
     ) {
    // Expand the rect by the margins
    screenRect.Inflate(margins);
  } else {
    // Calculate the displayport to make sure we fit within the max texture size
    // when not tiling.
    nscoord maxSizeAppUnits = GetMaxDisplayPortSize(aContent, presContext);
    MOZ_ASSERT(maxSizeAppUnits < nscoord_MAX);

    // The alignment code can round up to 3 tiles, we want to make sure
    // that the displayport can grow by up to 3 tiles without going
    // over the max texture size.
    const int MAX_ALIGN_ROUNDING = 3;

    // Find the maximum size in screen pixels.
    int32_t maxSizeDevPx = presContext->AppUnitsToDevPixels(maxSizeAppUnits);
    int32_t maxWidthScreenPx = floor(double(maxSizeDevPx) * res.xScale) -
                               MAX_ALIGN_ROUNDING * alignment.width;
    int32_t maxHeightScreenPx = floor(double(maxSizeDevPx) * res.yScale) -
                                MAX_ALIGN_ROUNDING * alignment.height;

    // For each axis, inflate the margins up to the maximum size.
    if (screenRect.height < maxHeightScreenPx) {
      int32_t budget = maxHeightScreenPx - screenRect.height;
      // Scale the margins down to fit into the budget if necessary, maintaining
      // their relative ratio.
      float scale = 1.0f;
      if (float(budget) < margins.TopBottom()) {
        scale = float(budget) / margins.TopBottom();
      }
      float top = margins.top * scale;
      float bottom = margins.bottom * scale;
      screenRect.y -= top;
      screenRect.height += top + bottom;
    }
    if (screenRect.width < maxWidthScreenPx) {
      int32_t budget = maxWidthScreenPx - screenRect.width;
      float scale = 1.0f;
      if (float(budget) < margins.LeftRight()) {
        scale = float(budget) / margins.LeftRight();
      }
      float left = margins.left * scale;
      float right = margins.right * scale;
      screenRect.x -= left;
      screenRect.width += left + right;
    }
  }

  ScreenPoint scrollPosScreen =
      LayoutDevicePoint::FromAppUnits(scrollPos, auPerDevPixel) * res;

  // Align the display port.
  screenRect += scrollPosScreen;
  float x = alignment.width * floor(screenRect.x / alignment.width);
  float y = alignment.height * floor(screenRect.y / alignment.height);
  float w = alignment.width * ceil(screenRect.width / alignment.width + 1);
  float h = alignment.height * ceil(screenRect.height / alignment.height + 1);
  screenRect = ScreenRect(x, y, w, h);
  screenRect -= scrollPosScreen;

  // Convert the aligned rect back into app units.
  nsRect result = LayoutDeviceRect::ToAppUnits(screenRect / res, auPerDevPixel);

  // If we have non-zero margins, expand the displayport for the low-res buffer
  // if that's what we're drawing. If we have zero margins, we want the
  // displayport to reflect the scrollport.
  if (margins != ScreenMargin()) {
    result = ApplyRectMultiplier(result, aMultiplier);
  }

  // Make sure the displayport remains within the scrollable rect.
  result = result.MoveInsideAndClamp(expandedScrollableRect - scrollPos);

  return result;
}

static bool GetDisplayPortData(
    nsIContent* aContent, DisplayPortPropertyData** aOutRectData,
    DisplayPortMarginsPropertyData** aOutMarginsData) {
  MOZ_ASSERT(aOutRectData && aOutMarginsData);

  *aOutRectData = static_cast<DisplayPortPropertyData*>(
      aContent->GetProperty(nsGkAtoms::DisplayPort));
  *aOutMarginsData = static_cast<DisplayPortMarginsPropertyData*>(
      aContent->GetProperty(nsGkAtoms::DisplayPortMargins));

  if (!*aOutRectData && !*aOutMarginsData) {
    // This content element has no displayport data at all
    return false;
  }

  if (*aOutRectData && *aOutMarginsData) {
    // choose margins if equal priority
    if ((*aOutRectData)->mPriority > (*aOutMarginsData)->mPriority) {
      *aOutMarginsData = nullptr;
    } else {
      *aOutRectData = nullptr;
    }
  }

  NS_ASSERTION((*aOutRectData == nullptr) != (*aOutMarginsData == nullptr),
               "Only one of aOutRectData or aOutMarginsData should be set!");

  return true;
}

static bool GetWasDisplayPortPainted(nsIContent* aContent) {
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;

  if (!GetDisplayPortData(aContent, &rectData, &marginsData)) {
    return false;
  }

  return rectData ? rectData->mPainted : marginsData->mPainted;
}

bool DisplayPortUtils::IsMissingDisplayPortBaseRect(nsIContent* aContent) {
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;

  if (GetDisplayPortData(aContent, &rectData, &marginsData) && marginsData) {
    return !aContent->GetProperty(nsGkAtoms::DisplayPortBase);
  }

  return false;
}

static void TranslateFromScrollPortToScrollFrame(nsIContent* aContent,
                                                 nsRect* aRect) {
  MOZ_ASSERT(aRect);
  if (nsIScrollableFrame* scrollableFrame =
          nsLayoutUtils::FindScrollableFrameFor(aContent)) {
    *aRect += scrollableFrame->GetScrollPortRect().TopLeft();
  }
}

static bool GetDisplayPortImpl(nsIContent* aContent, nsRect* aResult,
                               float aMultiplier,
                               const DisplayPortOptions& aOptions) {
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;

  if (!GetDisplayPortData(aContent, &rectData, &marginsData)) {
    return false;
  }

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (frame && !frame->PresShell()->AsyncPanZoomEnabled()) {
    return false;
  }

  if (!aResult) {
    // We have displayport data, but the caller doesn't want the actual
    // rect, so we don't need to actually compute it.
    return true;
  }

  bool isDisplayportSuppressed = false;

  if (frame) {
    nsPresContext* presContext = frame->PresContext();
    MOZ_ASSERT(presContext);
    PresShell* presShell = presContext->PresShell();
    MOZ_ASSERT(presShell);
    isDisplayportSuppressed = presShell->IsDisplayportSuppressed();
  }

  nsRect result;
  if (rectData) {
    result = GetDisplayPortFromRectData(aContent, rectData, aMultiplier);
  } else if (isDisplayportSuppressed ||
             nsLayoutUtils::ShouldDisableApzForElement(aContent)) {
    DisplayPortMarginsPropertyData noMargins(DisplayPortMargins::Empty(), 1,
                                             /*painted=*/false);
    result = GetDisplayPortFromMarginsData(aContent, &noMargins, aMultiplier,
                                           aOptions);
  } else {
    result = GetDisplayPortFromMarginsData(aContent, marginsData, aMultiplier,
                                           aOptions);
  }

  if (!StaticPrefs::layers_enable_tiles_AtStartup()) {
    // Perform the desired error handling if the displayport dimensions
    // exceeds the maximum allowed size
    nscoord maxSize = GetMaxDisplayPortSize(aContent, nullptr);
    if (result.width > maxSize || result.height > maxSize) {
      switch (aOptions.mMaxSizeExceededBehaviour) {
        case MaxSizeExceededBehaviour::Assert:
          NS_ASSERTION(false, "Displayport must be a valid texture size");
          break;
        case MaxSizeExceededBehaviour::Drop:
          return false;
      }
    }
  }

  if (aOptions.mRelativeTo == DisplayportRelativeTo::ScrollFrame) {
    TranslateFromScrollPortToScrollFrame(aContent, &result);
  }

  *aResult = result;
  return true;
}

bool DisplayPortUtils::GetDisplayPort(nsIContent* aContent, nsRect* aResult,
                                      const DisplayPortOptions& aOptions) {
  float multiplier = StaticPrefs::layers_low_precision_buffer()
                         ? 1.0f / StaticPrefs::layers_low_precision_resolution()
                         : 1.0f;
  return GetDisplayPortImpl(aContent, aResult, multiplier, aOptions);
}

bool DisplayPortUtils::HasDisplayPort(nsIContent* aContent) {
  return GetDisplayPort(aContent, nullptr);
}

bool DisplayPortUtils::HasPaintedDisplayPort(nsIContent* aContent) {
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;
  GetDisplayPortData(aContent, &rectData, &marginsData);
  if (rectData) {
    return rectData->mPainted;
  }
  if (marginsData) {
    return marginsData->mPainted;
  }
  return false;
}

void DisplayPortUtils::MarkDisplayPortAsPainted(nsIContent* aContent) {
  DisplayPortPropertyData* rectData = nullptr;
  DisplayPortMarginsPropertyData* marginsData = nullptr;
  GetDisplayPortData(aContent, &rectData, &marginsData);
  MOZ_ASSERT(rectData || marginsData,
             "MarkDisplayPortAsPainted should only be called for an element "
             "with a displayport");
  if (rectData) {
    rectData->mPainted = true;
  }
  if (marginsData) {
    marginsData->mPainted = true;
  }
}

/* static */
bool DisplayPortUtils::GetDisplayPortForVisibilityTesting(nsIContent* aContent,
                                                          nsRect* aResult) {
  MOZ_ASSERT(aResult);
  // Since the base rect might not have been updated very recently, it's
  // possible to end up with an extra-large displayport at this point, if the
  // zoom level is changed by a lot. Instead of using the default behaviour of
  // asserting, we can just ignore the displayport if that happens, as this
  // call site is best-effort.
  return GetDisplayPortImpl(aContent, aResult, 1.0f,
                            DisplayPortOptions()
                                .With(MaxSizeExceededBehaviour::Drop)
                                .With(DisplayportRelativeTo::ScrollFrame));
}

void DisplayPortUtils::InvalidateForDisplayPortChange(
    nsIContent* aContent, bool aHadDisplayPort, const nsRect& aOldDisplayPort,
    const nsRect& aNewDisplayPort, RepaintMode aRepaintMode) {
  if (aRepaintMode != RepaintMode::Repaint) {
    return;
  }

  bool changed =
      !aHadDisplayPort || !aOldDisplayPort.IsEqualEdges(aNewDisplayPort);

  nsIFrame* frame = nsLayoutUtils::GetScrollFrameFromContent(aContent);
  if (frame) {
    frame = do_QueryFrame(frame->GetScrollTargetFrame());
  }

  if (changed && frame) {
    // It is important to call SchedulePaint on the same frame that we set the
    // dirty rect properties on so we can find the frame later to remove the
    // properties.
    frame->SchedulePaint();

    if (!nsLayoutUtils::AreRetainedDisplayListsEnabled() ||
        !nsLayoutUtils::DisplayRootHasRetainedDisplayListBuilder(frame)) {
      return;
    }

    bool found;
    nsRect* rect = frame->GetProperty(
        nsDisplayListBuilder::DisplayListBuildingDisplayPortRect(), &found);

    if (!found) {
      rect = new nsRect();
      frame->AddProperty(
          nsDisplayListBuilder::DisplayListBuildingDisplayPortRect(), rect);
      frame->SetHasOverrideDirtyRegion(true);

      nsIFrame* rootFrame = frame->PresShell()->GetRootFrame();
      MOZ_ASSERT(rootFrame);

      RetainedDisplayListData* data =
          GetOrSetRetainedDisplayListData(rootFrame);
      data->Flags(frame) |= RetainedDisplayListData::FrameFlags::HasProps;
    } else {
      MOZ_ASSERT(rect, "this property should only store non-null values");
    }

    if (aHadDisplayPort) {
      // We only need to build a display list for any new areas added
      nsRegion newRegion(aNewDisplayPort);
      newRegion.SubOut(aOldDisplayPort);
      rect->UnionRect(*rect, newRegion.GetBounds());
    } else {
      rect->UnionRect(*rect, aNewDisplayPort);
    }
  }
}

bool DisplayPortUtils::SetDisplayPortMargins(nsIContent* aContent,
                                             PresShell* aPresShell,
                                             const DisplayPortMargins& aMargins,
                                             uint32_t aPriority,
                                             RepaintMode aRepaintMode) {
  MOZ_ASSERT(aContent);
  MOZ_ASSERT(aContent->GetComposedDoc() == aPresShell->GetDocument());

  DisplayPortMarginsPropertyData* currentData =
      static_cast<DisplayPortMarginsPropertyData*>(
          aContent->GetProperty(nsGkAtoms::DisplayPortMargins));
  if (currentData && currentData->mPriority > aPriority) {
    return false;
  }

  nsIFrame* scrollFrame = nsLayoutUtils::GetScrollFrameFromContent(aContent);

  nsRect oldDisplayPort;
  bool hadDisplayPort = false;
  bool wasPainted = GetWasDisplayPortPainted(aContent);
  if (scrollFrame) {
    // We only use the two return values from this function to call
    // InvalidateForDisplayPortChange. InvalidateForDisplayPortChange does
    // nothing if aContent does not have a frame. So getting the displayport is
    // useless if the content has no frame, so we avoid calling this to avoid
    // triggering a warning about not having a frame.
    hadDisplayPort = GetHighResolutionDisplayPort(aContent, &oldDisplayPort);
  }

  aContent->SetProperty(
      nsGkAtoms::DisplayPortMargins,
      new DisplayPortMarginsPropertyData(aMargins, aPriority, wasPainted),
      nsINode::DeleteProperty<DisplayPortMarginsPropertyData>);

  nsIScrollableFrame* scrollableFrame =
      scrollFrame ? scrollFrame->GetScrollTargetFrame() : nullptr;
  if (!scrollableFrame) {
    return true;
  }

  nsRect newDisplayPort;
  DebugOnly<bool> hasDisplayPort =
      GetHighResolutionDisplayPort(aContent, &newDisplayPort);
  MOZ_ASSERT(hasDisplayPort);

  InvalidateForDisplayPortChange(aContent, hadDisplayPort, oldDisplayPort,
                                 newDisplayPort, aRepaintMode);

  scrollableFrame->TriggerDisplayPortExpiration();

  // Display port margins changing means that the set of visible frames may
  // have drastically changed. Check if we should schedule an update.
  hadDisplayPort =
      scrollableFrame->GetDisplayPortAtLastApproximateFrameVisibilityUpdate(
          &oldDisplayPort);

  bool needVisibilityUpdate = !hadDisplayPort;
  // Check if the total size has changed by a large factor.
  if (!needVisibilityUpdate) {
    if ((newDisplayPort.width > 2 * oldDisplayPort.width) ||
        (oldDisplayPort.width > 2 * newDisplayPort.width) ||
        (newDisplayPort.height > 2 * oldDisplayPort.height) ||
        (oldDisplayPort.height > 2 * newDisplayPort.height)) {
      needVisibilityUpdate = true;
    }
  }
  // Check if it's moved by a significant amount.
  if (!needVisibilityUpdate) {
    if (nsRect* baseData = static_cast<nsRect*>(
            aContent->GetProperty(nsGkAtoms::DisplayPortBase))) {
      nsRect base = *baseData;
      if ((std::abs(newDisplayPort.X() - oldDisplayPort.X()) > base.width) ||
          (std::abs(newDisplayPort.XMost() - oldDisplayPort.XMost()) >
           base.width) ||
          (std::abs(newDisplayPort.Y() - oldDisplayPort.Y()) > base.height) ||
          (std::abs(newDisplayPort.YMost() - oldDisplayPort.YMost()) >
           base.height)) {
        needVisibilityUpdate = true;
      }
    }
  }
  if (needVisibilityUpdate) {
    aPresShell->ScheduleApproximateFrameVisibilityUpdateNow();
  }

  return true;
}

void DisplayPortUtils::SetDisplayPortBase(nsIContent* aContent,
                                          const nsRect& aBase) {
  aContent->SetProperty(nsGkAtoms::DisplayPortBase, new nsRect(aBase),
                        nsINode::DeleteProperty<nsRect>);
}

void DisplayPortUtils::SetDisplayPortBaseIfNotSet(nsIContent* aContent,
                                                  const nsRect& aBase) {
  if (!aContent->GetProperty(nsGkAtoms::DisplayPortBase)) {
    SetDisplayPortBase(aContent, aBase);
  }
}

bool DisplayPortUtils::GetCriticalDisplayPort(
    nsIContent* aContent, nsRect* aResult, const DisplayPortOptions& aOptions) {
  if (StaticPrefs::layers_low_precision_buffer()) {
    return GetDisplayPortImpl(aContent, aResult, 1.0f, aOptions);
  }
  return false;
}

bool DisplayPortUtils::HasCriticalDisplayPort(nsIContent* aContent) {
  return GetCriticalDisplayPort(aContent, nullptr);
}

bool DisplayPortUtils::GetHighResolutionDisplayPort(
    nsIContent* aContent, nsRect* aResult, const DisplayPortOptions& aOptions) {
  if (StaticPrefs::layers_low_precision_buffer()) {
    return GetCriticalDisplayPort(aContent, aResult, aOptions);
  }
  return GetDisplayPort(aContent, aResult, aOptions);
}

void DisplayPortUtils::RemoveDisplayPort(nsIContent* aContent) {
  aContent->RemoveProperty(nsGkAtoms::DisplayPort);
  aContent->RemoveProperty(nsGkAtoms::DisplayPortMargins);
}

bool DisplayPortUtils::ViewportHasDisplayPort(nsPresContext* aPresContext) {
  nsIFrame* rootScrollFrame = aPresContext->PresShell()->GetRootScrollFrame();
  return rootScrollFrame && HasDisplayPort(rootScrollFrame->GetContent());
}

bool DisplayPortUtils::IsFixedPosFrameInDisplayPort(const nsIFrame* aFrame) {
  // Fixed-pos frames are parented by the viewport frame or the page content
  // frame. We'll assume that printing/print preview don't have displayports for
  // their pages!
  nsIFrame* parent = aFrame->GetParent();
  if (!parent || parent->GetParent() ||
      aFrame->StyleDisplay()->mPosition != StylePositionProperty::Fixed) {
    return false;
  }
  return ViewportHasDisplayPort(aFrame->PresContext());
}

// We want to this return true for the scroll frame, but not the
// scrolled frame (which has the same content).
bool DisplayPortUtils::FrameHasDisplayPort(nsIFrame* aFrame,
                                           const nsIFrame* aScrolledFrame) {
  if (!aFrame->GetContent() || !HasDisplayPort(aFrame->GetContent())) {
    return false;
  }
  nsIScrollableFrame* sf = do_QueryFrame(aFrame);
  if (sf) {
    if (aScrolledFrame && aScrolledFrame != sf->GetScrolledFrame()) {
      return false;
    }
    return true;
  }
  return false;
}

bool DisplayPortUtils::CalculateAndSetDisplayPortMargins(
    nsIScrollableFrame* aScrollFrame, RepaintMode aRepaintMode) {
  nsIFrame* frame = do_QueryFrame(aScrollFrame);
  MOZ_ASSERT(frame);
  nsIContent* content = frame->GetContent();
  MOZ_ASSERT(content);

  FrameMetrics metrics =
      nsLayoutUtils::CalculateBasicFrameMetrics(aScrollFrame);
  ScreenMargin displayportMargins = layers::apz::CalculatePendingDisplayPort(
      metrics, ParentLayerPoint(0.0f, 0.0f));
  PresShell* presShell = frame->PresContext()->GetPresShell();
  return SetDisplayPortMargins(
      content, presShell,
      DisplayPortMargins::WithNoAdjustment(displayportMargins), 0,
      aRepaintMode);
}

bool DisplayPortUtils::MaybeCreateDisplayPort(nsDisplayListBuilder* aBuilder,
                                              nsIFrame* aScrollFrame,
                                              RepaintMode aRepaintMode) {
  nsIContent* content = aScrollFrame->GetContent();
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(aScrollFrame);
  if (!content || !scrollableFrame) {
    return false;
  }

  bool haveDisplayPort = HasDisplayPort(content);

  // We perform an optimization where we ensure that at least one
  // async-scrollable frame (i.e. one that WantsAsyncScroll()) has a
  // displayport. If that's not the case yet, and we are async-scrollable, we
  // will get a displayport.
  if (aBuilder->IsPaintingToWindow() &&
      nsLayoutUtils::AsyncPanZoomEnabled(aScrollFrame) &&
      !aBuilder->HaveScrollableDisplayPort() &&
      scrollableFrame->WantAsyncScroll()) {
    // If we don't already have a displayport, calculate and set one.
    if (!haveDisplayPort) {
      CalculateAndSetDisplayPortMargins(scrollableFrame, aRepaintMode);
#ifdef DEBUG
      haveDisplayPort = HasDisplayPort(content);
      MOZ_ASSERT(haveDisplayPort,
                 "should have a displayport after having just set it");
#endif
    }

    // Record that the we now have a scrollable display port.
    aBuilder->SetHaveScrollableDisplayPort();
    return true;
  }
  return false;
}
void DisplayPortUtils::SetZeroMarginDisplayPortOnAsyncScrollableAncestors(
    nsIFrame* aFrame) {
  nsIFrame* frame = aFrame;
  while (frame) {
    frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
    if (!frame) {
      break;
    }
    nsIScrollableFrame* scrollAncestor =
        nsLayoutUtils::GetAsyncScrollableAncestorFrame(frame);
    if (!scrollAncestor) {
      break;
    }
    frame = do_QueryFrame(scrollAncestor);
    MOZ_ASSERT(frame);
    MOZ_ASSERT(scrollAncestor->WantAsyncScroll() ||
               frame->PresShell()->GetRootScrollFrame() == frame);
    if (nsLayoutUtils::AsyncPanZoomEnabled(frame) &&
        !HasDisplayPort(frame->GetContent())) {
      SetDisplayPortMargins(frame->GetContent(), frame->PresShell(),
                            DisplayPortMargins::Empty(), 0,
                            RepaintMode::Repaint);
    }
  }
}

bool DisplayPortUtils::MaybeCreateDisplayPortInFirstScrollFrameEncountered(
    nsIFrame* aFrame, nsDisplayListBuilder* aBuilder) {
  nsIScrollableFrame* sf = do_QueryFrame(aFrame);
  if (sf) {
    if (MaybeCreateDisplayPort(aBuilder, aFrame, RepaintMode::Repaint)) {
      return true;
    }
  }
  if (aFrame->IsPlaceholderFrame()) {
    nsPlaceholderFrame* placeholder = static_cast<nsPlaceholderFrame*>(aFrame);
    if (MaybeCreateDisplayPortInFirstScrollFrameEncountered(
            placeholder->GetOutOfFlowFrame(), aBuilder)) {
      return true;
    }
  }
  if (aFrame->IsSubDocumentFrame()) {
    PresShell* presShell = static_cast<nsSubDocumentFrame*>(aFrame)
                               ->GetSubdocumentPresShellForPainting(0);
    nsIFrame* root = presShell ? presShell->GetRootFrame() : nullptr;
    if (root) {
      if (MaybeCreateDisplayPortInFirstScrollFrameEncountered(root, aBuilder)) {
        return true;
      }
    }
  }
  if (aFrame->IsDeckFrame()) {
    // only descend the visible card of a decks
    nsIFrame* child = static_cast<nsDeckFrame*>(aFrame)->GetSelectedBox();
    if (child) {
      return MaybeCreateDisplayPortInFirstScrollFrameEncountered(child,
                                                                 aBuilder);
    }
  }

  for (nsIFrame* child : aFrame->PrincipalChildList()) {
    if (MaybeCreateDisplayPortInFirstScrollFrameEncountered(child, aBuilder)) {
      return true;
    }
  }

  return false;
}

void DisplayPortUtils::ExpireDisplayPortOnAsyncScrollableAncestor(
    nsIFrame* aFrame) {
  nsIFrame* frame = aFrame;
  while (frame) {
    frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
    if (!frame) {
      break;
    }
    nsIScrollableFrame* scrollAncestor =
        nsLayoutUtils::GetAsyncScrollableAncestorFrame(frame);
    if (!scrollAncestor) {
      break;
    }
    frame = do_QueryFrame(scrollAncestor);
    MOZ_ASSERT(frame);
    if (!frame) {
      break;
    }
    MOZ_ASSERT(scrollAncestor->WantAsyncScroll() ||
               frame->PresShell()->GetRootScrollFrame() == frame);
    if (HasDisplayPort(frame->GetContent())) {
      scrollAncestor->TriggerDisplayPortExpiration();
      // Stop after the first trigger. If it failed, there's no point in
      // continuing because all the rest of the frames we encounter are going
      // to be ancestors of |scrollAncestor| which will keep its displayport.
      // If the trigger succeeded, we stop because when the trigger executes
      // it will call this function again to trigger the next ancestor up the
      // chain.
      break;
    }
  }
}

static PresShell* GetPresShell(const nsIContent* aContent) {
  if (dom::Document* doc = aContent->GetComposedDoc()) {
    return doc->GetPresShell();
  }
  return nullptr;
}

static void UpdateDisplayPortMarginsForPendingMetrics(
    const RepaintRequest& aMetrics) {
  nsIContent* content = nsLayoutUtils::FindContentFor(aMetrics.GetScrollId());
  if (!content) {
    return;
  }

  RefPtr<PresShell> presShell = GetPresShell(content);
  if (!presShell) {
    return;
  }

  if (nsLayoutUtils::AllowZoomingForDocument(presShell->GetDocument()) &&
      aMetrics.IsRootContent()) {
    // See APZCCallbackHelper::UpdateRootFrame for details.
    float presShellResolution = presShell->GetResolution();
    if (presShellResolution != aMetrics.GetPresShellResolution()) {
      return;
    }
  }

  nsIScrollableFrame* frame =
      nsLayoutUtils::FindScrollableFrameFor(aMetrics.GetScrollId());

  if (!frame) {
    return;
  }

  if (APZCCallbackHelper::IsScrollInProgress(frame)) {
    // If these conditions are true, then the UpdateFrame
    // message may be ignored by the main-thread, so we
    // shouldn't update the displayport based on it.
    return;
  }

  DisplayPortMarginsPropertyData* currentData =
      static_cast<DisplayPortMarginsPropertyData*>(
          content->GetProperty(nsGkAtoms::DisplayPortMargins));
  if (!currentData) {
    return;
  }

  CSSPoint frameScrollOffset =
      CSSPoint::FromAppUnits(frame->GetScrollPosition());

  DisplayPortUtils::SetDisplayPortMargins(
      content, presShell,
      DisplayPortMargins::WithAdjustment(
          aMetrics.GetDisplayPortMargins(), aMetrics.GetVisualScrollOffset(),
          frameScrollOffset, aMetrics.DisplayportPixelsPerCSSPixel()),
      0);
}

/* static */
void DisplayPortUtils::UpdateDisplayPortMarginsFromPendingMessages() {
  if (XRE_IsContentProcess() && layers::CompositorBridgeChild::Get() &&
      layers::CompositorBridgeChild::Get()->GetIPCChannel()) {
    layers::CompositorBridgeChild::Get()->GetIPCChannel()->PeekMessages(
        [](const IPC::Message& aMsg) -> bool {
          if (aMsg.type() == layers::PAPZ::Msg_RequestContentRepaint__ID) {
            PickleIterator iter(aMsg);
            RepaintRequest request;
            if (!IPC::ReadParam(&aMsg, &iter, &request)) {
              MOZ_ASSERT(false);
              return true;
            }

            UpdateDisplayPortMarginsForPendingMetrics(request);
          }
          return true;
        });
  }
}

}  // namespace mozilla