/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Microsoft's API Name hackery sucks
#undef CreateEvent

#include "mozilla/BasicEvents.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/HalSensor.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/EventCallbackDebuggerNotification.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/LoadedScript.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/EventTimelineMarker.h"
#include "mozilla/TimeStamp.h"

#include "EventListenerService.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsDOMCID.h"
#include "nsError.h"
#include "nsGkAtoms.h"
#include "nsIContent.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/dom/Document.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupports.h"
#include "nsJSUtils.h"
#include "nsNameSpaceManager.h"
#include "nsPIDOMWindow.h"
#include "nsSandboxFlags.h"
#include "xpcpublic.h"
#include "nsIFrame.h"
#include "nsDisplayList.h"

namespace mozilla {

using namespace dom;
using namespace hal;

#define EVENT_TYPE_EQUALS(ls, message, userType, allEvents)                    \
  ((ls->mEventMessage == message &&                                            \
    (ls->mEventMessage != eUnidentifiedEvent || ls->mTypeAtom == userType)) || \
   (allEvents && ls->mAllEvents))

static const uint32_t kAllMutationBits =
    NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
    NS_EVENT_BITS_MUTATION_NODEINSERTED | NS_EVENT_BITS_MUTATION_NODEREMOVED |
    NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
    NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT |
    NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
    NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;

static uint32_t MutationBitForEventType(EventMessage aEventType) {
  switch (aEventType) {
    case eLegacySubtreeModified:
      return NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
    case eLegacyNodeInserted:
      return NS_EVENT_BITS_MUTATION_NODEINSERTED;
    case eLegacyNodeRemoved:
      return NS_EVENT_BITS_MUTATION_NODEREMOVED;
    case eLegacyNodeRemovedFromDocument:
      return NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    case eLegacyNodeInsertedIntoDocument:
      return NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
    case eLegacyAttrModified:
      return NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
    case eLegacyCharacterDataModified:
      return NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
    default:
      break;
  }
  return 0;
}

uint32_t EventListenerManager::sMainThreadCreatedCount = 0;

EventListenerManagerBase::EventListenerManagerBase()
    : mNoListenerForEvent(eVoidEvent),
      mMayHavePaintEventListener(false),
      mMayHaveMutationListeners(false),
      mMayHaveCapturingListeners(false),
      mMayHaveSystemGroupListeners(false),
      mMayHaveTouchEventListener(false),
      mMayHaveMouseEnterLeaveEventListener(false),
      mMayHavePointerEnterLeaveEventListener(false),
      mMayHaveKeyEventListener(false),
      mMayHaveInputOrCompositionEventListener(false),
      mMayHaveSelectionChangeEventListener(false),
      mClearingListeners(false),
      mIsMainThreadELM(NS_IsMainThread()) {
  static_assert(sizeof(EventListenerManagerBase) == sizeof(uint32_t),
                "Keep the size of EventListenerManagerBase size compact!");
}

EventListenerManager::EventListenerManager(EventTarget* aTarget)
    : EventListenerManagerBase(), mTarget(aTarget) {
  NS_ASSERTION(aTarget, "unexpected null pointer");

  if (mIsMainThreadELM) {
    ++sMainThreadCreatedCount;
  }
}

EventListenerManager::~EventListenerManager() {
  // If your code fails this assertion, a possible reason is that
  // a class did not call our Disconnect() manually. Note that
  // this class can have Disconnect called in one of two ways:
  // if it is part of a cycle, then in Unlink() (such a cycle
  // would be with one of the listeners, not mTarget which is weak).
  // If not part of a cycle, then Disconnect must be called manually,
  // typically from the destructor of the owner class (mTarget).
  // XXX azakai: Is there any reason to not just call Disconnect
  //             from right here, if not previously called?
  NS_ASSERTION(!mTarget, "didn't call Disconnect");
  RemoveAllListenersSilently();
}

void EventListenerManager::RemoveAllListenersSilently() {
  if (mClearingListeners) {
    return;
  }
  mClearingListeners = true;
  mListeners.Clear();
  mClearingListeners = false;
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(EventListenerManager, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(EventListenerManager, Release)

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    EventListenerManager::Listener& aField, const char* aName,
    unsigned aFlags) {
  if (MOZ_UNLIKELY(aCallback.WantDebugInfo())) {
    nsAutoCString name;
    name.AppendASCII(aName);
    if (aField.mTypeAtom) {
      name.AppendLiteral(" event=");
      name.Append(nsAtomCString(aField.mTypeAtom));
      name.AppendLiteral(" listenerType=");
      name.AppendInt(aField.mListenerType);
      name.AppendLiteral(" ");
    }
    CycleCollectionNoteChild(aCallback, aField.mListener.GetISupports(),
                             name.get(), aFlags);
  } else {
    CycleCollectionNoteChild(aCallback, aField.mListener.GetISupports(), aName,
                             aFlags);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(EventListenerManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EventListenerManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListeners)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(EventListenerManager)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsPIDOMWindowInner* EventListenerManager::GetInnerWindowForTarget() {
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  if (node) {
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    return node->OwnerDoc()->GetInnerWindow();
  }

  nsCOMPtr<nsPIDOMWindowInner> window = GetTargetAsInnerWindow();
  return window;
}

already_AddRefed<nsPIDOMWindowInner>
EventListenerManager::GetTargetAsInnerWindow() const {
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(mTarget);
  return window.forget();
}

void EventListenerManager::AddEventListenerInternal(
    EventListenerHolder aListenerHolder, EventMessage aEventMessage,
    nsAtom* aTypeAtom, const EventListenerFlags& aFlags, bool aHandler,
    bool aAllEvents) {
  MOZ_ASSERT((aEventMessage && aTypeAtom) || aAllEvents,  // all-events listener
             "Missing type");

  if (!aListenerHolder || mClearingListeners) {
    return;
  }

  // Since there is no public API to call us with an EventListenerHolder, we
  // know that there's an EventListenerHolder on the stack holding a strong ref
  // to the listener.

  Listener* listener;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; i++) {
    listener = &mListeners.ElementAt(i);
    // mListener == aListenerHolder is the last one, since it can be a bit slow.
    if (listener->mListenerIsHandler == aHandler &&
        listener->mFlags.EqualsForAddition(aFlags) &&
        EVENT_TYPE_EQUALS(listener, aEventMessage, aTypeAtom, aAllEvents) &&
        listener->mListener == aListenerHolder) {
      return;
    }
  }

  mNoListenerForEvent = eVoidEvent;
  mNoListenerForEventAtom = nullptr;

  listener =
      aAllEvents ? mListeners.InsertElementAt(0) : mListeners.AppendElement();
  listener->mEventMessage = aEventMessage;
  listener->mTypeAtom = aTypeAtom;
  listener->mFlags = aFlags;
  listener->mListenerIsHandler = aHandler;
  listener->mHandlerIsString = false;
  listener->mAllEvents = aAllEvents;
  listener->mIsChrome =
      mIsMainThreadELM && nsContentUtils::LegacyIsCallerChromeOrNativeCode();

  // Detect the type of event listener.
  if (aFlags.mListenerIsJSListener) {
    MOZ_ASSERT(!aListenerHolder.HasWebIDLCallback());
    listener->mListenerType = Listener::eJSEventListener;
  } else if (aListenerHolder.HasWebIDLCallback()) {
    listener->mListenerType = Listener::eWebIDLListener;
  } else {
    listener->mListenerType = Listener::eNativeListener;
  }
  listener->mListener = std::move(aListenerHolder);

  if (aFlags.mInSystemGroup) {
    mMayHaveSystemGroupListeners = true;
  }
  if (aFlags.mCapture) {
    mMayHaveCapturingListeners = true;
  }

  if (aEventMessage == eAfterPaint) {
    mMayHavePaintEventListener = true;
    if (nsPIDOMWindowInner* window = GetInnerWindowForTarget()) {
      window->SetHasPaintEventListeners();
    }
  } else if (aEventMessage >= eLegacyMutationEventFirst &&
             aEventMessage <= eLegacyMutationEventLast) {
    // For mutation listeners, we need to update the global bit on the DOM
    // window. Otherwise we won't actually fire the mutation event.
    mMayHaveMutationListeners = true;
    // Go from our target to the nearest enclosing DOM window.
    if (nsPIDOMWindowInner* window = GetInnerWindowForTarget()) {
      nsCOMPtr<Document> doc = window->GetExtantDoc();
      if (doc &&
          !(aFlags.mInSystemGroup &&
            doc->DontWarnAboutMutationEventsAndAllowSlowDOMMutations())) {
        doc->WarnOnceAbout(DeprecatedOperations::eMutationEvent);
      }
      // If aEventMessage is eLegacySubtreeModified, we need to listen all
      // mutations. nsContentUtils::HasMutationListeners relies on this.
      window->SetMutationListeners(
          (aEventMessage == eLegacySubtreeModified)
              ? kAllMutationBits
              : MutationBitForEventType(aEventMessage));
    }
  } else if (aTypeAtom == nsGkAtoms::ondeviceorientation) {
    EnableDevice(eDeviceOrientation);
  } else if (aTypeAtom == nsGkAtoms::onabsolutedeviceorientation) {
    EnableDevice(eAbsoluteDeviceOrientation);
  } else if (aTypeAtom == nsGkAtoms::ondeviceproximity ||
             aTypeAtom == nsGkAtoms::onuserproximity) {
    EnableDevice(eDeviceProximity);
  } else if (aTypeAtom == nsGkAtoms::ondevicelight) {
    EnableDevice(eDeviceLight);
  } else if (aTypeAtom == nsGkAtoms::ondevicemotion) {
    EnableDevice(eDeviceMotion);
#if defined(MOZ_WIDGET_ANDROID)
  } else if (aTypeAtom == nsGkAtoms::onorientationchange) {
    EnableDevice(eOrientationChange);
#endif
  } else if (aTypeAtom == nsGkAtoms::ontouchstart ||
             aTypeAtom == nsGkAtoms::ontouchend ||
             aTypeAtom == nsGkAtoms::ontouchmove ||
             aTypeAtom == nsGkAtoms::ontouchcancel) {
    mMayHaveTouchEventListener = true;
    nsPIDOMWindowInner* window = GetInnerWindowForTarget();
    // we don't want touchevent listeners added by scrollbars to flip this flag
    // so we ignore listeners created with system event flag
    if (window && !aFlags.mInSystemGroup) {
      window->SetHasTouchEventListeners();
    }
  } else if (aEventMessage >= ePointerEventFirst &&
             aEventMessage <= ePointerEventLast) {
    nsPIDOMWindowInner* window = GetInnerWindowForTarget();
    if (aTypeAtom == nsGkAtoms::onpointerenter ||
        aTypeAtom == nsGkAtoms::onpointerleave) {
      mMayHavePointerEnterLeaveEventListener = true;
      if (window) {
#ifdef DEBUG
        nsCOMPtr<Document> d = window->GetExtantDoc();
        NS_WARNING_ASSERTION(
            !nsContentUtils::IsChromeDoc(d),
            "Please do not use pointerenter/leave events in chrome. "
            "They are slower than pointerover/out!");
#endif
        window->SetHasPointerEnterLeaveEventListeners();
      }
    }
  } else if (aTypeAtom == nsGkAtoms::onmouseenter ||
             aTypeAtom == nsGkAtoms::onmouseleave) {
    mMayHaveMouseEnterLeaveEventListener = true;
    if (nsPIDOMWindowInner* window = GetInnerWindowForTarget()) {
#ifdef DEBUG
      nsCOMPtr<Document> d = window->GetExtantDoc();
      NS_WARNING_ASSERTION(
          !nsContentUtils::IsChromeDoc(d),
          "Please do not use mouseenter/leave events in chrome. "
          "They are slower than mouseover/out!");
#endif
      window->SetHasMouseEnterLeaveEventListeners();
    }
  } else if (aEventMessage >= eGamepadEventFirst &&
             aEventMessage <= eGamepadEventLast) {
    if (nsPIDOMWindowInner* window = GetInnerWindowForTarget()) {
      window->SetHasGamepadEventListener();
    }
  } else if (aTypeAtom == nsGkAtoms::onkeydown ||
             aTypeAtom == nsGkAtoms::onkeypress ||
             aTypeAtom == nsGkAtoms::onkeyup) {
    if (!aFlags.mInSystemGroup) {
      mMayHaveKeyEventListener = true;
    }
  } else if (aTypeAtom == nsGkAtoms::oncompositionend ||
             aTypeAtom == nsGkAtoms::oncompositionstart ||
             aTypeAtom == nsGkAtoms::oncompositionupdate ||
             aTypeAtom == nsGkAtoms::oninput) {
    if (!aFlags.mInSystemGroup) {
      mMayHaveInputOrCompositionEventListener = true;
    }
  } else if (aEventMessage == eSelectionChange) {
    mMayHaveSelectionChangeEventListener = true;
    if (nsPIDOMWindowInner* window = GetInnerWindowForTarget()) {
      window->SetHasSelectionChangeEventListeners();
    }
  } else if (aTypeAtom == nsGkAtoms::ontext) {
    // Ignore event listeners in the system group since editor needs to
    // listen "text" events in the system group.
    if (!aFlags.mInSystemGroup) {
      if (nsPIDOMWindowInner* window = GetInnerWindowForTarget()) {
        window->SetHasTextEventListenerInDefaultGroup();
      }
    }
  }

  if (IsApzAwareListener(listener)) {
    ProcessApzAwareEventListenerAdd();
  }

  if (mTarget) {
    mTarget->EventListenerAdded(aTypeAtom);
  }

  if (mIsMainThreadELM && mTarget) {
    EventListenerService::NotifyAboutMainThreadListenerChange(mTarget,
                                                              aTypeAtom);
  }
}

void EventListenerManager::ProcessApzAwareEventListenerAdd() {
  // Mark the node as having apz aware listeners
  nsCOMPtr<nsINode> node = do_QueryInterface(mTarget);
  if (node) {
    node->SetMayBeApzAware();
  }

  // Schedule a paint so event regions on the layer tree gets updated
  Document* doc = nullptr;
  if (node) {
    doc = node->OwnerDoc();
  }
  if (!doc) {
    if (nsCOMPtr<nsPIDOMWindowInner> window = GetTargetAsInnerWindow()) {
      doc = window->GetExtantDoc();
    }
  }
  if (!doc) {
    if (nsCOMPtr<DOMEventTargetHelper> helper = do_QueryInterface(mTarget)) {
      if (nsPIDOMWindowInner* window = helper->GetOwner()) {
        doc = window->GetExtantDoc();
      }
    }
  }

  if (doc && gfxPlatform::AsyncPanZoomEnabled()) {
    PresShell* presShell = doc->GetPresShell();
    if (presShell) {
      nsIFrame* f = presShell->GetRootFrame();
      if (f) {
        f->SchedulePaint();
      }
    }
  }
}

bool EventListenerManager::IsDeviceType(EventMessage aEventMessage) {
  switch (aEventMessage) {
    case eDeviceOrientation:
    case eAbsoluteDeviceOrientation:
    case eDeviceMotion:
    case eDeviceLight:
    case eDeviceProximity:
    case eUserProximity:
#if defined(MOZ_WIDGET_ANDROID)
    case eOrientationChange:
#endif
      return true;
    default:
      break;
  }
  return false;
}

void EventListenerManager::EnableDevice(EventMessage aEventMessage) {
  nsCOMPtr<nsPIDOMWindowInner> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aEventMessage) {
    case eDeviceOrientation:
#ifdef MOZ_WIDGET_ANDROID
      // Falls back to SENSOR_ROTATION_VECTOR and SENSOR_ORIENTATION if
      // unavailable on device.
      window->EnableDeviceSensor(SENSOR_GAME_ROTATION_VECTOR);
      window->EnableDeviceSensor(SENSOR_ROTATION_VECTOR);
#else
      window->EnableDeviceSensor(SENSOR_ORIENTATION);
#endif
      break;
    case eAbsoluteDeviceOrientation:
#ifdef MOZ_WIDGET_ANDROID
      // Falls back to SENSOR_ORIENTATION if unavailable on device.
      window->EnableDeviceSensor(SENSOR_ROTATION_VECTOR);
#else
      window->EnableDeviceSensor(SENSOR_ORIENTATION);
#endif
      break;
    case eDeviceProximity:
    case eUserProximity:
      window->EnableDeviceSensor(SENSOR_PROXIMITY);
      break;
    case eDeviceLight:
      window->EnableDeviceSensor(SENSOR_LIGHT);
      break;
    case eDeviceMotion:
      window->EnableDeviceSensor(SENSOR_ACCELERATION);
      window->EnableDeviceSensor(SENSOR_LINEAR_ACCELERATION);
      window->EnableDeviceSensor(SENSOR_GYROSCOPE);
      break;
#if defined(MOZ_WIDGET_ANDROID)
    case eOrientationChange:
      window->EnableOrientationChangeListener();
      break;
#endif
    default:
      NS_WARNING("Enabling an unknown device sensor.");
      break;
  }
}

void EventListenerManager::DisableDevice(EventMessage aEventMessage) {
  nsCOMPtr<nsPIDOMWindowInner> window = GetTargetAsInnerWindow();
  if (!window) {
    return;
  }

  switch (aEventMessage) {
    case eDeviceOrientation:
#ifdef MOZ_WIDGET_ANDROID
      // Disable all potential fallback sensors.
      window->DisableDeviceSensor(SENSOR_GAME_ROTATION_VECTOR);
      window->DisableDeviceSensor(SENSOR_ROTATION_VECTOR);
#endif
      window->DisableDeviceSensor(SENSOR_ORIENTATION);
      break;
    case eAbsoluteDeviceOrientation:
#ifdef MOZ_WIDGET_ANDROID
      window->DisableDeviceSensor(SENSOR_ROTATION_VECTOR);
#endif
      window->DisableDeviceSensor(SENSOR_ORIENTATION);
      break;
    case eDeviceMotion:
      window->DisableDeviceSensor(SENSOR_ACCELERATION);
      window->DisableDeviceSensor(SENSOR_LINEAR_ACCELERATION);
      window->DisableDeviceSensor(SENSOR_GYROSCOPE);
      break;
    case eDeviceProximity:
    case eUserProximity:
      window->DisableDeviceSensor(SENSOR_PROXIMITY);
      break;
    case eDeviceLight:
      window->DisableDeviceSensor(SENSOR_LIGHT);
      break;
#if defined(MOZ_WIDGET_ANDROID)
    case eOrientationChange:
      window->DisableOrientationChangeListener();
      break;
#endif
    default:
      NS_WARNING("Disabling an unknown device sensor.");
      break;
  }
}

void EventListenerManager::NotifyEventListenerRemoved(nsAtom* aUserType) {
  // If the following code is changed, other callsites of EventListenerRemoved
  // and NotifyAboutMainThreadListenerChange should be changed too.
  mNoListenerForEvent = eVoidEvent;
  mNoListenerForEventAtom = nullptr;
  if (mTarget) {
    mTarget->EventListenerRemoved(aUserType);
  }
  if (mIsMainThreadELM && mTarget) {
    EventListenerService::NotifyAboutMainThreadListenerChange(mTarget,
                                                              aUserType);
  }
}

void EventListenerManager::RemoveEventListenerInternal(
    EventListenerHolder aListenerHolder, EventMessage aEventMessage,
    nsAtom* aUserType, const EventListenerFlags& aFlags, bool aAllEvents) {
  if (!aListenerHolder || !aEventMessage || mClearingListeners) {
    return;
  }

  Listener* listener;

  uint32_t count = mListeners.Length();
  bool deviceType = IsDeviceType(aEventMessage);

  RefPtr<EventListenerManager> kungFuDeathGrip(this);

  for (uint32_t i = 0; i < count; ++i) {
    listener = &mListeners.ElementAt(i);
    if (EVENT_TYPE_EQUALS(listener, aEventMessage, aUserType, aAllEvents)) {
      if (listener->mListener == aListenerHolder &&
          listener->mFlags.EqualsForRemoval(aFlags)) {
        mListeners.RemoveElementAt(i);
        NotifyEventListenerRemoved(aUserType);
        if (!aAllEvents && deviceType) {
          DisableDevice(aEventMessage);
        }
        return;
      }
    }
  }
}

bool EventListenerManager::ListenerCanHandle(const Listener* aListener,
                                             const WidgetEvent* aEvent,
                                             EventMessage aEventMessage) const

{
  MOZ_ASSERT(aEventMessage == aEvent->mMessage ||
                 aEventMessage == GetLegacyEventMessage(aEvent->mMessage),
             "aEvent and aEventMessage should agree, modulo legacyness");

  // The listener has been removed, it cannot handle anything.
  if (aListener->mListenerType == Listener::eNoListener) {
    return false;
  }
  // This is slightly different from EVENT_TYPE_EQUALS in that it returns
  // true even when aEvent->mMessage == eUnidentifiedEvent and
  // aListener=>mEventMessage != eUnidentifiedEvent as long as the atoms are
  // the same
  if (MOZ_UNLIKELY(aListener->mAllEvents)) {
    return true;
  }
  if (aEvent->mMessage == eUnidentifiedEvent) {
    return aListener->mTypeAtom == aEvent->mSpecifiedEventType;
  }
  MOZ_ASSERT(mIsMainThreadELM);
  return aListener->mEventMessage == aEventMessage;
}

static bool IsDefaultPassiveWhenOnRoot(EventMessage aMessage) {
  if (aMessage == eTouchStart || aMessage == eTouchMove) {
    return StaticPrefs::dom_event_default_to_passive_touch_listeners();
  }
  if (aMessage == eWheel || aMessage == eLegacyMouseLineOrPageScroll ||
      aMessage == eLegacyMousePixelScroll) {
    return StaticPrefs::dom_event_default_to_passive_wheel_listeners();
  }
  return false;
}

static bool IsRootEventTaget(EventTarget* aTarget) {
  if (nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aTarget)) {
    return true;
  }
  nsCOMPtr<nsINode> node = do_QueryInterface(aTarget);
  if (!node) {
    return false;
  }
  Document* doc = node->OwnerDoc();
  return node == doc || node == doc->GetRootElement() || node == doc->GetBody();
}

void EventListenerManager::MaybeMarkPassive(EventMessage aMessage,
                                            EventListenerFlags& aFlags) {
  if (!mIsMainThreadELM) {
    return;
  }
  if (!IsDefaultPassiveWhenOnRoot(aMessage)) {
    return;
  }
  if (!IsRootEventTaget(mTarget)) {
    return;
  }
  aFlags.mPassive = true;
}

void EventListenerManager::AddEventListenerByType(
    EventListenerHolder aListenerHolder, const nsAString& aType,
    const EventListenerFlags& aFlags, const Optional<bool>& aPassive) {
  RefPtr<nsAtom> atom;
  EventMessage message =
      GetEventMessageAndAtomForListener(aType, getter_AddRefs(atom));

  EventListenerFlags flags = aFlags;
  if (aPassive.WasPassed()) {
    flags.mPassive = aPassive.Value();
  } else {
    MaybeMarkPassive(message, flags);
  }

  AddEventListenerInternal(std::move(aListenerHolder), message, atom, flags);
}

void EventListenerManager::RemoveEventListenerByType(
    EventListenerHolder aListenerHolder, const nsAString& aType,
    const EventListenerFlags& aFlags) {
  RefPtr<nsAtom> atom;
  EventMessage message =
      GetEventMessageAndAtomForListener(aType, getter_AddRefs(atom));
  RemoveEventListenerInternal(std::move(aListenerHolder), message, atom,
                              aFlags);
}

EventListenerManager::Listener* EventListenerManager::FindEventHandler(
    EventMessage aEventMessage, nsAtom* aTypeAtom) {
  // Run through the listeners for this type and see if a script
  // listener is registered
  Listener* listener;
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    listener = &mListeners.ElementAt(i);
    if (listener->mListenerIsHandler &&
        EVENT_TYPE_EQUALS(listener, aEventMessage, aTypeAtom, false)) {
      return listener;
    }
  }
  return nullptr;
}

