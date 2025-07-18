/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppStartup.h"

#include "nsComponentManagerUtils.h"
#include "nsIAppShellService.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIProcess.h"
#include "nsIToolkitProfile.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowMediator.h"
#include "nsIXULRuntime.h"
#include "nsIAppWindow.h"
#include "nsNativeCharsetUtils.h"
#include "nsThreadUtils.h"
#include "nsString.h"
#include "mozilla/Preferences.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Unused.h"
#include "GeckoProfiler.h"

#include "prprf.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsWidgetsCID.h"
#include "nsAppRunner.h"
#include "nsAppShellCID.h"
#include "nsXPCOMCIDInternal.h"
#include "mozilla/Services.h"
#include "jsapi.h"
#include "js/Date.h"
#include "js/PropertyAndElement.h"  // JS_DefineProperty
#include "prenv.h"
#include "nsAppDirectoryServiceDefs.h"

#if defined(XP_WIN)
// Prevent collisions with nsAppStartup::GetStartupInfo()
#  undef GetStartupInfo

#  include <windows.h>
#elif defined(XP_DARWIN)
#  include <mach/mach_time.h>
#endif

#include "mozilla/IOInterposer.h"
#include "mozilla/StartupTimeline.h"

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#define kPrefLastSuccess "toolkit.startup.last_success"
#define kPrefMaxResumedCrashes "toolkit.startup.max_resumed_crashes"
#define kPrefRecentCrashes "toolkit.startup.recent_crashes"
#define kPrefAlwaysUseSafeMode "toolkit.startup.always_use_safe_mode"

#define kNanosecondsPerSecond 1000000000.0

using namespace mozilla;

class nsAppExitEvent : public mozilla::Runnable {
 private:
  RefPtr<nsAppStartup> mService;

 public:
  explicit nsAppExitEvent(nsAppStartup* service)
      : mozilla::Runnable("nsAppExitEvent"), mService(service) {}

  NS_IMETHOD Run() override {
    // Tell the appshell to exit
    mService->mAppShell->Exit();

    mService->mRunning = false;
    return NS_OK;
  }
};

/**
 * Computes an approximation of the absolute time represented by @a stamp
 * which is comparable to those obtained via PR_Now(). If the current absolute
 * time varies a lot (e.g. DST adjustments) since the first call then the
 * resulting times may be inconsistent.
 *
 * @param stamp The timestamp to be converted
 * @returns The converted timestamp
 */
static uint64_t ComputeAbsoluteTimestamp(TimeStamp stamp) {
  static PRTime sAbsoluteNow = PR_Now();
  static TimeStamp sMonotonicNow = TimeStamp::Now();

  return sAbsoluteNow - (sMonotonicNow - stamp).ToMicroseconds();
}

//
// nsAppStartup
//

nsAppStartup::nsAppStartup()
    : mConsiderQuitStopper(0),
      mRunning(false),
      mShuttingDown(false),
      mStartingUp(true),
      mAttemptingQuit(false),
      mRestart(false),
      mInterrupted(false),
      mIsSafeModeNecessary(false),
      mStartupCrashTrackingEnded(false) {}

nsresult nsAppStartup::Init() {
  nsresult rv;

  // Create widget application shell
  mAppShell = do_GetService(kAppShellCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) return NS_ERROR_FAILURE;

  os->AddObserver(this, "quit-application", true);
  os->AddObserver(this, "quit-application-forced", true);
  os->AddObserver(this, "sessionstore-init-started", true);
  os->AddObserver(this, "sessionstore-windows-restored", true);
  os->AddObserver(this, "profile-change-teardown", true);
  os->AddObserver(this, "xul-window-registered", true);
  os->AddObserver(this, "xul-window-destroyed", true);
  os->AddObserver(this, "profile-before-change", true);
  os->AddObserver(this, "xpcom-shutdown", true);

  return NS_OK;
}

//
// nsAppStartup->nsISupports
//

NS_IMPL_ISUPPORTS(nsAppStartup, nsIAppStartup, nsIWindowCreator, nsIObserver,
                  nsISupportsWeakReference)

