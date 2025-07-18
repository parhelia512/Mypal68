/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UIDirectionManager.h"
#include "mozilla/Preferences.h"
#include "nsIWindowMediator.h"
#include "nsDocShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/SimpleEnumerator.h"

namespace mozilla::dom {

/* static */
void OnPrefChange(const char* aPrefName, void*) {
  // Iterate over all of the windows and notify them of the direction change.
  nsCOMPtr<nsIWindowMediator> windowMediator =
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);
  NS_ENSURE_TRUE_VOID(windowMediator);

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  windowMediator->GetEnumerator(nullptr, getter_AddRefs(windowEnumerator));
  NS_ENSURE_TRUE_VOID(windowEnumerator);

  for (auto& elements : SimpleEnumerator<nsISupports>(windowEnumerator)) {
    nsCOMPtr<nsPIDOMWindowOuter> window = do_QueryInterface(elements);
    if (window->Closed()) {
      continue;
    }

    nsCOMPtr<nsIDocShell> rootDocShell = window->GetDocShell();
    nsTArray<RefPtr<nsIDocShell>> docShells;
    rootDocShell->GetAllDocShellsInSubtree(
        nsIDocShell::typeAll, nsIDocShell::ENUMERATE_FORWARDS, docShells);
    for (auto& docShell : docShells) {
      if (nsCOMPtr<nsPIDOMWindowOuter> win = do_GetInterface(docShell)) {
        if (dom::Document* doc = win->GetExtantDoc()) {
          doc->ResetDocumentDirection();
        }
      }
    }
  }
}

/* static */
void UIDirectionManager::Initialize() {
  DebugOnly<nsresult> rv =
      Preferences::RegisterCallback(OnPrefChange, "intl.l10n.pseudo");
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Failed to observe \"intl.l10n.pseudo\"");
}

/* static */
void UIDirectionManager::Shutdown() {
  Preferences::UnregisterCallback(OnPrefChange, "intl.l10n.pseudo");
}

}  // namespace mozilla::dom
