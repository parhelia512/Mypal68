/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZUtils_h
#define mozilla_layers_APZUtils_h

// This file is for APZ-related utilities that are used by code in gfx/layers
// only. For APZ-related utilities used by the Rest of the World (widget/,
// layout/, dom/, IPDL protocols, etc.), use APZPublicUtils.h.
// Do not include this header from source files outside of gfx/layers.

#include <stdint.h>  // for uint32_t
#include <type_traits>
#include "FrameMetrics.h"
#include "LayersTypes.h"
#include "UnitTransforms.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/EnumSet.h"
#include "mozilla/FloatingPoint.h"

namespace mozilla {

namespace layers {

enum CancelAnimationFlags : uint32_t {
  Default = 0x0,             /* Cancel all animations */
  ExcludeOverscroll = 0x1,   /* Don't clear overscroll */
  ScrollSnap = 0x2,          /* Snap to snap points */
  ExcludeWheel = 0x4,        /* Don't stop wheel smooth-scroll animations */
  TriggeredExternally = 0x8, /* Cancellation was not triggered by APZ in
                                response to an input event */
};

inline CancelAnimationFlags operator|(CancelAnimationFlags a,
                                      CancelAnimationFlags b) {
  return static_cast<CancelAnimationFlags>(static_cast<int>(a) |
                                           static_cast<int>(b));
}

typedef EnumSet<ScrollDirection> ScrollDirections;

// clang-format off
enum class ScrollSource {
  // scrollTo() or something similar.
  DOM,

  // Touch-screen or trackpad with gesture support.
  Touch,

  // Mouse wheel.
  Wheel,

  // Keyboard
  Keyboard,
};
// clang-format on

// Epsilon to be used when comparing 'float' coordinate values
// with FuzzyEqualsAdditive. The rationale is that 'float' has 7 decimal
// digits of precision, and coordinate values should be no larger than in the
// ten thousands. Note also that the smallest legitimate difference in page
// coordinates is 1 app unit, which is 1/60 of a (CSS pixel), so this epsilon
// isn't too large.
const float COORDINATE_EPSILON = 0.02f;

template <typename Units>
static bool IsZero(const gfx::PointTyped<Units>& aPoint) {
  return FuzzyEqualsAdditive(aPoint.x, 0.0f, COORDINATE_EPSILON) &&
         FuzzyEqualsAdditive(aPoint.y, 0.0f, COORDINATE_EPSILON);
}

// Represents async transforms consisting of a scale and a translation.
struct AsyncTransform {
  explicit AsyncTransform(
      LayerToParentLayerScale aScale = LayerToParentLayerScale(),
      ParentLayerPoint aTranslation = ParentLayerPoint())
      : mScale(aScale), mTranslation(aTranslation) {}

  operator AsyncTransformComponentMatrix() const {
    return AsyncTransformComponentMatrix::Scaling(mScale.scale, mScale.scale, 1)
        .PostTranslate(mTranslation.x, mTranslation.y, 0);
  }

  bool operator==(const AsyncTransform& rhs) const {
    return mTranslation == rhs.mTranslation && mScale == rhs.mScale;
  }

  bool operator!=(const AsyncTransform& rhs) const { return !(*this == rhs); }

  LayerToParentLayerScale mScale;
  ParentLayerPoint mTranslation;
};

// Deem an AsyncTransformComponentMatrix (obtained by multiplying together
// one or more AsyncTransformComponentMatrix objects) as constituting a
// complete async transform.
inline AsyncTransformMatrix CompleteAsyncTransform(
    const AsyncTransformComponentMatrix& aMatrix) {
  return ViewAs<AsyncTransformMatrix>(
      aMatrix, PixelCastJustification::MultipleAsyncTransforms);
}

struct TargetConfirmationFlags final {
  explicit TargetConfirmationFlags(bool aTargetConfirmed)
      : mTargetConfirmed(aTargetConfirmed),
        mRequiresTargetConfirmation(false),
        mHitScrollbar(false),
        mHitScrollThumb(false) {}

  explicit TargetConfirmationFlags(
      const gfx::CompositorHitTestInfo& aHitTestInfo)
      : mTargetConfirmed(
            (aHitTestInfo != gfx::CompositorHitTestInvisibleToHit) &&
            (aHitTestInfo & gfx::CompositorHitTestDispatchToContent).isEmpty()),
        mRequiresTargetConfirmation(aHitTestInfo.contains(
            gfx::CompositorHitTestFlags::eRequiresTargetConfirmation)),
        mHitScrollbar(
            aHitTestInfo.contains(gfx::CompositorHitTestFlags::eScrollbar)),
        mHitScrollThumb(aHitTestInfo.contains(
            gfx::CompositorHitTestFlags::eScrollbarThumb)) {}

  bool mTargetConfirmed : 1;
  bool mRequiresTargetConfirmation : 1;
  bool mHitScrollbar : 1;
  bool mHitScrollThumb : 1;
};

enum class AsyncTransformComponent { eLayout, eVisual };

using AsyncTransformComponents = EnumSet<AsyncTransformComponent>;

constexpr AsyncTransformComponents LayoutAndVisual(
    AsyncTransformComponent::eLayout, AsyncTransformComponent::eVisual);

namespace apz {

/**
 * Is aAngle within the given threshold of the horizontal axis?
 * @param aAngle an angle in radians in the range [0, pi]
 * @param aThreshold an angle in radians in the range [0, pi/2]
 */
bool IsCloseToHorizontal(float aAngle, float aThreshold);

// As above, but for the vertical axis.
bool IsCloseToVertical(float aAngle, float aThreshold);

}  // namespace apz

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_APZUtils_h