EventListenerManager::Listener* EventListenerManager::SetEventHandlerInternal(
    nsAtom* aName, const TypedEventHandler& aTypedHandler,
    bool aPermitUntrustedEvents) {
  MOZ_ASSERT(aName);

  EventMessage eventMessage = GetEventMessage(aName);
  Listener* listener = FindEventHandler(eventMessage, aName);

  if (!listener) {
    // If we didn't find a script listener or no listeners existed
    // create and add a new one.
    EventListenerFlags flags;
    flags.mListenerIsJSListener = true;
    MaybeMarkPassive(eventMessage, flags);

    nsCOMPtr<JSEventHandler> jsEventHandler;
    NS_NewJSEventHandler(mTarget, aName, aTypedHandler,
                         getter_AddRefs(jsEventHandler));
    AddEventListenerInternal(EventListenerHolder(jsEventHandler), eventMessage,
                             aName, flags, true);

    listener = FindEventHandler(eventMessage, aName);
  } else {
    JSEventHandler* jsEventHandler = listener->GetJSEventHandler();
    MOZ_ASSERT(jsEventHandler,
               "How can we have an event handler with no JSEventHandler?");

    bool same = jsEventHandler->GetTypedEventHandler() == aTypedHandler;
    // Possibly the same listener, but update still the context and scope.
    jsEventHandler->SetHandler(aTypedHandler);
    if (mTarget && !same) {
      mTarget->EventListenerRemoved(aName);
      mTarget->EventListenerAdded(aName);
    }
    if (mIsMainThreadELM && mTarget) {
      EventListenerService::NotifyAboutMainThreadListenerChange(mTarget, aName);
    }
  }

  // Set flag to indicate possible need for compilation later
  listener->mHandlerIsString = !aTypedHandler.HasEventHandler();
  if (aPermitUntrustedEvents) {
    listener->mFlags.mAllowUntrustedEvents = true;
  }

  return listener;
}

