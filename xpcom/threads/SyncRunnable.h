/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SyncRunnable_h
#define mozilla_SyncRunnable_h

#include <utility>

#include "mozilla/AbstractThread.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/Monitor2.h"
#include "nsThreadUtils.h"

namespace mozilla {

/**
 * This class will wrap a nsIRunnable and dispatch it to the main thread
 * synchronously. This is different from nsIEventTarget.DISPATCH_SYNC:
 * this class does not spin the event loop waiting for the event to be
 * dispatched. This means that you don't risk reentrance from pending
 * messages, but you must be sure that the target thread does not ever block
 * on this thread, or else you will deadlock.
 *
 * Typical usage:
 * RefPtr<SyncRunnable> sr = new SyncRunnable(new myrunnable...());
 * sr->DispatchToThread(t);
 *
 * We also provide a convenience wrapper:
 * SyncRunnable::DispatchToThread(new myrunnable...());
 *
 */
class SyncRunnable : public Runnable {
 public:
  explicit SyncRunnable(nsIRunnable* aRunnable)
      : Runnable("SyncRunnable"),
        mRunnable(aRunnable),
        mMonitor("SyncRunnable"),
        mDone(false) {}

  explicit SyncRunnable(already_AddRefed<nsIRunnable> aRunnable)
      : Runnable("SyncRunnable"),
        mRunnable(std::move(aRunnable)),
        mMonitor("SyncRunnable"),
        mDone(false) {}

  void DispatchToThread(nsIEventTarget* aThread, bool aForceDispatch = false) {
    nsresult rv;
    bool on;

    if (!aForceDispatch) {
      rv = aThread->IsOnCurrentThread(&on);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv) && on) {
        mRunnable->Run();
        return;
      }
    }

    rv = aThread->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_SUCCEEDED(rv)) {
      mozilla::Monitor2AutoLock lock(mMonitor);
      // This could be synchronously dispatching to a thread currently waiting
      // for JS execution clearance. Yield JS execution.
      dom::AutoYieldJSThreadExecution yield;

      while (!mDone) {
        lock.Wait();
      }
    }
  }

  void DispatchToThread(AbstractThread* aThread, bool aForceDispatch = false) {
    if (!aForceDispatch && aThread->IsCurrentThreadIn()) {
      mRunnable->Run();
      return;
    }

    // Check we don't have tail dispatching here. Otherwise we will deadlock
    // ourself when spinning the loop below.
    MOZ_ASSERT(!aThread->RequiresTailDispatchFromCurrentThread());

    aThread->Dispatch(RefPtr<nsIRunnable>(this).forget());
    mozilla::Monitor2AutoLock lock(mMonitor);
    while (!mDone) {
      lock.Wait();
    }
  }

  static void DispatchToThread(nsIEventTarget* aThread, nsIRunnable* aRunnable,
                               bool aForceDispatch = false) {
    RefPtr<SyncRunnable> s(new SyncRunnable(aRunnable));
    s->DispatchToThread(aThread, aForceDispatch);
  }

  static void DispatchToThread(AbstractThread* aThread, nsIRunnable* aRunnable,
                               bool aForceDispatch = false) {
    RefPtr<SyncRunnable> s(new SyncRunnable(aRunnable));
    s->DispatchToThread(aThread, aForceDispatch);
  }

 protected:
  NS_IMETHOD Run() override {
    mRunnable->Run();

    mozilla::Monitor2AutoLock lock(mMonitor);
    MOZ_ASSERT(!mDone);

    mDone = true;
    mMonitor.Signal();

    return NS_OK;
  }

 private:
  nsCOMPtr<nsIRunnable> mRunnable;
  mozilla::Monitor2 mMonitor;
  bool mDone;
};

}  // namespace mozilla

#endif  // mozilla_SyncRunnable_h
