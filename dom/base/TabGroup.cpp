/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabGroup.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h" //MY
#include "mozilla/dom/TimeoutManager.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsIDocShell.h"

namespace mozilla {
namespace dom {

static StaticRefPtr<TabGroup> sChromeTabGroup;

LinkedList<TabGroup>* TabGroup::sTabGroups = nullptr;

TabGroup::TabGroup(bool aIsChrome)
    : mLastWindowLeft(false),
      mThrottledQueuesInitialized(false),
      mNumOfIndexedDBTransactions(0),
      mNumOfIndexedDBDatabases(0),
      mIsChrome(aIsChrome),
      mForegroundCount(0) {
  if (!sTabGroups) {
    sTabGroups = new LinkedList<TabGroup>();
  }
  sTabGroups->insertBack(this);

  CreateEventTargets(/* aNeedValidation = */ !aIsChrome);

  // Do not throttle runnables from chrome windows.  In theory we should
  // not have abuse issues from these windows and many browser chrome
  // tests have races that fail if we do throttle chrome runnables.
  if (aIsChrome) {
    MOZ_ASSERT(!sChromeTabGroup);
    return;
  }

  // This constructor can be called from the IPC I/O thread. In that case, we
  // won't actually use the TabGroup on the main thread until GetFromWindowActor
  // is called, so we initialize the throttled queues there.
  if (NS_IsMainThread()) {
    EnsureThrottledEventQueues();
  }
}

TabGroup::~TabGroup() {
  MOZ_ASSERT(mDocGroups.IsEmpty());
  MOZ_ASSERT(mWindows.IsEmpty());
  MOZ_RELEASE_ASSERT(mLastWindowLeft || mIsChrome);

  LinkedListElement<TabGroup>* listElement =
      static_cast<LinkedListElement<TabGroup>*>(this);
  listElement->remove();

  if (sTabGroups->isEmpty()) {
    delete sTabGroups;
    sTabGroups = nullptr;
  }
}

void TabGroup::EnsureThrottledEventQueues() {
  if (mThrottledQueuesInitialized) {
    return;
  }

  mThrottledQueuesInitialized = true;

  for (size_t i = 0; i < size_t(TaskCategory::Count); i++) {
    TaskCategory category = static_cast<TaskCategory>(i);
    if (category == TaskCategory::Worker) {
      mEventTargets[i] = ThrottledEventQueue::Create(mEventTargets[i],
                                                     "TabGroup worker queue");
    } else if (category == TaskCategory::Timer) {
      mEventTargets[i] =
          ThrottledEventQueue::Create(mEventTargets[i], "TabGroup timer queue");
    }
  }
}

/* static */
TabGroup* TabGroup::GetChromeTabGroup() {
  if (!sChromeTabGroup) {
    sChromeTabGroup = new TabGroup(true /* chrome tab group */);
    ClearOnShutdown(&sChromeTabGroup);
  }
  return sChromeTabGroup;
}

/* static */
TabGroup* TabGroup::GetFromWindow(mozIDOMWindowProxy* aWindow) {
  if (BrowserChild* browserChild = BrowserChild::GetFrom(aWindow)) {
    return browserChild->TabGroup();
  }

  return nullptr;
}

/* static */
TabGroup* TabGroup::GetFromActor(BrowserChild* aBrowserChild) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIEventTarget> target =
      aBrowserChild->Manager()->GetEventTargetFor(aBrowserChild);
  if (!target) {
    return nullptr;
  }

  // We have an event target. We assume the IPC code created it via
  // TabGroup::CreateEventTarget.
  RefPtr<SchedulerGroup> group = SchedulerGroup::FromEventTarget(target);
  MOZ_RELEASE_ASSERT(group);
  auto tabGroup = group->AsTabGroup();
  MOZ_RELEASE_ASSERT(tabGroup);

  // We delay creating the event targets until now since the TabGroup
  // constructor ran off the main thread.
  tabGroup->EnsureThrottledEventQueues();

