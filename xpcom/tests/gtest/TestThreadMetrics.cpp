/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/TaskCategory.h"
#include "mozilla/PerformanceCounter.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsThread.h"

using namespace mozilla;
using mozilla::Runnable;
using mozilla::dom::DocGroup;

/* A struct that describes a runnable to run and, optionally, a
 * docgroup to dispatch it to.
 */
struct RunnableDescriptor {
  MOZ_IMPLICIT RunnableDescriptor(nsIRunnable* aRunnable,
                                  DocGroup* aDocGroup = nullptr)
      : mRunnable(aRunnable), mDocGroup(aDocGroup) {}

  RunnableDescriptor(RunnableDescriptor&& aDescriptor)
      : mRunnable(std::move(aDescriptor.mRunnable)),
        mDocGroup(std::move(aDescriptor.mDocGroup)) {}

  nsCOMPtr<nsIRunnable> mRunnable;
  RefPtr<DocGroup> mDocGroup;
};

class MockSchedulerGroup : public SchedulerGroup {
 public:
  explicit MockSchedulerGroup(mozilla::dom::DocGroup* aDocGroup)
      : mDocGroup(aDocGroup) {}
  NS_INLINE_DECL_REFCOUNTING(MockSchedulerGroup);

  MOCK_METHOD1(SetValidatingAccess, void(ValidationType aType));
  mozilla::dom::DocGroup* DocGroup() { return mDocGroup; }

 protected:
  virtual ~MockSchedulerGroup() = default;

 private:
  mozilla::dom::DocGroup* mDocGroup;
};

typedef testing::NiceMock<MockSchedulerGroup> MSchedulerGroup;

/* Timed runnable which simulates some execution time
 * and can run some nested runnables.
 */
class TimedRunnable final : public Runnable {
 public:
  explicit TimedRunnable(uint32_t aExecutionTime1, uint32_t aExecutionTime2)
      : Runnable("TimedRunnable"),
        mExecutionTime1(aExecutionTime1),
        mExecutionTime2(aExecutionTime2) {}

  NS_IMETHODIMP Run() {
    PR_Sleep(PR_MillisecondsToInterval(mExecutionTime1 + 5));
    for (uint32_t index = 0; index < mNestedRunnables.Length(); ++index) {
      if (index != 0) {
        PR_Sleep(PR_MillisecondsToInterval(mExecutionTime1 + 5));
      }
      (void)DispatchNestedRunnable(mNestedRunnables[index].mRunnable,
                                   mNestedRunnables[index].mDocGroup);
    }
    PR_Sleep(PR_MillisecondsToInterval(mExecutionTime2 + 5));
    return NS_OK;
  }

  void AddNestedRunnable(RunnableDescriptor aDescriptor) {
    mNestedRunnables.AppendElement(std::move(aDescriptor));
  }

  static void DispatchNestedRunnable(nsIRunnable* aRunnable,
                                     DocGroup* aDocGroup) {
    // Dispatch another runnable so nsThread::ProcessNextEvent is called
    // recursively
    nsCOMPtr<nsIThread> thread = do_GetMainThread();
    if (aDocGroup) {
      (void)DispatchWithDocgroup(aRunnable, aDocGroup);
    } else {
      thread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
    }
    (void)NS_ProcessNextEvent(thread, false);
  }

  static nsresult DispatchWithDocgroup(nsIRunnable* aRunnable,
                                       DocGroup* aDocGroup) {
    nsCOMPtr<nsIRunnable> runnable = aRunnable;
    runnable = new SchedulerGroup::Runnable(runnable.forget(), mSchedulerGroup,
                                            mDocGroup);
    return aDocGroup->Dispatch(TaskCategory::Other, runnable.forget());
  }

 private:
  uint32_t mExecutionTime1;
  uint32_t mExecutionTime2;
  nsTArray<RunnableDescriptor> mNestedRunnables;
};

/* test class used for all metrics tests
 *
 * - sets up the enable_scheduler_timing pref
 * - provides a function to dispatch runnables and spin the loop
 */

class ThreadMetrics : public ::testing::Test {
 public:
  explicit ThreadMetrics() = default;

 protected:
  virtual void SetUp() {
    // building the TabGroup/DocGroup structure
    RefPtr<dom::TabGroup> tabGroup = new dom::TabGroup(false);
    mDocGroup = tabGroup->AddDocument(NS_LITERAL_CSTRING("key"), nullptr);
    mDocGroup2 = tabGroup->AddDocument(NS_LITERAL_CSTRING("key2"), nullptr);
    mSchedulerGroup = new MSchedulerGroup(mDocGroup);
    mCounter = mDocGroup->GetPerformanceCounter();
    mCounter2 = mDocGroup2->GetPerformanceCounter();
    mThreadMgr = do_GetService("@mozilla.org/thread-manager;1");
    mOther = DispatchCategory(TaskCategory::Other).GetValue();
    mDispatchCount = (uint32_t)TaskCategory::Other + 1;
  }

  virtual void TearDown() {
    // and remove the document from the doc group (actually, a nullptr)
    mDocGroup->RemoveDocument(nullptr);
    mDocGroup2->RemoveDocument(nullptr);
    mDocGroup = nullptr;
    mDocGroup2 = nullptr;
    ProcessAllEvents();
  }

  // this is used to get rid of transient events
  void initScheduler() { ProcessAllEvents(); }