nsresult EventListenerManager::SetEventHandler(nsAtom* aName,
                                               const nsAString& aBody,
                                               bool aDeferCompilation,
                                               bool aPermitUntrustedEvents,
                                               Element* aElement) {
  nsCOMPtr<Document> doc;
  nsCOMPtr<nsIScriptGlobalObject> global =
      GetScriptGlobalAndDocument(getter_AddRefs(doc));

  if (!global) {
    // This can happen; for example this document might have been
    // loaded as data.
    return NS_OK;
  }

  nsresult rv = NS_OK;
  // return early preventing the event listener from being added
  // 'doc' is fetched above
  if (doc) {
    // Don't allow adding an event listener if the document is sandboxed
    // without 'allow-scripts'.
    if (doc->HasScriptsBlockedBySandbox()) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    // Perform CSP check
    nsCOMPtr<nsIContentSecurityPolicy> csp = doc->GetCsp();
    unsigned lineNum = 0;
    unsigned columnNum = 0;

    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    if (cx && !JS::DescribeScriptedCaller(cx, nullptr, &lineNum, &columnNum)) {
      JS_ClearPendingException(cx);
    }

    if (csp) {
      bool allowsInlineScript = true;
      rv = csp->GetAllowsInline(
          nsIContentSecurityPolicy::SCRIPT_SRC_DIRECTIVE,
          u""_ns,  // aNonce
          true,    // aParserCreated (true because attribute event handler)
          aElement,
          nullptr,  // nsICSPEventListener
          aBody, lineNum, columnNum, &allowsInlineScript);
      NS_ENSURE_SUCCESS(rv, rv);

      // return early if CSP wants us to block inline scripts
      if (!allowsInlineScript) {
        return NS_OK;
      }
    }
  }

  // This might be the first reference to this language in the global
  // We must init the language before we attempt to fetch its context.
  if (NS_FAILED(global->EnsureScriptEnvironment())) {
    NS_WARNING("Failed to setup script environment for this language");
    // but fall through and let the inevitable failure below handle it.
  }

  nsIScriptContext* context = global->GetScriptContext();
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);
  NS_ENSURE_STATE(global->HasJSGlobal());

  Listener* listener = SetEventHandlerInternal(aName, TypedEventHandler(),
                                               aPermitUntrustedEvents);

  if (!aDeferCompilation) {
    return CompileEventHandlerInternal(listener, &aBody, aElement);
  }

  return NS_OK;
}

