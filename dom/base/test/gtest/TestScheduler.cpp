/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/CCGCScheduler.h"
#include "mozilla/TimeStamp.h"

// This is a test for mozilla::CCGCScheduler.

using namespace mozilla;

static TimeDuration kOneSecond = TimeDuration::FromSeconds(1);
static TimeDuration kTenthSecond = TimeDuration::FromSeconds(0.1);
static TimeDuration kFrameDuration = TimeDuration::FromSeconds(1.0 / 60.0);

static mozilla::TimeStamp sNow = TimeStamp::Now();

static mozilla::TimeStamp sStartupTime = sNow;

inline mozilla::TimeStamp mozilla::CCGCScheduler::Now() { return sNow; }

static mozilla::TimeStamp AdvanceTime(TimeDuration aDuration) {
  sNow += aDuration;
  return sNow;
}

static uint32_t sSuspected = 0;

inline uint32_t mozilla::CCGCScheduler::SuspectedCCObjects() {
  return sSuspected;
}
static void SetNumSuspected(uint32_t n) { sSuspected = n; }
static void SuspectMore(uint32_t n) { sSuspected += n; }

using CCRunnerState = mozilla::CCGCScheduler::CCRunnerState;

static TimeStamp Now() { return sNow; }

class TestGC {
 protected:
  CCGCScheduler& mScheduler;

 public:
  explicit TestGC(CCGCScheduler& aScheduler) : mScheduler(aScheduler) {}
  void Run(int aNumSlices);
};

void TestGC::Run(int aNumSlices) {
  // Make the purple buffer nearly empty so it is itself not an adequate reason
  // for wanting a CC.
  static_assert(3 < mozilla::kCCPurpleLimit);
  SetNumSuspected(3);

  // Running the GC should not influence whether a CC is currently seen as
  // needed. But the first time we run GC, it will be false; later, we will
  // have run a GC and set it to true.
  bool neededCCAtStartOfGC = mScheduler.IsCCNeeded();

  mScheduler.NoteGCBegin();

  for (int slice = 0; slice < aNumSlices; slice++) {
    EXPECT_TRUE(mScheduler.InIncrementalGC());
    TimeStamp idleDeadline = Now() + kTenthSecond;
    TimeDuration budget =
        mScheduler.ComputeInterSliceGCBudget(idleDeadline, Now());
    EXPECT_NEAR(budget.ToSeconds(), 0.1, 1.e-6);
    // Pretend the GC took exactly the budget.
    AdvanceTime(budget);

    EXPECT_EQ(mScheduler.IsCCNeeded(), neededCCAtStartOfGC);

    // Mutator runs for 1 second.
    AdvanceTime(kOneSecond);
  }

  mScheduler.NoteGCEnd();
  mScheduler.SetNeedsFullGC(false);
}

class TestCC {
 protected:
  CCGCScheduler& mScheduler;

 public:
  explicit TestCC(CCGCScheduler& aScheduler) : mScheduler(aScheduler) {}

  void Run(int aNumSlices) {
    Prepare();
    MaybePokeCC();
    TimerFires(aNumSlices);
    EndCycleCollectionCallback();
    KillCCRunner();
  }

  virtual void Prepare() = 0;
  virtual void MaybePokeCC();
  virtual void TimerFires(int aNumSlices);
  virtual void RunSlices(int aNumSlices);
  virtual void RunSlice(TimeStamp aCCStartTime, TimeStamp aPrevSliceEnd,
                        int aSliceNum, int aNumSlices) = 0;
  virtual void ForgetSkippable();
  virtual void EndCycleCollectionCallback();
  virtual void KillCCRunner();
};

void TestCC::MaybePokeCC() {
  // nsJSContext::MaybePokeCC
  EXPECT_TRUE(mScheduler.ShouldScheduleCC());

  mScheduler.InitCCRunnerStateMachine(CCRunnerState::ReducePurple);
  EXPECT_TRUE(mScheduler.IsEarlyForgetSkippable());
}

