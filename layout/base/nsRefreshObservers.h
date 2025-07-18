/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#ifndef LAYOUT_BASE_NSREFRESHOBSERVERS_H_
#define LAYOUT_BASE_NSREFRESHOBSERVERS_H_

#include <functional>

#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h"
#include "nsISupports.h"

namespace mozilla {
class AnimationEventDispatcher;
class PendingFullscreenEvent;
class RefreshDriverTimer;
namespace layout {
class VsyncChild;
}
}  // namespace mozilla

/**
 * An abstract base class to be implemented by callers wanting to be
 * notified at refresh times.  When nothing needs to be painted, callers
 * may not be notified.
 */
class nsARefreshObserver {
 public:
  // AddRef and Release signatures that match nsISupports.  Implementors
  // must implement reference counting, and those that do implement
  // nsISupports will already have methods with the correct signature.
  //
  // The refresh driver does NOT hold references to refresh observers
  // except while it is notifying them.
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  MOZ_CAN_RUN_SCRIPT virtual void WillRefresh(mozilla::TimeStamp aTime) = 0;
};

/**
 * An abstract base class to be implemented by callers wanting to be notified
 * when the observing refresh driver updated mMostRecentRefresh due to active
 * timer changes. Callers must ensure an observer is removed before it is
 * destroyed.
 */
class nsATimerAdjustmentObserver {
 public:
  virtual void NotifyTimerAdjusted(mozilla::TimeStamp aTime) = 0;
};

/**
 * An abstract base class to be implemented by callers wanting to be notified
 * that a refresh has occurred. Callers must ensure an observer is removed
 * before it is destroyed.
 */
class nsAPostRefreshObserver {
 public:
  virtual void DidRefresh() = 0;
};

#endif