void EventListenerManager::RemoveEventHandler(nsAtom* aName) {
  if (mClearingListeners) {
    return;
  }

  EventMessage eventMessage = GetEventMessage(aName);
  Listener* listener = FindEventHandler(eventMessage, aName);

  if (listener) {
    mListeners.RemoveElementAt(uint32_t(listener - &mListeners.ElementAt(0)));
    NotifyEventListenerRemoved(aName);
    if (IsDeviceType(eventMessage)) {
      DisableDevice(eventMessage);
    }
  }
}

nsresult EventListenerManager::CompileEventHandlerInternal(
    Listener* aListener, const nsAString* aBody, Element* aElement) {
  MOZ_ASSERT(aListener->GetJSEventHandler());
  MOZ_ASSERT(aListener->mHandlerIsString,
             "Why are we compiling a non-string JS listener?");
  JSEventHandler* jsEventHandler = aListener->GetJSEventHandler();
  MOZ_ASSERT(!jsEventHandler->GetTypedEventHandler().HasEventHandler(),
             "What is there to compile?");

  nsresult result = NS_OK;
  nsCOMPtr<Document> doc;
  nsCOMPtr<nsIScriptGlobalObject> global =
      GetScriptGlobalAndDocument(getter_AddRefs(doc));
  NS_ENSURE_STATE(global);

  // Activate JSAPI, and make sure that exceptions are reported on the right
  // Window.
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(global))) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();

  RefPtr<nsAtom> typeAtom = aListener->mTypeAtom;
  nsAtom* attrName = typeAtom;

  // Flag us as not a string so we don't keep trying to compile strings which
  // can't be compiled.
  aListener->mHandlerIsString = false;

  // mTarget may not be an Element if it's a window and we're
  // getting an inline event listener forwarded from <html:body> or
  // <html:frameset> or <xul:window> or the like.
  // XXX I don't like that we have to reference content from
  // here. The alternative is to store the event handler string on
  // the JSEventHandler itself, and that still doesn't address
  // the arg names issue.
  nsCOMPtr<Element> element = do_QueryInterface(mTarget);
  MOZ_ASSERT(element || aBody, "Where will we get our body?");
  nsAutoString handlerBody;
  const nsAString* body = aBody;
  if (!aBody) {
    if (aListener->mTypeAtom == nsGkAtoms::onSVGLoad) {
      attrName = nsGkAtoms::onload;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGUnload) {
      attrName = nsGkAtoms::onunload;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGResize) {
      attrName = nsGkAtoms::onresize;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGScroll) {
      attrName = nsGkAtoms::onscroll;
    } else if (aListener->mTypeAtom == nsGkAtoms::onSVGZoom) {
      attrName = nsGkAtoms::onzoom;
    } else if (aListener->mTypeAtom == nsGkAtoms::onbeginEvent) {
      attrName = nsGkAtoms::onbegin;
    } else if (aListener->mTypeAtom == nsGkAtoms::onrepeatEvent) {
      attrName = nsGkAtoms::onrepeat;
    } else if (aListener->mTypeAtom == nsGkAtoms::onendEvent) {
      attrName = nsGkAtoms::onend;
    }

    element->GetAttr(kNameSpaceID_None, attrName, handlerBody);
    body = &handlerBody;
    aElement = element;
  }
  aListener = nullptr;

  nsAutoCString url("-moz-evil:lying-event-listener"_ns);
  MOZ_ASSERT(body);
  MOZ_ASSERT(aElement);
  nsIURI* uri = aElement->OwnerDoc()->GetDocumentURI();
  if (uri) {
    uri->GetSpec(url);
  }

  nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(mTarget);
  uint32_t argCount;
  const char** argNames;
  nsContentUtils::GetEventArgNames(aElement->GetNameSpaceID(), typeAtom, win,
                                   &argCount, &argNames);

  // Wrap the event target, so that we can use it as the scope for the event
  // handler. Note that mTarget is different from aElement in the <body> case,
  // where mTarget is a Window.
  //
  // The wrapScope doesn't really matter here, because the target will create
  // its reflector in the proper scope, and then we'll enter that realm.
  JS::Rooted<JSObject*> wrapScope(cx, global->GetGlobalJSObject());
  JS::Rooted<JS::Value> v(cx);
  {
    JSAutoRealm ar(cx, wrapScope);
    nsresult rv = nsContentUtils::WrapNative(cx, mTarget, &v,
                                             /* aAllowWrapping = */ false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  JS::Rooted<JSObject*> target(cx, &v.toObject());
  JSAutoRealm ar(cx, target);

  // Now that we've entered the realm we actually care about, create our
  // scope chain.  Note that we start with |element|, not aElement, because
  // mTarget is different from aElement in the <body> case, where mTarget is a
  // Window, and in that case we do not want the scope chain to include the body
  // or the document.
  JS::RootedVector<JSObject*> scopeChain(cx);
  if (!nsJSUtils::GetScopeChainForElement(cx, element, &scopeChain)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsDependentAtomString str(attrName);
  // Most of our names are short enough that we don't even have to malloc
  // the JS string stuff, so don't worry about playing games with
  // refcounting XPCOM stringbuffers.
  JS::Rooted<JSString*> jsStr(
      cx, JS_NewUCStringCopyN(cx, str.BeginReading(), str.Length()));
  NS_ENSURE_TRUE(jsStr, NS_ERROR_OUT_OF_MEMORY);

  // Get the reflector for |aElement|, so that we can pass to setElement.
  if (NS_WARN_IF(!GetOrCreateDOMReflector(cx, aElement, &v))) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<ScriptFetchOptions> fetchOptions = new ScriptFetchOptions(
      CORS_NONE, aElement->OwnerDoc()->GetReferrerPolicy(), aElement,
      aElement->OwnerDoc()->NodePrincipal(), nullptr);

  RefPtr<EventScript> eventScript = new EventScript(fetchOptions, uri);

  JS::CompileOptions options(cx);
  // Use line 0 to make the function body starts from line 1.
  options.setIntroductionType("eventHandler")
      .setFileAndLine(url.get(), 0)
      .setdeferDebugMetadata(true);

  JS::Rooted<JSObject*> handler(cx);
  result = nsJSUtils::CompileFunction(jsapi, scopeChain, options,
                                      nsAtomCString(typeAtom), argCount,
                                      argNames, *body, handler.address());
  NS_ENSURE_SUCCESS(result, result);
  NS_ENSURE_TRUE(handler, NS_ERROR_FAILURE);

  JS::Rooted<JS::Value> privateValue(cx, JS::PrivateValue(eventScript));
  result = nsJSUtils::UpdateFunctionDebugMetadata(jsapi, handler, options,
                                                  jsStr, privateValue);
  NS_ENSURE_SUCCESS(result, result);

  MOZ_ASSERT(js::IsObjectInContextCompartment(handler, cx));
  JS::Rooted<JSObject*> handlerGlobal(cx, JS::CurrentGlobalOrNull(cx));

  if (jsEventHandler->EventName() == nsGkAtoms::onerror && win) {
    RefPtr<OnErrorEventHandlerNonNull> handlerCallback =
        new OnErrorEventHandlerNonNull(static_cast<JSContext*>(nullptr),
                                       handler, handlerGlobal,
                                       /* aIncumbentGlobal = */ nullptr);
    jsEventHandler->SetHandler(handlerCallback);
  } else if (jsEventHandler->EventName() == nsGkAtoms::onbeforeunload && win) {
    RefPtr<OnBeforeUnloadEventHandlerNonNull> handlerCallback =
        new OnBeforeUnloadEventHandlerNonNull(static_cast<JSContext*>(nullptr),
                                              handler, handlerGlobal,
                                              /* aIncumbentGlobal = */ nullptr);
    jsEventHandler->SetHandler(handlerCallback);
  } else {
    RefPtr<EventHandlerNonNull> handlerCallback = new EventHandlerNonNull(
        static_cast<JSContext*>(nullptr), handler, handlerGlobal,
        /* aIncumbentGlobal = */ nullptr);
    jsEventHandler->SetHandler(handlerCallback);
  }

  return result;
}

nsresult EventListenerManager::HandleEventSubType(Listener* aListener,
                                                  Event* aDOMEvent,
                                                  EventTarget* aCurrentTarget) {
  nsresult result = NS_OK;
  // strong ref
  EventListenerHolder listenerHolder(aListener->mListener.Clone());

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if ((aListener->mListenerType == Listener::eJSEventListener) &&
      aListener->mHandlerIsString) {
    result = CompileEventHandlerInternal(aListener, nullptr, nullptr);
    aListener = nullptr;
  }

  if (NS_SUCCEEDED(result)) {
    EventCallbackDebuggerNotificationGuard dbgGuard(aCurrentTarget, aDOMEvent);
    nsAutoMicroTask mt;

    // Event::currentTarget is set in EventDispatcher.
    if (listenerHolder.HasWebIDLCallback()) {
      ErrorResult rv;
      listenerHolder.GetWebIDLCallback()->HandleEvent(aCurrentTarget,
                                                      *aDOMEvent, rv);
      result = rv.StealNSResult();
    } else {
      // listenerHolder is holding a stack ref here.
      result = MOZ_KnownLive(listenerHolder.GetXPCOMCallback())
                   ->HandleEvent(aDOMEvent);
    }
  }

  return result;
}

EventMessage EventListenerManager::GetLegacyEventMessage(
    EventMessage aEventMessage) const {
  // webkit-prefixed legacy events:
  if (aEventMessage == eTransitionEnd) {
    return eWebkitTransitionEnd;
  }
  if (aEventMessage == eAnimationStart) {
    return eWebkitAnimationStart;
  }
  if (aEventMessage == eAnimationEnd) {
    return eWebkitAnimationEnd;
  }
  if (aEventMessage == eAnimationIteration) {
    return eWebkitAnimationIteration;
  }

  switch (aEventMessage) {
    case eFullscreenChange:
      return eMozFullscreenChange;
    case eFullscreenError:
      return eMozFullscreenError;
    default:
      return aEventMessage;
  }
}

EventMessage EventListenerManager::GetEventMessage(nsAtom* aEventName) const {
  if (mIsMainThreadELM) {
    return nsContentUtils::GetEventMessage(aEventName);
  }

  // The nsContentUtils event message hashtables aren't threadsafe, so just fall
  // back to eUnidentifiedEvent.
  return eUnidentifiedEvent;
}

EventMessage EventListenerManager::GetEventMessageAndAtomForListener(
    const nsAString& aType, nsAtom** aAtom) {
  if (mIsMainThreadELM) {
    return nsContentUtils::GetEventMessageAndAtomForListener(aType, aAtom);
  }

  *aAtom = NS_Atomize(u"on"_ns + aType).take();
  return eUnidentifiedEvent;
}

already_AddRefed<nsPIDOMWindowInner> EventListenerManager::WindowFromListener(
    Listener* aListener, bool aItemInShadowTree) {
  nsCOMPtr<nsPIDOMWindowInner> innerWindow;
  if (!aItemInShadowTree) {
    if (aListener->mListener.HasWebIDLCallback()) {
      CallbackObject* callback = aListener->mListener.GetWebIDLCallback();
      nsIGlobalObject* global = nullptr;
      if (callback) {
        global = callback->IncumbentGlobalOrNull();
      }
      if (global) {
        innerWindow = global->AsInnerWindow();  // Can be nullptr
      }
    } else {
      // Can't get the global from
      // listener->mListener.GetXPCOMCallback().
      // In most cases, it would be the same as for
      // the target, so let's do that.
      innerWindow = GetInnerWindowForTarget();  // Can be nullptr
    }
  }
  return innerWindow.forget();
}

/**
 * Causes a check for event listeners and processing by them if they exist.
 * @param an event listener
 */

void EventListenerManager::HandleEventInternal(nsPresContext* aPresContext,
                                               WidgetEvent* aEvent,
                                               Event** aDOMEvent,
                                               EventTarget* aCurrentTarget,
                                               nsEventStatus* aEventStatus,
                                               bool aItemInShadowTree) {
  // Set the value of the internal PreventDefault flag properly based on
  // aEventStatus
  if (!aEvent->DefaultPrevented() &&
      *aEventStatus == nsEventStatus_eConsumeNoDefault) {
    // Assume that if only aEventStatus claims that the event has already been
    // consumed, the consumer is default event handler.
    aEvent->PreventDefault();
  }

  Maybe<AutoHandlingUserInputStatePusher> userInputStatePusher;
  Maybe<AutoPopupStatePusher> popupStatePusher;
  if (mIsMainThreadELM) {
    userInputStatePusher.emplace(UserActivation::IsUserInteractionEvent(aEvent),
                                 aEvent);
    popupStatePusher.emplace(
        PopupBlocker::GetEventPopupControlState(aEvent, *aDOMEvent));
  }

  bool hasListener = false;
  bool hasListenerForCurrentGroup = false;
  bool usingLegacyMessage = false;
  bool hasRemovedListener = false;
  EventMessage eventMessage = aEvent->mMessage;

  while (true) {
    nsAutoTObserverArray<Listener, 2>::EndLimitedIterator iter(mListeners);
    Maybe<EventMessageAutoOverride> legacyAutoOverride;
    while (iter.HasMore()) {
      if (aEvent->mFlags.mImmediatePropagationStopped) {
        break;
      }
      Listener* listener = &iter.GetNext();
      // Check that the phase is same in event and event listener.
      // Handle only trusted events, except when listener permits untrusted
      // events.
      if (ListenerCanHandle(listener, aEvent, eventMessage)) {
        hasListener = true;
        hasListenerForCurrentGroup =
            hasListenerForCurrentGroup ||
            listener->mFlags.mInSystemGroup == aEvent->mFlags.mInSystemGroup;
        if (listener->IsListening(aEvent) &&
            (aEvent->IsTrusted() || listener->mFlags.mAllowUntrustedEvents)) {
          if (!*aDOMEvent) {
            // This is tiny bit slow, but happens only once per event.
            // Similar code also in EventDispatcher.
            nsCOMPtr<EventTarget> et = aEvent->mOriginalTarget;
            RefPtr<Event> event =
                EventDispatcher::CreateEvent(et, aPresContext, aEvent, u""_ns);
            event.forget(aDOMEvent);
          }
          if (*aDOMEvent) {
            if (!aEvent->mCurrentTarget) {
              aEvent->mCurrentTarget = aCurrentTarget->GetTargetForDOMEvent();
              if (!aEvent->mCurrentTarget) {
                break;
              }
            }
            if (usingLegacyMessage && !legacyAutoOverride) {
              // Override the aDOMEvent's event-message (its .type) until we
              // finish traversing listeners (when legacyAutoOverride destructs)
              legacyAutoOverride.emplace(*aDOMEvent, eventMessage);
            }

            // Maybe add a marker to the docshell's timeline, but only
            // bother with all the logic if some docshell is recording.
            nsCOMPtr<nsIDocShell> docShell;
            RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
            bool needsEndEventMarker = false;

            if (mIsMainThreadELM &&
                listener->mListenerType != Listener::eNativeListener) {
              docShell = nsContentUtils::GetDocShellForEventTarget(mTarget);
              if (docShell) {
                if (timelines && timelines->HasConsumer(docShell)) {
                  needsEndEventMarker = true;
                  nsAutoString typeStr;
                  (*aDOMEvent)->GetType(typeStr);
                  uint16_t phase = (*aDOMEvent)->EventPhase();
                  timelines->AddMarkerForDocShell(
                      docShell, MakeUnique<EventTimelineMarker>(
                                    typeStr, phase, MarkerTracingType::START));
                }
              }
            }

            aEvent->mFlags.mInPassiveListener = listener->mFlags.mPassive;
            Maybe<Listener> listenerHolder;
            if (listener->mFlags.mOnce) {
              // Move the listener to the stack before handling the event.
              // The order is important, otherwise the listener could be
              // called again inside the listener.
              listenerHolder.emplace(std::move(*listener));
              listener = listenerHolder.ptr();
              hasRemovedListener = true;
            }

            nsCOMPtr<nsPIDOMWindowInner> innerWindow =
                WindowFromListener(listener, aItemInShadowTree);
            mozilla::dom::Event* oldWindowEvent = nullptr;
            if (innerWindow) {
              oldWindowEvent = innerWindow->SetEvent(*aDOMEvent);
            }

            nsresult rv =
                HandleEventSubType(listener, *aDOMEvent, aCurrentTarget);

            if (innerWindow) {
              Unused << innerWindow->SetEvent(oldWindowEvent);
            }

            if (NS_FAILED(rv)) {
              aEvent->mFlags.mExceptionWasRaised = true;
            }
            aEvent->mFlags.mInPassiveListener = false;

            if (needsEndEventMarker) {
              timelines->AddMarkerForDocShell(docShell, "DOMEvent",
                                              MarkerTracingType::END);
            }
          }
        }
      }
    }

    // If we didn't find any matching listeners, and our event has a legacy
    // version, we'll now switch to looking for that legacy version and we'll
    // recheck our listeners.
    if (hasListenerForCurrentGroup || usingLegacyMessage ||
        !aEvent->IsTrusted()) {
      // No need to recheck listeners, because we already found a match, we
      // already rechecked them, or it is not a trusted event.
      break;
    }
    EventMessage legacyEventMessage = GetLegacyEventMessage(eventMessage);
    if (legacyEventMessage == eventMessage) {
      break;  // There's no legacy version of our event; no need to recheck.
    }
    MOZ_ASSERT(
        GetLegacyEventMessage(legacyEventMessage) == legacyEventMessage,
        "Legacy event messages should not themselves have legacy versions");

    // Recheck our listeners, using the legacy event message we just looked up:
    eventMessage = legacyEventMessage;
    usingLegacyMessage = true;
  }

  aEvent->mCurrentTarget = nullptr;

  if (hasRemovedListener) {
    // If there are any once listeners replaced with a placeholder in
    // the loop above, we need to clean up them here. Note that, this
    // could clear once listeners handled in some outer level as well,
    // but that should not affect the result.
    mListeners.RemoveElementsBy([](const Listener& aListener) {
      return aListener.mListenerType == Listener::eNoListener;
    });
    NotifyEventListenerRemoved(aEvent->mSpecifiedEventType);
    if (IsDeviceType(aEvent->mMessage)) {
      // This is a device-type event, we need to check whether we can
      // disable device after removing the once listeners.
      bool hasAnyListener = false;
      nsAutoTObserverArray<Listener, 2>::ForwardIterator iter(mListeners);
      while (iter.HasMore()) {
        Listener* listener = &iter.GetNext();
        if (EVENT_TYPE_EQUALS(listener, aEvent->mMessage,
                              aEvent->mSpecifiedEventType,
                              /* all events */ false)) {
          hasAnyListener = true;
          break;
        }
      }
      if (!hasAnyListener) {
        DisableDevice(aEvent->mMessage);
      }
    }
  }

  if (mIsMainThreadELM && !hasListener) {
    mNoListenerForEvent = aEvent->mMessage;
    mNoListenerForEventAtom = aEvent->mSpecifiedEventType;
  }

  if (aEvent->DefaultPrevented()) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }
}

void EventListenerManager::Disconnect() {
  mTarget = nullptr;
  RemoveAllListenersSilently();
}

void EventListenerManager::AddEventListener(const nsAString& aType,
                                            EventListenerHolder aListenerHolder,
                                            bool aUseCapture,
                                            bool aWantsUntrusted) {
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  return AddEventListenerByType(std::move(aListenerHolder), aType, flags);
}

void EventListenerManager::AddEventListener(
    const nsAString& aType, EventListenerHolder aListenerHolder,
    const dom::AddEventListenerOptionsOrBoolean& aOptions,
    bool aWantsUntrusted) {
  EventListenerFlags flags;
  Optional<bool> passive;
  if (aOptions.IsBoolean()) {
    flags.mCapture = aOptions.GetAsBoolean();
  } else {
    const auto& options = aOptions.GetAsAddEventListenerOptions();
    flags.mCapture = options.mCapture;
    flags.mInSystemGroup = options.mMozSystemGroup;
    flags.mOnce = options.mOnce;
    if (options.mPassive.WasPassed()) {
      passive.Construct(options.mPassive.Value());
    }
  }
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  return AddEventListenerByType(std::move(aListenerHolder), aType, flags,
                                passive);
}

void EventListenerManager::RemoveEventListener(
    const nsAString& aType, EventListenerHolder aListenerHolder,
    bool aUseCapture) {
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  RemoveEventListenerByType(std::move(aListenerHolder), aType, flags);
}

void EventListenerManager::RemoveEventListener(
    const nsAString& aType, EventListenerHolder aListenerHolder,
    const dom::EventListenerOptionsOrBoolean& aOptions) {
  EventListenerFlags flags;
  if (aOptions.IsBoolean()) {
    flags.mCapture = aOptions.GetAsBoolean();
  } else {
    const auto& options = aOptions.GetAsEventListenerOptions();
    flags.mCapture = options.mCapture;
    flags.mInSystemGroup = options.mMozSystemGroup;
  }
  RemoveEventListenerByType(std::move(aListenerHolder), aType, flags);
}

void EventListenerManager::AddListenerForAllEvents(EventListener* aDOMListener,
                                                   bool aUseCapture,
                                                   bool aWantsUntrusted,
                                                   bool aSystemEventGroup) {
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = aWantsUntrusted;
  flags.mInSystemGroup = aSystemEventGroup;
  AddEventListenerInternal(EventListenerHolder(aDOMListener), eAllEvents,
                           nullptr, flags, false, true);
}

void EventListenerManager::RemoveListenerForAllEvents(
    EventListener* aDOMListener, bool aUseCapture, bool aSystemEventGroup) {
  EventListenerFlags flags;
  flags.mCapture = aUseCapture;
  flags.mInSystemGroup = aSystemEventGroup;
  RemoveEventListenerInternal(EventListenerHolder(aDOMListener), eAllEvents,
                              nullptr, flags, true);
}

bool EventListenerManager::HasMutationListeners() {
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      Listener* listener = &mListeners.ElementAt(i);
      if (listener->mEventMessage >= eLegacyMutationEventFirst &&
          listener->mEventMessage <= eLegacyMutationEventLast) {
        return true;
      }
    }
  }

  return false;
}