void TestCC::TimerFires(int aNumSlices) {
  // Series of CCRunner timer fires.
  CCRunnerStep step;

  while (true) {
    SuspectMore(1000);
    TimeStamp idleDeadline = Now() + kOneSecond;
    step = mScheduler.GetNextCCRunnerAction(idleDeadline);
    // Should first see a series of ForgetSkippable actions.
    if (step.mAction != CCRunnerAction::ForgetSkippable ||
        step.mRemoveChildless != KeepChildless) {
      break;
    }
    EXPECT_EQ(step.mYield, Yield);
    ForgetSkippable();
  }

  while (step.mYield == Continue) {
    TimeStamp idleDeadline = Now() + kOneSecond;
    step = mScheduler.GetNextCCRunnerAction(idleDeadline);
  }
  EXPECT_EQ(step.mAction, CCRunnerAction::ForgetSkippable);
  EXPECT_EQ(step.mRemoveChildless, RemoveChildless);
  ForgetSkippable();

  TimeStamp idleDeadline = Now() + kOneSecond;
  step = mScheduler.GetNextCCRunnerAction(idleDeadline);
  EXPECT_EQ(step.mAction, CCRunnerAction::CleanupContentUnbinder);
  step = mScheduler.GetNextCCRunnerAction(idleDeadline);
  EXPECT_EQ(step.mAction, CCRunnerAction::CleanupDeferred);

  RunSlices(aNumSlices);
}

void TestCC::ForgetSkippable() {
  uint32_t suspectedBefore = sSuspected;
  // ...ForgetSkippable would happen here...
  js::SliceBudget budget =
      mScheduler.ComputeForgetSkippableBudget(Now(), Now() + kTenthSecond);
  EXPECT_NEAR(budget.timeBudget(), kTenthSecond.ToMilliseconds(), 1);
  AdvanceTime(kTenthSecond);
  mScheduler.NoteForgetSkippableComplete(Now(), suspectedBefore);
}

void TestCC::RunSlices(int aNumSlices) {
  TimeStamp ccStartTime = Now();
  TimeStamp prevSliceEnd = ccStartTime;
  for (int ccslice = 0; ccslice < aNumSlices; ccslice++) {
    RunSlice(ccStartTime, prevSliceEnd, ccslice, aNumSlices);
    prevSliceEnd = Now();
  }

  SetNumSuspected(0);
}

void TestCC::EndCycleCollectionCallback() {
  // nsJSContext::EndCycleCollectionCallback
  CycleCollectorResults results;
  results.mFreedGCed = 10;
  results.mFreedJSZones = 2;
  mScheduler.NoteCycleCollected(results);

  // Because > 0 zones were freed.
  EXPECT_TRUE(mScheduler.NeedsGCAfterCC());
}

void TestCC::KillCCRunner() {
  // nsJSContext::KillCCRunner
  mScheduler.UnblockCC();
  mScheduler.DeactivateCCRunner();
  mScheduler.NoteCCEnd(Now());
}

class TestIdleCC : public TestCC {
 public:
  explicit TestIdleCC(CCGCScheduler& aScheduler) : TestCC(aScheduler) {}

  void Prepare() override;
  void RunSlice(TimeStamp aCCStartTime, TimeStamp aPrevSliceEnd, int aSliceNum,
                int aNumSlices) override;
};

void TestIdleCC::Prepare() { EXPECT_TRUE(!mScheduler.InIncrementalGC()); }

void TestIdleCC::RunSlice(TimeStamp aCCStartTime, TimeStamp aPrevSliceEnd,
                          int aSliceNum, int aNumSlices) {
  CCRunnerStep step;
  TimeStamp idleDeadline = Now() + kTenthSecond;

  // The scheduler should request a CycleCollect slice.
  step = mScheduler.GetNextCCRunnerAction(idleDeadline);
  EXPECT_EQ(step.mAction, CCRunnerAction::CycleCollect);

  // nsJSContext::RunCycleCollectorSlice

  EXPECT_FALSE(mScheduler.InIncrementalGC());
  bool preferShorter;
  js::SliceBudget budget = mScheduler.ComputeCCSliceBudget(
      idleDeadline, aCCStartTime, aPrevSliceEnd, &preferShorter);
  // The scheduler will set the budget to our deadline (0.1sec in the future).
  EXPECT_NEAR(budget.timeBudget(), kTenthSecond.ToMilliseconds(), 1);
  EXPECT_FALSE(preferShorter);

  AdvanceTime(kTenthSecond);
}

class TestNonIdleCC : public TestCC {
 public:
  explicit TestNonIdleCC(CCGCScheduler& aScheduler) : TestCC(aScheduler) {}

  void Prepare() override;
  void RunSlice(TimeStamp aCCStartTime, TimeStamp aPrevSliceEnd, int aSliceNum,
                int aNumSlices) override;
};

void TestNonIdleCC::Prepare() {
  EXPECT_TRUE(!mScheduler.InIncrementalGC());

  // Advance time by an hour to give time for a user event in the past.
  AdvanceTime(TimeDuration::FromSeconds(3600));
}