//
// nsAppStartup->nsIAppStartup
//

NS_IMETHODIMP
nsAppStartup::CreateHiddenWindow() {
#if defined(MOZ_WIDGET_UIKIT)
  return NS_OK;
#else
  nsCOMPtr<nsIAppShellService> appShellService(
      do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShellService, NS_ERROR_FAILURE);

  return appShellService->CreateHiddenWindow();
#endif
}

NS_IMETHODIMP
nsAppStartup::DestroyHiddenWindow() {
#if defined(MOZ_WIDGET_UIKIT)
  return NS_OK;
#else
  nsCOMPtr<nsIAppShellService> appShellService(
      do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShellService, NS_ERROR_FAILURE);

  return appShellService->DestroyHiddenWindow();
#endif
}

NS_IMETHODIMP
nsAppStartup::Run(void) {
  NS_ASSERTION(!mRunning, "Reentrant appstartup->Run()");

  // If we have no windows open and no explicit calls to
  // enterLastWindowClosingSurvivalArea, or somebody has explicitly called
  // quit, don't bother running the event loop which would probably leave us
  // with a zombie process.

  if (!mShuttingDown && mConsiderQuitStopper != 0) {
#ifdef XP_MACOSX
    EnterLastWindowClosingSurvivalArea();
#endif

    mRunning = true;

    nsresult rv = mAppShell->Run();
    if (NS_FAILED(rv)) return rv;
  }

  // Make sure that the appropriate quit notifications have been dispatched
  // regardless of whether the event loop has spun or not. Note that this call
  // is a no-op if Quit has already been called previously.
  Quit(eForceQuit);

  nsresult retval = NS_OK;
  if (mRestart) {
    retval = NS_SUCCESS_RESTART_APP;
  }

  return retval;
}