uint32_t EventListenerManager::MutationListenerBits() {
  uint32_t bits = 0;
  if (mMayHaveMutationListeners) {
    uint32_t count = mListeners.Length();
    for (uint32_t i = 0; i < count; ++i) {
      Listener* listener = &mListeners.ElementAt(i);
      if (listener->mEventMessage >= eLegacyMutationEventFirst &&
          listener->mEventMessage <= eLegacyMutationEventLast) {
        if (listener->mEventMessage == eLegacySubtreeModified) {
          return kAllMutationBits;
        }
        bits |= MutationBitForEventType(listener->mEventMessage);
      }
    }
  }
  return bits;
}

bool EventListenerManager::HasListenersFor(const nsAString& aEventName) const {
  RefPtr<nsAtom> atom = NS_Atomize(u"on"_ns + aEventName);
  return HasListenersFor(atom);
}

bool EventListenerManager::HasListenersFor(nsAtom* aEventNameWithOn) const {
#ifdef DEBUG
  nsAutoString name;
  aEventNameWithOn->ToString(name);
#endif
  NS_ASSERTION(StringBeginsWith(name, u"on"_ns),
               "Event name does not start with 'on'");
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const Listener* listener = &mListeners.ElementAt(i);
    if (listener->mTypeAtom == aEventNameWithOn) {
      return true;
    }
  }
  return false;
}

