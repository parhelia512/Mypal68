/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameMetrics.h"

#include "gfxUtils.h"
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"
#include "mozilla/WritingModes.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace layers {

const ScrollableLayerGuid::ViewID ScrollableLayerGuid::NULL_SCROLL_ID = 0;

void FrameMetrics::RecalculateLayoutViewportOffset() {
  // For subframes, the visual and layout viewports coincide, so just
  // keep the layout viewport offset in sync with the visual one.
  if (!mIsRootContent) {
    mLayoutViewport.MoveTo(GetVisualScrollOffset());
    return;
  }
  // For the root, the two viewports can diverge, but the layout
  // viewport needs to keep enclosing the visual viewport.
  KeepLayoutViewportEnclosingVisualViewport(GetVisualViewport(),
                                            mScrollableRect, mLayoutViewport);
}

/* static */
void FrameMetrics::KeepLayoutViewportEnclosingVisualViewport(
    const CSSRect& aVisualViewport, const CSSRect& aScrollableRect,
    CSSRect& aLayoutViewport) {
  // If the visual viewport is contained within the layout viewport, we don't
  // need to make any adjustments, so we can exit early.
  //
  // Additionally, if the composition bounds changes (due to an orientation
  // change, window resize, etc.), it may take a few frames for aLayoutViewport
  // to update and during that time, the visual viewport may be larger than the
  // layout viewport. In such situations, we take an early exit if the visual
  // viewport contains the layout viewport.
  if (aLayoutViewport.Contains(aVisualViewport) ||
      aVisualViewport.Contains(aLayoutViewport)) {
    return;
  }

  // If visual viewport size is greater than the layout viewport, move the
  // layout viewport such that it remains inside the visual viewport. Otherwise,
  // move the layout viewport such that the visual viewport is contained
  // inside the layout viewport.
  if ((aLayoutViewport.Width() < aVisualViewport.Width() &&
       !FuzzyEqualsMultiplicative(aLayoutViewport.Width(),
                                  aVisualViewport.Width())) ||
      (aLayoutViewport.Height() < aVisualViewport.Height() &&
       !FuzzyEqualsMultiplicative(aLayoutViewport.Height(),
                                  aVisualViewport.Height()))) {
    if (aLayoutViewport.X() < aVisualViewport.X()) {
      // layout viewport moves right
      aLayoutViewport.MoveToX(aVisualViewport.X());
    } else if (aVisualViewport.XMost() < aLayoutViewport.XMost()) {
      // layout viewport moves left
      aLayoutViewport.MoveByX(aVisualViewport.XMost() -
                              aLayoutViewport.XMost());
    }
    if (aLayoutViewport.Y() < aVisualViewport.Y()) {
      // layout viewport moves down
      aLayoutViewport.MoveToY(aVisualViewport.Y());
    } else if (aVisualViewport.YMost() < aLayoutViewport.YMost()) {
      // layout viewport moves up
      aLayoutViewport.MoveByY(aVisualViewport.YMost() -
                              aLayoutViewport.YMost());
    }
  } else {
    if (aVisualViewport.X() < aLayoutViewport.X()) {
      aLayoutViewport.MoveToX(aVisualViewport.X());
    } else if (aLayoutViewport.XMost() < aVisualViewport.XMost()) {
      aLayoutViewport.MoveByX(aVisualViewport.XMost() -
                              aLayoutViewport.XMost());
    }
    if (aVisualViewport.Y() < aLayoutViewport.Y()) {
      aLayoutViewport.MoveToY(aVisualViewport.Y());
    } else if (aLayoutViewport.YMost() < aVisualViewport.YMost()) {
      aLayoutViewport.MoveByY(aVisualViewport.YMost() -
                              aLayoutViewport.YMost());
    }
  }

  // Regardless of any adjustment above, the layout viewport is not allowed
  // to go outside the scrollable rect.
  aLayoutViewport = aLayoutViewport.MoveInsideAndClamp(aScrollableRect);
}

void FrameMetrics::ApplyScrollUpdateFrom(const ScrollPositionUpdate& aUpdate) {
  // In applying a main-thread scroll update, try to preserve the relative
  // offset between the visual and layout viewports.
  CSSPoint relativeOffset = GetVisualScrollOffset() - GetLayoutScrollOffset();
  MOZ_ASSERT(IsRootContent() || relativeOffset == CSSPoint());
  // We need to set the two offsets together, otherwise a subsequent
  // RecalculateLayoutViewportOffset() could see divergent layout and
  // visual offsets.
  SetLayoutScrollOffset(aUpdate.GetDestination());
  ClampAndSetVisualScrollOffset(aUpdate.GetDestination() + relativeOffset);
}

