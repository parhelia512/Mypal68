/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PostMessageEvent.h"

#include "MessageEvent.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/EventDispatcher.h"
#include "nsDocShell.h"
#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"
#include "nsIConsoleService.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsPresContext.h"
#include "nsQueryObject.h"

namespace mozilla::dom {

PostMessageEvent::PostMessageEvent(BrowsingContext* aSource,
                                   const nsAString& aCallerOrigin,
                                   nsGlobalWindowOuter* aTargetWindow,
                                   nsIPrincipal* aProvidedPrincipal,
                                   const Maybe<uint64_t>& aCallerWindowID,
                                   nsIURI* aCallerDocumentURI,
                                   bool aIsFromPrivateWindow)
    : Runnable("dom::PostMessageEvent"),
      mSource(aSource),
      mCallerOrigin(aCallerOrigin),
      mTargetWindow(aTargetWindow),
      mProvidedPrincipal(aProvidedPrincipal),
      mCallerWindowID(aCallerWindowID),
      mCallerDocumentURI(aCallerDocumentURI),
      mIsFromPrivateWindow(aIsFromPrivateWindow) {}

PostMessageEvent::~PostMessageEvent() = default;

NS_IMETHODIMP
PostMessageEvent::Run() {
  // Note: We don't init this AutoJSAPI with targetWindow, because we do not
  // want exceptions during message deserialization to trigger error events on
  // targetWindow.
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();

  // The document URI is just used for the principal mismatch error message
  // below. Use a stack variable so mCallerDocumentURI is not held onto after
  // this method finishes, regardless of the method outcome.
  nsCOMPtr<nsIURI> callerDocumentURI = mCallerDocumentURI.forget();

  // If we bailed before this point we're going to leak mMessage, but
  // that's probably better than crashing.

  RefPtr<nsGlobalWindowInner> targetWindow;
  if (mTargetWindow->IsClosedOrClosing() ||
      !(targetWindow = mTargetWindow->GetCurrentInnerWindowInternal()) ||
      targetWindow->IsDying())
    return NS_OK;

  // If the window's document has suppressed event handling, hand off this event
  // for running later. We check the top window's document so that when multiple
  // same-origin windows exist in the same top window, postMessage events will
  // be delivered in the same order they were posted, regardless of which window
  // they were posted to.
  if (nsCOMPtr<nsPIDOMWindowOuter> topWindow =
          targetWindow->GetOuterWindow()->GetInProcessTop()) {
    if (nsCOMPtr<nsPIDOMWindowInner> topInner =
            topWindow->GetCurrentInnerWindow()) {
      if (topInner->GetExtantDoc() &&
          topInner->GetExtantDoc()->SuspendPostMessageEvent(this)) {
        return NS_OK;
      }
    }
  }

  JSAutoRealm ar(cx, targetWindow->GetWrapper());

  // Ensure that any origin which might have been provided is the origin of this
  // window's document.  Note that we do this *now* instead of when postMessage
  // is called because the target window might have been navigated to a
  // different location between then and now.  If this check happened when
  // postMessage was called, it would be fairly easy for a malicious webpage to
  // intercept messages intended for another site by carefully timing navigation
  // of the target window so it changed location after postMessage but before
  // now.
  if (mProvidedPrincipal) {
    // Get the target's origin either from its principal or, in the case the
    // principal doesn't carry a URI (e.g. the system principal), the target's
    // document.
    nsIPrincipal* targetPrin = targetWindow->GetPrincipal();
    if (NS_WARN_IF(!targetPrin)) return NS_OK;

    // Note: This is contrary to the spec with respect to file: URLs, which
    //       the spec groups into a single origin, but given we intentionally
    //       don't do that in other places it seems better to hold the line for
    //       now.  Long-term, we want HTML5 to address this so that we can
    //       be compliant while being safer.
    if (!targetPrin->Equals(mProvidedPrincipal)) {
      OriginAttributes sourceAttrs = mProvidedPrincipal->OriginAttributesRef();
      OriginAttributes targetAttrs = targetPrin->OriginAttributesRef();

      MOZ_DIAGNOSTIC_ASSERT(
          sourceAttrs.mUserContextId == targetAttrs.mUserContextId,
          "Target and source should have the same userContextId attribute.");

      nsAutoString providedOrigin, targetOrigin;
      nsresult rv = nsContentUtils::GetUTFOrigin(targetPrin, targetOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = nsContentUtils::GetUTFOrigin(mProvidedPrincipal, providedOrigin);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString errorText;
      nsContentUtils::FormatLocalizedString(
          errorText, nsContentUtils::eDOM_PROPERTIES,
          "TargetPrincipalDoesNotMatch", providedOrigin, targetOrigin);

      nsCOMPtr<nsIScriptError> errorObject =
          do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      if (mCallerWindowID.isSome()) {
        rv = errorObject->InitWithSourceURI(
            errorText, callerDocumentURI, u""_ns, 0, 0, nsIScriptError::errorFlag,
            "DOM Window", mCallerWindowID.value());
      } else {
        nsString uriSpec;
        rv = NS_GetSanitizedURIStringFromURI(callerDocumentURI, uriSpec);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = errorObject->Init(errorText, uriSpec, u""_ns, 0, 0,
                               nsIScriptError::errorFlag, "DOM Window",
                               mIsFromPrivateWindow,
                               mProvidedPrincipal->IsSystemPrincipal());
      }
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIConsoleService> consoleService =
          do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      return consoleService->LogMessage(errorObject);
    }
  }

  IgnoredErrorResult rv;
  JS::Rooted<JS::Value> messageData(cx);
  nsCOMPtr<mozilla::dom::EventTarget> eventTarget =
      do_QueryObject(targetWindow);

  StructuredCloneHolder* holder;
  if (mHolder.constructed<StructuredCloneHolder>()) {
    mHolder.ref<StructuredCloneHolder>().Read(targetWindow->AsGlobal(), cx,
                                              &messageData, rv);
    holder = &mHolder.ref<StructuredCloneHolder>();
  } else {
    MOZ_ASSERT(mHolder.constructed<ipc::StructuredCloneData>());
    mHolder.ref<ipc::StructuredCloneData>().Read(cx, &messageData, rv);
    holder = &mHolder.ref<ipc::StructuredCloneData>();
  }
  if (NS_WARN_IF(rv.Failed())) {
    DispatchError(cx, targetWindow, eventTarget);
    return NS_OK;
  }

  // Create the event
  RefPtr<MessageEvent> event = new MessageEvent(eventTarget, nullptr, nullptr);

  Nullable<WindowProxyOrMessagePortOrServiceWorker> source;
  if (mSource) {
    source.SetValue().SetAsWindowProxy() = mSource;
  }

  Sequence<OwningNonNull<MessagePort>> ports;
  if (!holder->TakeTransferredPortsAsSequence(ports)) {
    DispatchError(cx, targetWindow, eventTarget);
    return NS_OK;
  }

  event->InitMessageEvent(nullptr, u"message"_ns, CanBubble::eNo,
                          Cancelable::eNo, messageData, mCallerOrigin, u""_ns,
                          source, ports);

  Dispatch(targetWindow, event);
  return NS_OK;
}

void PostMessageEvent::DispatchError(JSContext* aCx,
                                     nsGlobalWindowInner* aTargetWindow,
                                     mozilla::dom::EventTarget* aEventTarget) {
  RootedDictionary<MessageEventInit> init(aCx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mOrigin = mCallerOrigin;

  if (mSource) {
    init.mSource.SetValue().SetAsWindowProxy() = mSource;
  }

  RefPtr<Event> event =
      MessageEvent::Constructor(aEventTarget, u"messageerror"_ns, init);
  Dispatch(aTargetWindow, event);
}

void PostMessageEvent::Dispatch(nsGlobalWindowInner* aTargetWindow,
                                Event* aEvent) {
  // We can't simply call dispatchEvent on the window because doing so ends
  // up flipping the trusted bit on the event, and we don't want that to
  // happen because then untrusted content can call postMessage on a chrome
  // window if it can get a reference to it.

  RefPtr<nsPresContext> presContext =
      aTargetWindow->GetExtantDoc()->GetPresContext();

  aEvent->SetTrusted(true);
  WidgetEvent* internalEvent = aEvent->WidgetEventPtr();

  nsEventStatus status = nsEventStatus_eIgnore;
  EventDispatcher::Dispatch(ToSupports(aTargetWindow), presContext,
                            internalEvent, aEvent, &status);
}

}  // namespace mozilla::dom