bool EventListenerManager::HasListeners() const {
  return !mListeners.IsEmpty();
}

nsresult EventListenerManager::GetListenerInfo(
    nsTArray<RefPtr<nsIEventListenerInfo>>& aList) {
  nsCOMPtr<EventTarget> target = mTarget;
  NS_ENSURE_STATE(target);
  aList.Clear();
  nsAutoTObserverArray<Listener, 2>::ForwardIterator iter(mListeners);
  while (iter.HasMore()) {
    const Listener& listener = iter.GetNext();
    // If this is a script handler and we haven't yet
    // compiled the event handler itself go ahead and compile it
    if (listener.mListenerType == Listener::eJSEventListener &&
        listener.mHandlerIsString) {
      CompileEventHandlerInternal(const_cast<Listener*>(&listener), nullptr,
                                  nullptr);
    }
    nsAutoString eventType;
    if (listener.mAllEvents) {
      eventType.SetIsVoid(true);
    } else if (listener.mListenerType == Listener::eNoListener) {
      continue;
    } else {
      eventType.Assign(Substring(nsDependentAtomString(listener.mTypeAtom), 2));
    }

    JS::Rooted<JSObject*> callback(RootingCx());
    JS::Rooted<JSObject*> callbackGlobal(RootingCx());
    if (JSEventHandler* handler = listener.GetJSEventHandler()) {
      if (handler->GetTypedEventHandler().HasEventHandler()) {
        CallbackFunction* callbackFun = handler->GetTypedEventHandler().Ptr();
        callback = callbackFun->CallableOrNull();
        callbackGlobal = callbackFun->CallbackGlobalOrNull();
        if (!callback) {
          // This will be null for cross-compartment event listeners
          // which have been destroyed.
          continue;
        }
      }
    } else if (listener.mListenerType == Listener::eWebIDLListener) {
      EventListener* listenerCallback = listener.mListener.GetWebIDLCallback();
      callback = listenerCallback->CallbackOrNull();
      callbackGlobal = listenerCallback->CallbackGlobalOrNull();
      if (!callback) {
        // This will be null for cross-compartment event listeners
        // which have been destroyed.
        continue;
      }
    }

    RefPtr<EventListenerInfo> info = new EventListenerInfo(
        eventType, callback, callbackGlobal, listener.mFlags.mCapture,
        listener.mFlags.mAllowUntrustedEvents, listener.mFlags.mInSystemGroup);
    aList.AppendElement(info.forget());
  }
  return NS_OK;
}