CSSPoint FrameMetrics::ApplyRelativeScrollUpdateFrom(
    const ScrollPositionUpdate& aUpdate) {
  MOZ_ASSERT(aUpdate.GetType() == ScrollUpdateType::Relative);
  CSSPoint origin = GetVisualScrollOffset();
  CSSPoint delta = (aUpdate.GetDestination() - aUpdate.GetSource());
  ClampAndSetVisualScrollOffset(origin + delta);
  return GetVisualScrollOffset() - origin;
}

CSSPoint FrameMetrics::ApplyPureRelativeScrollUpdateFrom(
    const ScrollPositionUpdate& aUpdate) {
  MOZ_ASSERT(aUpdate.GetType() == ScrollUpdateType::PureRelative);
  CSSPoint origin = GetVisualScrollOffset();
  ClampAndSetVisualScrollOffset(origin + aUpdate.GetDelta());
  return GetVisualScrollOffset() - origin;
}

void FrameMetrics::UpdatePendingScrollInfo(const ScrollPositionUpdate& aInfo) {
  // We only get this "pending scroll info" for paint-skip transactions,
  // but PureRelative position updates always trigger a full paint, so
  // we should never enter this code with a PureRelative update type. For
  // the other types, the destination field on the ScrollPositionUpdate will
  // tell us the final layout scroll position on the main thread.
  MOZ_ASSERT(aInfo.GetType() != ScrollUpdateType::PureRelative);

  // In applying a main-thread scroll update, try to preserve the relative
  // offset between the visual and layout viewports.
  CSSPoint relativeOffset = GetVisualScrollOffset() - GetLayoutScrollOffset();
  MOZ_ASSERT(IsRootContent() || relativeOffset == CSSPoint());

  SetLayoutScrollOffset(aInfo.GetDestination());
  ClampAndSetVisualScrollOffset(aInfo.GetDestination() + relativeOffset);
  mScrollGeneration = aInfo.GetGeneration();
}

ScrollSnapInfo::ScrollSnapInfo()
    : mScrollSnapStrictnessX(StyleScrollSnapStrictness::None),
      mScrollSnapStrictnessY(StyleScrollSnapStrictness::None) {}

bool ScrollSnapInfo::HasScrollSnapping() const {
  return mScrollSnapStrictnessY != StyleScrollSnapStrictness::None ||
         mScrollSnapStrictnessX != StyleScrollSnapStrictness::None;
}

bool ScrollSnapInfo::HasSnapPositions() const {
  return (!mSnapPositionX.IsEmpty() &&
          mScrollSnapStrictnessX != StyleScrollSnapStrictness::None) ||
         (!mSnapPositionY.IsEmpty() &&
          mScrollSnapStrictnessY != StyleScrollSnapStrictness::None);
}

void ScrollSnapInfo::InitializeScrollSnapStrictness(
    WritingMode aWritingMode, const nsStyleDisplay* aDisplay) {
  if (aDisplay->mScrollSnapType.strictness == StyleScrollSnapStrictness::None) {
    return;
  }

  mScrollSnapStrictnessX = StyleScrollSnapStrictness::None;
  mScrollSnapStrictnessY = StyleScrollSnapStrictness::None;

  switch (aDisplay->mScrollSnapType.axis) {
    case StyleScrollSnapAxis::X:
      mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Y:
      mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      break;
    case StyleScrollSnapAxis::Block:
      if (aWritingMode.IsVertical()) {
        mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Inline:
      if (aWritingMode.IsVertical()) {
        mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      } else {
        mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      }
      break;
    case StyleScrollSnapAxis::Both:
      mScrollSnapStrictnessX = aDisplay->mScrollSnapType.strictness;
      mScrollSnapStrictnessY = aDisplay->mScrollSnapType.strictness;
      break;
  }
}

static OverscrollBehavior ToOverscrollBehavior(
    StyleOverscrollBehavior aBehavior) {
  switch (aBehavior) {
    case StyleOverscrollBehavior::Auto:
      return OverscrollBehavior::Auto;
    case StyleOverscrollBehavior::Contain:
      return OverscrollBehavior::Contain;
    case StyleOverscrollBehavior::None:
      return OverscrollBehavior::None;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid overscroll behavior");
  return OverscrollBehavior::Auto;
}

OverscrollBehaviorInfo OverscrollBehaviorInfo::FromStyleConstants(
    StyleOverscrollBehavior aBehaviorX, StyleOverscrollBehavior aBehaviorY) {
  OverscrollBehaviorInfo result;
  result.mBehaviorX = ToOverscrollBehavior(aBehaviorX);
  result.mBehaviorY = ToOverscrollBehavior(aBehaviorY);
  return result;
}

StaticAutoPtr<const ScrollMetadata> ScrollMetadata::sNullMetadata;

}  // namespace layers
}  // namespace mozilla