  nsresult Dispatch(nsIRunnable* aRunnable) {
    ProcessAllEvents();
    return TimedRunnable::DispatchWithDocgroup(aRunnable, mDocGroup);
  }

  void ProcessAllEvents() { mThreadMgr->SpinEventLoopUntilEmpty(); }

  uint32_t mOther;
  bool mOldPref;
  RefPtr<MSchedulerGroup> mSchedulerGroup;
  RefPtr<DocGroup> mDocGroup;
  RefPtr<DocGroup> mDocGroup2;
  RefPtr<PerformanceCounter> mCounter;
  RefPtr<PerformanceCounter> mCounter2;
  nsCOMPtr<nsIThreadManager> mThreadMgr;
  uint32_t mDispatchCount;
};

TEST_F(ThreadMetrics, CollectMetrics) {
  nsresult rv;
  initScheduler();

  // Dispatching a runnable that will last for +50ms
  nsCOMPtr<nsIRunnable> runnable = new TimedRunnable(25, 25);
  rv = Dispatch(runnable);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Flush the queue
  ProcessAllEvents();

  // Let's look at the task category "other" counter
  ASSERT_EQ(mCounter->GetDispatchCounter()[mOther], 1u);

  // other counters should stay empty
  for (uint32_t i = 0; i < mDispatchCount; i++) {
    if (i != mOther) {
      ASSERT_EQ(mCounter->GetDispatchCounter()[i], 0u);
    }
  }

  // Did we get incremented in the docgroup ?
  uint64_t duration = mCounter->GetExecutionDuration();
  ASSERT_GE(duration, 50000u);
}

TEST_F(ThreadMetrics, CollectRecursiveMetrics) {
  nsresult rv;

  initScheduler();

  // Dispatching a runnable that will last for +50ms
  // and run another one recursively that lasts for 400ms
  RefPtr<TimedRunnable> runnable = new TimedRunnable(25, 25);
  nsCOMPtr<nsIRunnable> nested = new TimedRunnable(400, 0);
  runnable->AddNestedRunnable({nested});
  rv = Dispatch(runnable);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Flush the queue
  ProcessAllEvents();

  // let's look at the counters
  ASSERT_EQ(mCounter->GetDispatchCounter()[mOther], 1u);

  // other counters should stay empty
  for (uint32_t i = 0; i < mDispatchCount; i++) {
    if (i != mOther) {
      ASSERT_EQ(mCounter->GetDispatchCounter()[i], 0u);
    }
  }

  // did we get incremented in the docgroup ?
  uint64_t duration = mCounter->GetExecutionDuration();
  ASSERT_GE(duration, 50000u);

  // let's make sure we don't count the time spent in recursive calls
  ASSERT_LT(duration, 300000u);
}

TEST_F(ThreadMetrics, CollectMultipleRecursiveMetrics) {
  nsresult rv;

  initScheduler();

  // Dispatching a runnable that will last for +75ms
  // and run another two recursively that last for 400ms each.
  RefPtr<TimedRunnable> runnable = new TimedRunnable(25, 25);
  for (auto i : {1, 2}) {
    Unused << i;
    nsCOMPtr<nsIRunnable> nested = new TimedRunnable(400, 0);
    runnable->AddNestedRunnable({nested});
  }

  rv = Dispatch(runnable);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Flush the queue
  ProcessAllEvents();

  // let's look at the counters
  ASSERT_EQ(mCounter->GetDispatchCounter()[mOther], 1u);

  // other counters should stay empty
  for (uint32_t i = 0; i < mDispatchCount; i++) {
    if (i != mOther) {
      ASSERT_EQ(mCounter->GetDispatchCounter()[i], 0u);
    }
  }

  // did we get incremented in the docgroup ?
  uint64_t duration = mCounter->GetExecutionDuration();
  ASSERT_GE(duration, 75000u);

  // let's make sure we don't count the time spent in recursive calls
  ASSERT_LT(duration, 300000u);
}

TEST_F(ThreadMetrics, CollectMultipleRecursiveMetricsWithTwoDocgroups) {
  nsresult rv;

  initScheduler();

  // Dispatching a runnable that will last for +75ms
  // and run another two recursively that last for 400ms each.  The
  // first nested runnable will have a docgroup, but the second will
  // not, to test that the time for first nested event is accounted
  // correctly.
  RefPtr<TimedRunnable> runnable = new TimedRunnable(25, 25);
  nsCOMPtr<nsIRunnable> nested = new TimedRunnable(400, 0);
  runnable->AddNestedRunnable({nested, mDocGroup2});
  nested = new TimedRunnable(400, 0);
  runnable->AddNestedRunnable({nested});

  rv = Dispatch(runnable);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Flush the queue
  ProcessAllEvents();

  // let's look at the counters
  ASSERT_EQ(mCounter->GetDispatchCounter()[mOther], 1u);

  // other counters should stay empty
  for (uint32_t i = 0; i < mDispatchCount; i++) {
    if (i != mOther) {
      ASSERT_EQ(mCounter->GetDispatchCounter()[i], 0u);
    }
  }

  uint64_t duration = mCounter2->GetExecutionDuration();
  // Make sure this we incremented the timings for the first nested
  // runnable correctly.
  ASSERT_GE(duration, 400000u);
  ASSERT_LT(duration, 420000u);

  // And now for the outer runnable.
  duration = mCounter->GetExecutionDuration();
  ASSERT_GE(duration, 75000u);

  // let's make sure we don't count the time spent in recursive calls
  ASSERT_LT(duration, 300000u);
}