bool EventListenerManager::HasUnloadListeners() {
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (listener->mEventMessage == eUnload ||
        listener->mEventMessage == eBeforeUnload) {
      return true;
    }
  }
  return false;
}

void EventListenerManager::SetEventHandler(nsAtom* aEventName,
                                           EventHandlerNonNull* aHandler) {
  if (!aHandler) {
    RemoveEventHandler(aEventName);
    return;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  SetEventHandlerInternal(
      aEventName, TypedEventHandler(aHandler),
      !mIsMainThreadELM || !nsContentUtils::IsCallerChrome());
}

void EventListenerManager::SetEventHandler(
    OnErrorEventHandlerNonNull* aHandler) {
  if (!aHandler) {
    RemoveEventHandler(nsGkAtoms::onerror);
    return;
  }

  // Untrusted events are always permitted on workers and for non-chrome script
  // on the main thread.
  bool allowUntrusted = !mIsMainThreadELM || !nsContentUtils::IsCallerChrome();

  SetEventHandlerInternal(nsGkAtoms::onerror, TypedEventHandler(aHandler),
                          allowUntrusted);
}

void EventListenerManager::SetEventHandler(
    OnBeforeUnloadEventHandlerNonNull* aHandler) {
  if (!aHandler) {
    RemoveEventHandler(nsGkAtoms::onbeforeunload);
    return;
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  SetEventHandlerInternal(
      nsGkAtoms::onbeforeunload, TypedEventHandler(aHandler),
      !mIsMainThreadELM || !nsContentUtils::IsCallerChrome());
}

const TypedEventHandler* EventListenerManager::GetTypedEventHandler(
    nsAtom* aEventName) {
  EventMessage eventMessage = GetEventMessage(aEventName);
  Listener* listener = FindEventHandler(eventMessage, aEventName);

  if (!listener) {
    return nullptr;
  }

  JSEventHandler* jsEventHandler = listener->GetJSEventHandler();

  if (listener->mHandlerIsString) {
    CompileEventHandlerInternal(listener, nullptr, nullptr);
  }

  const TypedEventHandler& typedHandler =
      jsEventHandler->GetTypedEventHandler();
  return typedHandler.HasEventHandler() ? &typedHandler : nullptr;
}

size_t EventListenerManager::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);
  n += mListeners.ShallowSizeOfExcludingThis(aMallocSizeOf);
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    JSEventHandler* jsEventHandler =
        mListeners.ElementAt(i).GetJSEventHandler();
    if (jsEventHandler) {
      n += jsEventHandler->SizeOfIncludingThis(aMallocSizeOf);
    }
  }
  return n;
}

