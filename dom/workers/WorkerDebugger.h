/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerDebugger_h
#define mozilla_dom_workers_WorkerDebugger_h

#include "mozilla/PerformanceTypes.h"
#include "nsCOMPtr.h"
#include "nsIWorkerDebugger.h"

class mozIDOMWindow;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class WorkerPrivate;

class WorkerDebugger : public nsIWorkerDebugger {
  class ReportDebuggerErrorRunnable;
  class PostDebuggerMessageRunnable;

  WorkerPrivate* mWorkerPrivate;
  bool mIsInitialized;
  nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> mListeners;

 public:
  explicit WorkerDebugger(WorkerPrivate* aWorkerPrivate);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWORKERDEBUGGER

  void AssertIsOnParentThread();

  void Close();

  void PostMessageToDebugger(const nsAString& aMessage);

  void ReportErrorToDebugger(const nsAString& aFilename, uint32_t aLineno,
                             const nsAString& aMessage);

  /*
   * Sends back a PerformanceInfo struct from the counters
   * in mWorkerPrivate. Counters are reset to zero after this call.
   */
  RefPtr<PerformanceInfoPromise> ReportPerformanceInfo();

 private:
  virtual ~WorkerDebugger();

  void PostMessageToDebuggerOnMainThread(const nsAString& aMessage);

  void ReportErrorToDebuggerOnMainThread(const nsAString& aFilename,
                                         uint32_t aLineno,
                                         const nsAString& aMessage);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_WorkerDebugger_h