  return tabGroup;
}

already_AddRefed<DocGroup> TabGroup::GetDocGroup(const nsACString& aKey) {
  RefPtr<DocGroup> docGroup(mDocGroups.GetEntry(aKey)->mDocGroup);
  return docGroup.forget();
}

already_AddRefed<DocGroup> TabGroup::AddDocument(const nsACString& aKey,
                                                 Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  HashEntry* entry = mDocGroups.PutEntry(aKey);
  RefPtr<DocGroup> docGroup;
  if (entry->mDocGroup) {
    docGroup = entry->mDocGroup;
  } else {
    docGroup = new DocGroup(this, aKey);
    entry->mDocGroup = docGroup;
  }

  // Make sure that the hashtable was updated and now contains the correct value
  MOZ_ASSERT(RefPtr<DocGroup>(GetDocGroup(aKey)) == docGroup);

  docGroup->mDocuments.AppendElement(aDocument);

  return docGroup.forget();
}

/* static */
already_AddRefed<TabGroup> TabGroup::Join(nsPIDOMWindowOuter* aWindow,
                                          TabGroup* aTabGroup) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<TabGroup> tabGroup = aTabGroup;
  if (!tabGroup) {
    tabGroup = new TabGroup();
  }
  MOZ_RELEASE_ASSERT(!tabGroup->mLastWindowLeft);
  MOZ_ASSERT(!tabGroup->mWindows.Contains(aWindow));
  tabGroup->mWindows.AppendElement(aWindow);

  if (!aWindow->IsBackground()) {
    tabGroup->mForegroundCount++;
  }

  return tabGroup.forget();
}

void TabGroup::Leave(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWindows.Contains(aWindow));
  mWindows.RemoveElement(aWindow);

  if (!aWindow->IsBackground()) {
    MOZ_DIAGNOSTIC_ASSERT(mForegroundCount > 0);
    mForegroundCount--;
  }

  // The Chrome TabGroup doesn't have cyclical references through mEventTargets
  // to itself, meaning that we don't have to worry about nulling mEventTargets
  // out after the last window leaves.
  if (!mIsChrome && mWindows.IsEmpty()) {
    mLastWindowLeft = true;
    Shutdown(false);
  }
}

nsresult TabGroup::FindItemWithName(const nsAString& aName,
                                    nsIDocShellTreeItem* aRequestor,
                                    nsIDocShellTreeItem* aOriginalRequestor,
                                    nsIDocShellTreeItem** aFoundItem) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aFoundItem);
  *aFoundItem = nullptr;

  MOZ_ASSERT(!aName.LowerCaseEqualsLiteral("_blank") &&
             !aName.LowerCaseEqualsLiteral("_top") &&
             !aName.LowerCaseEqualsLiteral("_parent") &&
             !aName.LowerCaseEqualsLiteral("_self"));

  for (nsPIDOMWindowOuter* outerWindow : mWindows) {
    // Ignore non-toplevel windows
    if (outerWindow->GetInProcessScriptableParentOrNull()) {
      continue;
    }

    nsCOMPtr<nsIDocShellTreeItem> docshell = outerWindow->GetDocShell();
    if (!docshell) {
      continue;
    }

    nsCOMPtr<nsIDocShellTreeItem> root;
    docshell->GetInProcessSameTypeRootTreeItem(getter_AddRefs(root));
    MOZ_RELEASE_ASSERT(docshell == root);
    if (root && aRequestor != root) {
      root->FindItemWithName(aName, aRequestor, aOriginalRequestor,
                             /* aSkipTabGroup = */ true, aFoundItem);
      if (*aFoundItem) {
        break;
      }
    }
  }

  return NS_OK;
}

nsTArray<nsPIDOMWindowOuter*> TabGroup::GetTopLevelWindows() const {
  MOZ_ASSERT(NS_IsMainThread());
  nsTArray<nsPIDOMWindowOuter*> array;

  for (nsPIDOMWindowOuter* outerWindow : mWindows) {
    if (outerWindow->GetDocShell() &&
        !outerWindow->GetInProcessScriptableParentOrNull()) {
      array.AppendElement(outerWindow);
    }
  }

  return array;
}

TabGroup::HashEntry::HashEntry(const nsACString* aKey)
    : nsCStringHashKey(aKey), mDocGroup(nullptr) {}

nsISerialEventTarget* TabGroup::EventTargetFor(TaskCategory aCategory) const {
  if (aCategory == TaskCategory::Worker || aCategory == TaskCategory::Timer) {
    MOZ_RELEASE_ASSERT(mThrottledQueuesInitialized || mIsChrome);
  }
  return SchedulerGroup::EventTargetFor(aCategory);
}