NS_IMETHODIMP
nsAppStartup::Quit(uint32_t aMode) {
  uint32_t ferocity = (aMode & 0xF);

  // Quit the application. We will asynchronously call the appshell's
  // Exit() method via nsAppExitEvent to allow one last pass
  // through any events in the queue. This guarantees a tidy cleanup.
  nsresult rv = NS_OK;
  bool postedExitEvent = false;

  if (mShuttingDown) return NS_OK;

  // If we're considering quitting, we will only do so if:
  if (ferocity == eConsiderQuit) {
#ifdef XP_MACOSX
    nsCOMPtr<nsIAppShellService> appShell(
        do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    int32_t suspiciousCount = 1;
#endif

    if (mConsiderQuitStopper == 0) {
      // there are no windows...
      ferocity = eAttemptQuit;
    }
#ifdef XP_MACOSX
    else if (mConsiderQuitStopper == suspiciousCount) {
      // ... or there is only a hiddenWindow left, and it's useless:

      // Failure shouldn't be fatal, but will abort quit attempt:
      if (!appShell) return NS_OK;

      bool usefulHiddenWindow;
      appShell->GetApplicationProvidedHiddenWindow(&usefulHiddenWindow);
      nsCOMPtr<nsIAppWindow> hiddenWindow;
      appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
      // If the remaining windows are useful, we won't quit:
      if (!hiddenWindow || usefulHiddenWindow) {
        return NS_OK;
      }

      ferocity = eAttemptQuit;
    }
#endif
  }

  nsCOMPtr<nsIObserverService> obsService;
  if (ferocity == eAttemptQuit || ferocity == eForceQuit) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
    nsCOMPtr<nsIWindowMediator> mediator(
        do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
    if (mediator) {
      mediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
      if (windowEnumerator) {
        bool more;
        windowEnumerator->HasMoreElements(&more);
        // If we reported no windows, we definitely shouldn't be
        // iterating any here.
        MOZ_ASSERT_IF(!mConsiderQuitStopper, !more);

        while (more) {
          nsCOMPtr<nsISupports> window;
          windowEnumerator->GetNext(getter_AddRefs(window));
          nsCOMPtr<nsPIDOMWindowOuter> domWindow(do_QueryInterface(window));
          if (domWindow) {
            if (!domWindow->CanClose()) return NS_OK;
          }
          windowEnumerator->HasMoreElements(&more);
        }
      }
    }

    PROFILER_ADD_MARKER("Shutdown start", OTHER);
    mozilla::RecordShutdownStartTimeStamp();
    mShuttingDown = true;
    if (!mRestart) {
      mRestart = (aMode & eRestart) != 0;
    }

    if (mRestart) {
      // Mark the next startup as a restart.
      PR_SetEnv("MOZ_APP_RESTART=1");

      /* Firefox-restarts reuse the process so regular process start-time isn't
         a useful indicator of startup time anymore. */
      TimeStamp::RecordProcessRestart();
    }

    obsService = mozilla::services::GetObserverService();

    if (!mAttemptingQuit) {
      mAttemptingQuit = true;
#ifdef XP_MACOSX
      // now even the Mac wants to quit when the last window is closed
      ExitLastWindowClosingSurvivalArea();
#endif
      if (obsService)
        obsService->NotifyObservers(nullptr, "quit-application-granted",
                                    nullptr);
    }

    /* Enumerate through each open window and close it. It's important to do
       this before we forcequit because this can control whether we really quit
       at all. e.g. if one of these windows has an unload handler that
       opens a new window. Ugh. I know. */
    CloseAllWindows();

    if (mediator) {
      if (ferocity == eAttemptQuit) {
        ferocity = eForceQuit;  // assume success

        /* Were we able to immediately close all windows? if not, eAttemptQuit
           failed. This could happen for a variety of reasons; in fact it's
           very likely. Perhaps we're being called from JS and the window->Close
           method hasn't had a chance to wrap itself up yet. So give up.
           We'll return (with eConsiderQuit) as the remaining windows are
           closed. */
        mediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
        if (windowEnumerator) {
          bool more;
          while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) &&
                 more) {
            /* we can't quit immediately. we'll try again as the last window
               finally closes. */
            ferocity = eAttemptQuit;
            nsCOMPtr<nsISupports> window;
            windowEnumerator->GetNext(getter_AddRefs(window));
            nsCOMPtr<nsPIDOMWindowOuter> domWindow = do_QueryInterface(window);
            if (domWindow) {
              if (!domWindow->Closed()) {
                rv = NS_ERROR_FAILURE;
                break;
              }
            }
          }
        }
      }
    }
  }

  if (ferocity == eForceQuit) {
    // do it!

    // No chance of the shutdown being cancelled from here on; tell people
    // we're shutting down for sure while all services are still available.
    if (obsService) {
      obsService->NotifyObservers(nullptr, "quit-application",
                                  mRestart ? u"restart" : u"shutdown");
    }

    if (!mRunning) {
      postedExitEvent = true;
    } else {
      // no matter what, make sure we send the exit event.  If
      // worst comes to worst, we'll do a leaky shutdown but we WILL
      // shut down. Well, assuming that all *this* stuff works ;-).
      nsCOMPtr<nsIRunnable> event = new nsAppExitEvent(this);
      rv = NS_DispatchToCurrentThread(event);
      if (NS_SUCCEEDED(rv)) {
        postedExitEvent = true;
      } else {
        NS_WARNING("failed to dispatch nsAppExitEvent");
      }
    }
  }

  // turn off the reentrancy check flag, but not if we have
  // more asynchronous work to do still.
  if (!postedExitEvent) mShuttingDown = false;
  return rv;
}

void nsAppStartup::CloseAllWindows() {
  nsCOMPtr<nsIWindowMediator> mediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

  mediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));

  if (!windowEnumerator) return;

  bool more;
  while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) && more) {
    nsCOMPtr<nsISupports> isupports;
    if (NS_FAILED(windowEnumerator->GetNext(getter_AddRefs(isupports)))) break;

    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(isupports);
    NS_ASSERTION(window, "not an nsPIDOMWindow");
    if (window) {
      window->ForceClose();
    }
  }
}