void TestNonIdleCC::RunSlice(TimeStamp aCCStartTime, TimeStamp aPrevSliceEnd,
                             int aSliceNum, int aNumSlices) {
  CCRunnerStep step;
  TimeStamp nullDeadline;

  // The scheduler should tell us to run a slice of cycle collection.
  step = mScheduler.GetNextCCRunnerAction(nullDeadline);
  EXPECT_EQ(step.mAction, CCRunnerAction::CycleCollect);

  // nsJSContext::RunCycleCollectorSlice

  EXPECT_FALSE(mScheduler.InIncrementalGC());

  bool preferShorter;
  js::SliceBudget budget = mScheduler.ComputeCCSliceBudget(
      nullDeadline, aCCStartTime, aPrevSliceEnd, &preferShorter);
  if (aSliceNum == 0) {
    // First slice of the CC, so always use the baseBudget which is
    // kICCSliceBudget (3ms) for a non-idle slice.
    EXPECT_NEAR(budget.timeBudget(), kICCSliceBudget.ToMilliseconds(), 0.1);
  } else if (aSliceNum == 1) {
    // Second slice still uses the baseBudget, since not much time has passed
    // so none of the lengthening mechanisms have kicked in yet.
    EXPECT_NEAR(budget.timeBudget(), kICCSliceBudget.ToMilliseconds(), 0.1);
  } else if (aSliceNum == 2) {
    // We're not overrunning kMaxICCDuration, so we don't go unlimited.
    EXPECT_FALSE(budget.isUnlimited());
    // This slice is delayed by twice the allowed amount. Slice time should be
    // doubled.
    EXPECT_NEAR(budget.timeBudget(), kICCSliceBudget.ToMilliseconds() * 2, 0.1);
  } else {
    // We're not overrunning kMaxICCDuration, so we don't go unlimited.
    EXPECT_FALSE(budget.isUnlimited());

    // These slices are not delayed, but enough time has passed that the
    // dominating factor is now the linear ramp up to max slice time at the
    // halfway point to kMaxICCDuration.
    EXPECT_TRUE(budget.timeBudget() > kICCSliceBudget.ToMilliseconds());
    EXPECT_TRUE(budget.timeBudget() <=
                MainThreadIdlePeriod::GetLongIdlePeriod());
  }
  EXPECT_TRUE(preferShorter);  // Non-idle prefers shorter slices

  AdvanceTime(TimeDuration::FromMilliseconds(budget.timeBudget()));
  if (aSliceNum == 1) {
    // Delay the third slice (only).
    AdvanceTime(kICCIntersliceDelay * 2);
  }
}

// Do a GC then CC then GC.
static bool BasicScenario(CCGCScheduler& aScheduler, TestGC* aTestGC,
                          TestCC* aTestCC) {
  // Run a 10-slice incremental GC.
  aTestGC->Run(10);

  // After a GC, the scheduler should decide to do a full CC regardless of the
  // number of purple buffer entries.
  SetNumSuspected(3);
  EXPECT_TRUE(aScheduler.IsCCNeeded());

  // Now we should want to CC.
  EXPECT_TRUE(aScheduler.ShouldScheduleCC());

  // Do a 5-slice CC.
  aTestCC->Run(5);

  // Not enough suspected objects to deserve a CC.
  EXPECT_FALSE(aScheduler.IsCCNeeded());
  EXPECT_FALSE(aScheduler.ShouldScheduleCC());
  SetNumSuspected(10000);

  // We shouldn't want to CC again yet, it's too soon.
  EXPECT_FALSE(aScheduler.ShouldScheduleCC());
  AdvanceTime(mozilla::kCCDelay);

  // *Now* it's time for another CC.
  EXPECT_TRUE(aScheduler.ShouldScheduleCC());

  // Run a 3-slice incremental GC.
  EXPECT_TRUE(!aScheduler.InIncrementalGC());
  aTestGC->Run(3);

  return true;
}

static CCGCScheduler scheduler;
static TestGC gc(scheduler);
static TestIdleCC ccIdle(scheduler);
static TestNonIdleCC ccNonIdle(scheduler);

TEST(TestScheduler, Idle)
{
  // Cannot CC until we GC once.
  EXPECT_FALSE(scheduler.ShouldScheduleCC());

  EXPECT_TRUE(BasicScenario(scheduler, &gc, &ccIdle));
}

TEST(TestScheduler, NonIdle)
{ EXPECT_TRUE(BasicScenario(scheduler, &gc, &ccNonIdle)); }