AbstractThread* TabGroup::AbstractMainThreadForImpl(TaskCategory aCategory) {
  // The mEventTargets of the chrome TabGroup are all set to do_GetMainThread().
  // We could just return AbstractThread::MainThread() without a wrapper.
  // Once we've disconnected everything, we still allow people to dispatch.
  // We'll just go directly to the main thread.
  if (this == sChromeTabGroup || NS_WARN_IF(mLastWindowLeft)) {
    return AbstractThread::MainThread();
  }

  return SchedulerGroup::AbstractMainThreadForImpl(aCategory);
}

void TabGroup::WindowChangedBackgroundStatus(bool aIsNowBackground) {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

  if (aIsNowBackground) {
    MOZ_DIAGNOSTIC_ASSERT(mForegroundCount > 0);
    mForegroundCount -= 1;
  } else {
    mForegroundCount += 1;
  }
}

bool TabGroup::IsBackground() const {
  MOZ_RELEASE_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  uint32_t foregrounded = 0;
  for (auto& window : mWindows) {
    if (!window->IsBackground()) {
      foregrounded++;
    }
  }
  MOZ_ASSERT(foregrounded == mForegroundCount);
#endif

  return mForegroundCount == 0;
}

nsresult TabGroup::QueuePostMessageEvent(
    already_AddRefed<nsIRunnable>&& aRunnable) {
  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled()) {
    if (!mPostMessageEventQueue) {
      nsCOMPtr<nsISerialEventTarget> target = GetMainThreadSerialEventTarget();
      mPostMessageEventQueue = ThrottledEventQueue::Create(
          target, "PostMessage Queue",
          nsIRunnablePriority::PRIORITY_DEFERRED_TIMERS);
      nsresult rv = mPostMessageEventQueue->SetIsPaused(false);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    // Ensure the queue is enabled
    if (mPostMessageEventQueue->IsPaused()) {
      nsresult rv = mPostMessageEventQueue->SetIsPaused(false);
      MOZ_ALWAYS_SUCCEEDS(rv);
    }

    if (mPostMessageEventQueue) {
      mPostMessageEventQueue->Dispatch(std::move(aRunnable),
                                       NS_DISPATCH_NORMAL);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

void TabGroup::FlushPostMessageEvents() {
  if (StaticPrefs::dom_separate_event_queue_for_post_message_enabled()) {
    if (mPostMessageEventQueue) {
      nsresult rv = mPostMessageEventQueue->SetIsPaused(true);
      MOZ_ALWAYS_SUCCEEDS(rv);
      nsCOMPtr<nsIRunnable> event;
      while ((event = mPostMessageEventQueue->GetEvent())) {
        Dispatch(TaskCategory::Other, event.forget());
      }
    }
  }
}

uint32_t TabGroup::Count(bool aActiveOnly) const {
  if (!aActiveOnly) {
    return mDocGroups.Count();
  }

  uint32_t count = 0;
  for (auto iter = mDocGroups.ConstIter(); !iter.Done(); iter.Next()) {
    if (iter.Get()->mDocGroup->IsActive()) {
      ++count;
    }
  }

  return count;
}

/*static*/
bool TabGroup::HasOnlyThrottableTabs() {
  if (!sTabGroups) {
    return false;
  }

  for (TabGroup* tabGroup = sTabGroups->getFirst(); tabGroup;
       tabGroup =
           static_cast<LinkedListElement<TabGroup>*>(tabGroup)->getNext()) {
    for (auto iter = tabGroup->Iter(); !iter.Done(); iter.Next()) {
      DocGroup* docGroup = iter.Get()->mDocGroup;
      for (auto* documentInDocGroup : *docGroup) {
        if (documentInDocGroup->IsCurrentActiveDocument()) {
          nsPIDOMWindowInner* win = documentInDocGroup->GetInnerWindow();
          if (win && win->IsCurrentInnerWindow()) {
            nsPIDOMWindowOuter* outer = win->GetOuterWindow();
            if (outer) {
              TimeoutManager& tm = win->TimeoutManager();
              if (!tm.BudgetThrottlingEnabled(outer->IsBackground())) {
                return false;
              }
            }
          }
        }
      }
    }
  }
  return true;
}

}  // namespace dom
}  // namespace mozilla