NS_IMETHODIMP
nsAppStartup::EnterLastWindowClosingSurvivalArea(void) {
  ++mConsiderQuitStopper;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::ExitLastWindowClosingSurvivalArea(void) {
  NS_ASSERTION(mConsiderQuitStopper > 0, "consider quit stopper out of bounds");
  --mConsiderQuitStopper;

  if (mRunning) Quit(eConsiderQuit);

  return NS_OK;
}

//
// nsAppStartup->nsIAppStartup2
//

NS_IMETHODIMP
nsAppStartup::GetShuttingDown(bool* aResult) {
  *aResult = mShuttingDown;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetStartingUp(bool* aResult) {
  *aResult = mStartingUp;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::DoneStartingUp() {
  // This must be called once at most
  MOZ_ASSERT(mStartingUp);

  mStartingUp = false;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetRestarting(bool* aResult) {
  *aResult = mRestart;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetWasRestarted(bool* aResult) {
  char* mozAppRestart = PR_GetEnv("MOZ_APP_RESTART");

  /* When calling PR_SetEnv() with an empty value the existing variable may
   * be unset or set to the empty string depending on the underlying platform
   * thus we have to check if the variable is present and not empty. */
  *aResult = mozAppRestart && (strcmp(mozAppRestart, "") != 0);

  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetSecondsSinceLastOSRestart(int64_t *aResult) {
return NS_ERROR_NOT_IMPLEMENTED;
/*#if defined(XP_WIN)
  *aResult = int64_t(GetTickCount64() / 1000ull);
  return NS_OK;
#elif defined(XP_DARWIN)
  uint64_t absTime = mach_absolute_time();
  mach_timebase_info_data_t timebaseInfo;
  mach_timebase_info(&timebaseInfo);
  double toNanoseconds =
      double(timebaseInfo.numer) / double(timebaseInfo.denom);
  *aResult =
      std::llround(double(absTime) * toNanoseconds / kNanosecondsPerSecond);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif*/
}

NS_IMETHODIMP
nsAppStartup::SetInterrupted(bool aInterrupted) {
  mInterrupted = aInterrupted;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetInterrupted(bool* aInterrupted) {
  *aInterrupted = mInterrupted;
  return NS_OK;
}

//
// nsAppStartup->nsIWindowCreator
//

NS_IMETHODIMP
nsAppStartup::CreateChromeWindow(nsIWebBrowserChrome* aParent,
                                 uint32_t aChromeFlags,
                                 nsIRemoteTab* aOpeningTab,
                                 mozIDOMWindowProxy* aOpener,
                                 uint64_t aNextRemoteTabId, bool* aCancel,
                                 nsIWebBrowserChrome** _retval) {
  NS_ENSURE_ARG_POINTER(aCancel);
  NS_ENSURE_ARG_POINTER(_retval);
  *aCancel = false;
  *_retval = 0;

  // Non-modal windows cannot be opened if we are attempting to quit
  if (mAttemptingQuit &&
      (aChromeFlags & nsIWebBrowserChrome::CHROME_MODAL) == 0)
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;

  nsCOMPtr<nsIAppWindow> newWindow;

  if (aParent) {
    nsCOMPtr<nsIAppWindow> appParent(do_GetInterface(aParent));
    NS_ASSERTION(appParent,
                 "window created using non-app parent. that's unexpected, but "
                 "may work.");

    if (appParent)
      appParent->CreateNewWindow(aChromeFlags, aOpeningTab, aOpener,
                                 aNextRemoteTabId, getter_AddRefs(newWindow));
    // And if it fails, don't try again without a parent. It could fail
    // intentionally (bug 115969).
  } else {  // try using basic methods:
    MOZ_RELEASE_ASSERT(aNextRemoteTabId == 0,
                       "Unexpected aNextRemoteTabId, we shouldn't ever have a "
                       "next actor ID without a parent");

    /* You really shouldn't be making dependent windows without a parent.
      But unparented modal (and therefore dependent) windows happen
      in our codebase, so we allow it after some bellyaching: */
    if (aChromeFlags & nsIWebBrowserChrome::CHROME_DEPENDENT)
      NS_WARNING("dependent window created without a parent");

    nsCOMPtr<nsIAppShellService> appShell(
        do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
    if (!appShell) return NS_ERROR_FAILURE;

    appShell->CreateTopLevelWindow(
        0, 0, aChromeFlags, nsIAppShellService::SIZE_TO_CONTENT,
        nsIAppShellService::SIZE_TO_CONTENT, aOpeningTab, aOpener,
        getter_AddRefs(newWindow));
  }

  // if anybody gave us anything to work with, use it
  if (newWindow) {
    nsCOMPtr<nsIInterfaceRequestor> thing(do_QueryInterface(newWindow));
    if (thing) CallGetInterface(thing.get(), _retval);
  }

  return *_retval ? NS_OK : NS_ERROR_FAILURE;
}

//
// nsAppStartup->nsIObserver
//

NS_IMETHODIMP
nsAppStartup::Observe(nsISupports* aSubject, const char* aTopic,
                      const char16_t* aData) {
  NS_ASSERTION(mAppShell, "appshell service notified before appshell built");
  if (!strcmp(aTopic, "quit-application-forced")) {
    mShuttingDown = true;
  } else if (!strcmp(aTopic, "profile-change-teardown")) {
    if (!mShuttingDown) {
      EnterLastWindowClosingSurvivalArea();
      CloseAllWindows();
      ExitLastWindowClosingSurvivalArea();
    }
  } else if (!strcmp(aTopic, "xul-window-registered")) {
    EnterLastWindowClosingSurvivalArea();
  } else if (!strcmp(aTopic, "xul-window-destroyed")) {
    ExitLastWindowClosingSurvivalArea();
  } else if (!strcmp(aTopic, "sessionstore-windows-restored")) {
    StartupTimeline::Record(StartupTimeline::SESSION_RESTORED);
    IOInterposer::EnteringNextStage();
  } else if (!strcmp(aTopic, "sessionstore-init-started")) {
    StartupTimeline::Record(StartupTimeline::SESSION_RESTORE_INIT);
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    IOInterposer::EnteringNextStage();
  } else if (!strcmp(aTopic, "quit-application")) {
    StartupTimeline::Record(StartupTimeline::QUIT_APPLICATION);
  } else if (!strcmp(aTopic, "profile-before-change")) {
    StartupTimeline::Record(StartupTimeline::PROFILE_BEFORE_CHANGE);
  } else {
    NS_ERROR("Unexpected observer topic.");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetStartupInfo(JSContext* aCx,
                             JS::MutableHandle<JS::Value> aRetval) {
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));

  aRetval.setObject(*obj);

  TimeStamp procTime = StartupTimeline::Get(StartupTimeline::PROCESS_CREATION);

  if (procTime.IsNull()) {
    bool error = false;

    procTime = TimeStamp::ProcessCreation(&error);

    StartupTimeline::Record(StartupTimeline::PROCESS_CREATION, procTime);
  }

  for (int i = StartupTimeline::PROCESS_CREATION;
       i < StartupTimeline::MAX_EVENT_ID; ++i) {
    StartupTimeline::Event ev = static_cast<StartupTimeline::Event>(i);
    TimeStamp stamp = StartupTimeline::Get(ev);

    if (stamp.IsNull() && (ev == StartupTimeline::MAIN)) {
      // Always define main to aid with bug 689256.
      stamp = procTime;
      MOZ_ASSERT(!stamp.IsNull());
    }

    if (!stamp.IsNull()) {
      if (stamp >= procTime) {
        PRTime prStamp = ComputeAbsoluteTimestamp(stamp) / PR_USEC_PER_MSEC;
        JS::Rooted<JSObject*> date(
            aCx, JS::NewDateObject(aCx, JS::TimeClip(prStamp)));
        JS_DefineProperty(aCx, obj, StartupTimeline::Describe(ev), date,
                          JSPROP_ENUMERATE);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::GetAutomaticSafeModeNecessary(bool* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  bool alwaysSafe = false;
  Preferences::GetBool(kPrefAlwaysUseSafeMode, &alwaysSafe);

  if (!alwaysSafe) {
#if DEBUG
    mIsSafeModeNecessary = false;
#else
    mIsSafeModeNecessary &= !PR_GetEnv("MOZ_DISABLE_AUTO_SAFE_MODE");
#endif
  }

  *_retval = mIsSafeModeNecessary;
  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::TrackStartupCrashBegin(bool* aIsSafeModeNecessary) {
  const int32_t MAX_TIME_SINCE_STARTUP = 6 * 60 * 60 * 1000;
  const int32_t MAX_STARTUP_BUFFER = 10;
  nsresult rv;

  mStartupCrashTrackingEnded = false;

  StartupTimeline::Record(StartupTimeline::STARTUP_CRASH_DETECTION_BEGIN);

  bool hasLastSuccess = Preferences::HasUserValue(kPrefLastSuccess);
  if (!hasLastSuccess) {
    // Clear so we don't get stuck with SafeModeNecessary returning true if we
    // have had too many recent crashes and the last success pref is missing.
    Preferences::ClearUser(kPrefRecentCrashes);
    return NS_ERROR_NOT_AVAILABLE;
  }

  bool inSafeMode = false;
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(xr, NS_ERROR_FAILURE);

  xr->GetInSafeMode(&inSafeMode);

  PRTime replacedLockTime;
  rv = xr->GetReplacedLockTime(&replacedLockTime);

  if (NS_FAILED(rv) || !replacedLockTime) {
    if (!inSafeMode) Preferences::ClearUser(kPrefRecentCrashes);
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_OK;
  }

  // check whether safe mode is necessary
  int32_t maxResumedCrashes = -1;
  rv = Preferences::GetInt(kPrefMaxResumedCrashes, &maxResumedCrashes);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  int32_t recentCrashes = 0;
  Preferences::GetInt(kPrefRecentCrashes, &recentCrashes);
  mIsSafeModeNecessary =
      (recentCrashes > maxResumedCrashes && maxResumedCrashes != -1);

  // Bug 731613 - Don't check if the last startup was a crash if
  // XRE_PROFILE_PATH is set.  After profile manager, the profile lock's mod.
  // time has been changed so can't be used on this startup. After a restart,
  // it's safe to assume the last startup was successful.
  char* xreProfilePath = PR_GetEnv("XRE_PROFILE_PATH");
  if (xreProfilePath) {
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_ERROR_NOT_AVAILABLE;
  }

  // time of last successful startup
  int32_t lastSuccessfulStartup;
  rv = Preferences::GetInt(kPrefLastSuccess, &lastSuccessfulStartup);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t lockSeconds = (int32_t)(replacedLockTime / PR_MSEC_PER_SEC);

  // started close enough to good startup so call it good
  if (lockSeconds <= lastSuccessfulStartup + MAX_STARTUP_BUFFER &&
      lockSeconds >= lastSuccessfulStartup - MAX_STARTUP_BUFFER) {
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_OK;
  }

  // sanity check that the pref set at last success is not greater than the
  // current time
  if (PR_Now() / PR_USEC_PER_SEC <= lastSuccessfulStartup)
    return NS_ERROR_FAILURE;

  if (inSafeMode) {
    GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
    return NS_OK;
  }

  PRTime now = (PR_Now() / PR_USEC_PER_MSEC);
  // if the last startup attempt which crashed was in the last 6 hours
  if (replacedLockTime >= now - MAX_TIME_SINCE_STARTUP) {
    NS_WARNING("Last startup was detected as a crash.");
    recentCrashes++;
    rv = Preferences::SetInt(kPrefRecentCrashes, recentCrashes);
  } else {
    // Otherwise ignore that crash and all previous since it may not be
    // applicable anymore and we don't want someone to get stuck in safe mode if
    // their prefs are read-only.
    rv = Preferences::ClearUser(kPrefRecentCrashes);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // recalculate since recent crashes count may have changed above
  mIsSafeModeNecessary =
      (recentCrashes > maxResumedCrashes && maxResumedCrashes != -1);

  nsCOMPtr<nsIPrefService> prefs = Preferences::GetService();
  rv = static_cast<Preferences*>(prefs.get())
           ->SavePrefFileBlocking();  // flush prefs to disk since we are
                                      // tracking crashes
  NS_ENSURE_SUCCESS(rv, rv);

  GetAutomaticSafeModeNecessary(aIsSafeModeNecessary);
  return rv;
}

static nsresult RemoveIncompleteStartupFile() {
  nsCOMPtr<nsIFile> file;
  MOZ_TRY(NS_GetSpecialDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR,
                                 getter_AddRefs(file)));

  MOZ_TRY_VAR(file, mozilla::startup::GetIncompleteStartupFile(file));
  return file->Remove(false);
}

NS_IMETHODIMP
nsAppStartup::TrackStartupCrashEnd() {
  bool inSafeMode = false;
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (xr) xr->GetInSafeMode(&inSafeMode);

  // return if we already ended or we're restarting into safe mode
  if (mStartupCrashTrackingEnded || (mIsSafeModeNecessary && !inSafeMode))
    return NS_OK;
  mStartupCrashTrackingEnded = true;

  StartupTimeline::Record(StartupTimeline::STARTUP_CRASH_DETECTION_END);

  // Remove the incomplete startup canary file, so the next startup doesn't
  // detect a recent startup crash.
  Unused << NS_WARN_IF(NS_FAILED(RemoveIncompleteStartupFile()));

  // Use the timestamp of XRE_main as an approximation for the lock file
  // timestamp. See MAX_STARTUP_BUFFER for the buffer time period.
  TimeStamp mainTime = StartupTimeline::Get(StartupTimeline::MAIN);
  nsresult rv;

  if (mainTime.IsNull()) {
    NS_WARNING("Could not get StartupTimeline::MAIN time.");
  } else {
    uint64_t lockFileTime = ComputeAbsoluteTimestamp(mainTime);

    rv = Preferences::SetInt(kPrefLastSuccess,
                             (int32_t)(lockFileTime / PR_USEC_PER_SEC));

    if (NS_FAILED(rv))
      NS_WARNING("Could not set startup crash detection pref.");
  }

  if (inSafeMode && mIsSafeModeNecessary) {
    // On a successful startup in automatic safe mode, allow the user one more
    // crash in regular mode before returning to safe mode.
    int32_t maxResumedCrashes = 0;
    int32_t prefType;
    rv = Preferences::GetRootBranch(PrefValueKind::Default)
             ->GetPrefType(kPrefMaxResumedCrashes, &prefType);
    NS_ENSURE_SUCCESS(rv, rv);
    if (prefType == nsIPrefBranch::PREF_INT) {
      rv = Preferences::GetInt(kPrefMaxResumedCrashes, &maxResumedCrashes);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = Preferences::SetInt(kPrefRecentCrashes, maxResumedCrashes);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (!inSafeMode) {
    // clear the count of recent crashes after a succesful startup when not in
    // safe mode
    rv = Preferences::ClearUser(kPrefRecentCrashes);
    if (NS_FAILED(rv)) NS_WARNING("Could not clear startup crash count.");
  }
  nsCOMPtr<nsIPrefService> prefs = Preferences::GetService();
  // save prefs to disk since we are tracking crashes.  This may be
  // asynchronous, so a crash could sneak in that we would mistake for
  // a start up crash. See bug 789945 and bug 1361262.
  rv = prefs->SavePrefFile(nullptr);

  return rv;
}

NS_IMETHODIMP
nsAppStartup::RestartInSafeMode(uint32_t aQuitMode) {
  PR_SetEnv("MOZ_SAFE_MODE_RESTART=1");
  this->Quit(aQuitMode | nsIAppStartup::eRestart);

  return NS_OK;
}

NS_IMETHODIMP
nsAppStartup::CreateInstanceWithProfile(nsIToolkitProfile* aProfile) {
  if (NS_WARN_IF(!aProfile)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(gAbsoluteArgv0Path.IsEmpty())) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIFile> execPath;
  nsresult rv =
      NS_NewLocalFile(gAbsoluteArgv0Path, true, getter_AddRefs(execPath));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = process->Init(execPath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString profileName;
  rv = aProfile->GetName(profileName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  const char* args[] = {"-no-remote", "-P", profileName.get()};
  rv = process->Run(false, args, 3);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}