void EventListenerManager::MarkForCC() {
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const Listener& listener = mListeners.ElementAt(i);
    JSEventHandler* jsEventHandler = listener.GetJSEventHandler();
    if (jsEventHandler) {
      const TypedEventHandler& typedHandler =
          jsEventHandler->GetTypedEventHandler();
      if (typedHandler.HasEventHandler()) {
        typedHandler.Ptr()->MarkForCC();
      }
    } else if (listener.mListenerType == Listener::eWebIDLListener) {
      listener.mListener.GetWebIDLCallback()->MarkForCC();
    }
  }
  if (mRefCnt.IsPurple()) {
    mRefCnt.RemovePurple();
  }
}

void EventListenerManager::TraceListeners(JSTracer* aTrc) {
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    const Listener& listener = mListeners.ElementAt(i);
    JSEventHandler* jsEventHandler = listener.GetJSEventHandler();
    if (jsEventHandler) {
      const TypedEventHandler& typedHandler =
          jsEventHandler->GetTypedEventHandler();
      if (typedHandler.HasEventHandler()) {
        mozilla::TraceScriptHolder(typedHandler.Ptr(), aTrc);
      }
    } else if (listener.mListenerType == Listener::eWebIDLListener) {
      mozilla::TraceScriptHolder(listener.mListener.GetWebIDLCallback(), aTrc);
    }
    // We might have eWrappedJSListener, but that is the legacy type for
    // JS implemented event listeners, and trickier to handle here.
  }
}

bool EventListenerManager::HasNonSystemGroupListenersForUntrustedKeyEvents() {
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (!listener->mFlags.mInSystemGroup &&
        listener->mFlags.mAllowUntrustedEvents &&
        (listener->mTypeAtom == nsGkAtoms::onkeydown ||
         listener->mTypeAtom == nsGkAtoms::onkeypress ||
         listener->mTypeAtom == nsGkAtoms::onkeyup)) {
      return true;
    }
  }
  return false;
}

bool EventListenerManager::
    HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents() {
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (!listener->mFlags.mPassive && !listener->mFlags.mInSystemGroup &&
        listener->mFlags.mAllowUntrustedEvents &&
        (listener->mTypeAtom == nsGkAtoms::onkeydown ||
         listener->mTypeAtom == nsGkAtoms::onkeypress ||
         listener->mTypeAtom == nsGkAtoms::onkeyup)) {
      return true;
    }
  }
  return false;
}

bool EventListenerManager::HasApzAwareListeners() {
  uint32_t count = mListeners.Length();
  for (uint32_t i = 0; i < count; ++i) {
    Listener* listener = &mListeners.ElementAt(i);
    if (IsApzAwareListener(listener)) {
      return true;
    }
  }
  return false;
}

bool EventListenerManager::IsApzAwareListener(Listener* aListener) {
  return !aListener->mFlags.mPassive && IsApzAwareEvent(aListener->mTypeAtom);
}

bool EventListenerManager::IsApzAwareEvent(nsAtom* aEvent) {
  if (aEvent == nsGkAtoms::onwheel || aEvent == nsGkAtoms::onDOMMouseScroll ||
      aEvent == nsGkAtoms::onmousewheel ||
      aEvent == nsGkAtoms::onMozMousePixelScroll) {
    return true;
  }
  // In theory we should schedule a repaint if the touch event pref changes,
  // because the event regions might be out of date. In practice that seems like
  // overkill because users generally shouldn't be flipping this pref, much
  // less expecting touch listeners on the page to immediately start preventing
  // scrolling without so much as a repaint. Tests that we write can work
  // around this constraint easily enough.
  if (aEvent == nsGkAtoms::ontouchstart || aEvent == nsGkAtoms::ontouchmove) {
    return TouchEvent::PrefEnabled(
        nsContentUtils::GetDocShellForEventTarget(mTarget));
  }
  return false;
}

void EventListenerManager::RemoveAllListeners() {
  while (!mListeners.IsEmpty()) {
    size_t idx = mListeners.Length() - 1;
    RefPtr<nsAtom> type = mListeners.ElementAt(idx).mTypeAtom;
    EventMessage message = mListeners.ElementAt(idx).mEventMessage;
    mListeners.RemoveElementAt(idx);
    NotifyEventListenerRemoved(type);
    if (IsDeviceType(message)) {
      DisableDevice(message);
    }
  }
}

already_AddRefed<nsIScriptGlobalObject>
EventListenerManager::GetScriptGlobalAndDocument(Document** aDoc) {
  nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));
  nsCOMPtr<Document> doc;
  nsCOMPtr<nsIScriptGlobalObject> global;
  if (node) {
    // Try to get context from doc
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    doc = node->OwnerDoc();
    if (doc->IsLoadedAsData()) {
      return nullptr;
    }

    // We want to allow compiling an event handler even in an unloaded
    // document, so use GetScopeObject here, not GetScriptHandlingObject.
    global = do_QueryInterface(doc->GetScopeObject());
  } else {
    if (nsCOMPtr<nsPIDOMWindowInner> win = GetTargetAsInnerWindow()) {
      doc = win->GetExtantDoc();
      global = do_QueryInterface(win);
    } else {
      global = do_QueryInterface(mTarget);
    }
  }

  doc.forget(aDoc);
  return global.forget();
}

}  // namespace mozilla
