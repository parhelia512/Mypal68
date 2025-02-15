/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmoothScrollAnimation.h"
#include "ScrollAnimationBezierPhysics.h"
#include "mozilla/layers/APZPublicUtils.h"

namespace mozilla {
namespace layers {

SmoothScrollAnimation::SmoothScrollAnimation(AsyncPanZoomController& aApzc,
                                             const nsPoint& aInitialPosition,
                                             ScrollOrigin aOrigin)
    : GenericScrollAnimation(
          aApzc, aInitialPosition,
          apz::ComputeBezierAnimationSettingsForOrigin(aOrigin)),
      mOrigin(aOrigin) {}

SmoothScrollAnimation* SmoothScrollAnimation::AsSmoothScrollAnimation() {
  return this;
}

ScrollOrigin SmoothScrollAnimation::GetScrollOrigin() const { return mOrigin; }

}  // namespace layers
}  // namespace mozilla
