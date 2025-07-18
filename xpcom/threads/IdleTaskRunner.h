/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IdleTaskRunner_h
#define IdleTaskRunner_h

#include "mozilla/TimeStamp.h"
#include "mozilla/TaskCategory.h"
#include "nsThreadUtils.h"
#include <functional>

namespace mozilla {

// A general purpose repeating callback runner (it can be configured to a
// one-time runner, too.) If it is running repeatedly, one has to either
// explicitly Cancel() the runner or have MayStopProcessing() callback return
// true to completely remove the runner.
class IdleTaskRunner final : public CancelableIdleRunnable {
 public:
  // Return true if some meaningful work was done.
  using CallbackType = std::function<bool(TimeStamp aDeadline)>;

  // A callback for "stop processing" decision. Return true to
  // stop processing. This can be an alternative to Cancel() or
  // work together in different way.
  using MayStopProcessingCallbackType = std::function<bool()>;

 public:
  // An IdleTaskRunner has (up to) three phases:
  //
  //  - (duration aStartDelay) waiting to run (aStartDelay can be zero)
  //
  //  - (duration aMaxDelay) attempting to find a long enough amount of idle
  //    time, at least aMinimumUsefulBudget
  //
  //  - overdue for idle time, run as soon as possible
  //
  // If aRepeating is true, then aStartDelay applies only to the first run; the
  // second run will attempt to run in the first idle slice that is long
  // enough.
  //
  // All durations are in milliseconds.
  //
  static already_AddRefed<IdleTaskRunner> Create(
      const CallbackType& aCallback, const char* aRunnableName,
      uint32_t aStartDelay, uint32_t aMaxDelay, int64_t aMinimumUsefulBudget,
      bool aRepeating, const MayStopProcessingCallbackType& aMayStopProcessing,
      TaskCategory aTaskCategory = TaskCategory::Count);

  NS_IMETHOD Run() override;

  // (Used by the task triggering code.) Record the end of the current idle
  // period, or null if not running during idle time.
  void SetDeadline(mozilla::TimeStamp aDeadline) override;

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override;

  // Update the minimum idle time that this callback would be invoked for.
  void SetMinimumUsefulBudget(int64_t aMinimumUsefulBudget);

  nsresult Cancel() override;
  void Schedule(bool aAllowIdleDispatch);

 private:
  explicit IdleTaskRunner(
      const CallbackType& aCallback, const char* aRunnableName,
      uint32_t aStartDelay, uint32_t aMaxDelay, int64_t aMinimumUsefulBudget,
      bool aRepeating, const MayStopProcessingCallbackType& aMayStopProcessing,
      TaskCategory aTaskCategory);
  ~IdleTaskRunner();
  void CancelTimer();
  void SetTimerInternal(uint32_t aDelay);

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mScheduleTimer;
  CallbackType mCallback;

  // Do not run until this time.
  const mozilla::TimeStamp mStartTime;

  // Wait this long for idle time before giving up and running a non-idle
  // callback.
  uint32_t mMaxDelay;

  // If running during idle time, the expected end of the current idle period.
  // The null timestamp when the run is triggered by aMaxDelay instead of idle.
  TimeStamp mDeadline;

  // The least duration worth calling the callback for during idle time.
  TimeDuration mMinimumUsefulBudget;

  bool mRepeating;
  bool mTimerActive;
  MayStopProcessingCallbackType mMayStopProcessing;
  const TaskCategory mTaskCategory;
  const char* mName;
};

}  // end of namespace mozilla.

#endif
