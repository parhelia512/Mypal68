/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_base_nsPIDOMWindowInlines_h___
#define dom_base_nsPIDOMWindowInlines_h___

inline bool nsPIDOMWindowOuter::IsLoading() const {
  auto* win = GetCurrentInnerWindow();

  if (!win) {
    NS_ERROR("No current inner window available!");

    return false;
  }

  return win->IsLoading();
}

inline bool nsPIDOMWindowInner::IsLoading() const {
  if (!mOuterWindow) {
    NS_ERROR("IsLoading() called on orphan inner window!");

    return false;
  }

  return !mIsDocumentLoaded;
}

inline bool nsPIDOMWindowOuter::IsHandlingResizeEvent() const {
  auto* win = GetCurrentInnerWindow();

  if (!win) {
    NS_ERROR("No current inner window available!");

    return false;
  }

  return win->IsHandlingResizeEvent();
}

inline bool nsPIDOMWindowInner::IsHandlingResizeEvent() const {
  if (!mOuterWindow) {
    NS_ERROR("IsHandlingResizeEvent() called on orphan inner window!");

    return false;
  }

  return mIsHandlingResizeEvent;
}

inline bool nsPIDOMWindowInner::IsCurrentInnerWindow() const {
  return mOuterWindow && mOuterWindow->GetCurrentInnerWindow() == this;
}

inline bool nsPIDOMWindowInner::HasActiveDocument() {
  return IsCurrentInnerWindow() ||
         (mOuterWindow && mOuterWindow->GetCurrentInnerWindow() &&
          mOuterWindow->GetCurrentInnerWindow()->GetDoc() == mDoc);
}

inline bool nsPIDOMWindowInner::IsTopInnerWindow() const {
  return mTopInnerWindow == this;
}

inline nsIDocShell* nsPIDOMWindowOuter::GetDocShell() const {
  return mDocShell;
}

inline nsIDocShell* nsPIDOMWindowInner::GetDocShell() const {
  return mOuterWindow ? mOuterWindow->GetDocShell() : nullptr;
}

inline mozilla::dom::BrowsingContext* nsPIDOMWindowOuter::GetBrowsingContext()
    const {
  return mBrowsingContext;
}

inline mozilla::dom::Element* nsPIDOMWindowOuter::GetFocusedElement() const {
  return mInnerWindow ? mInnerWindow->GetFocusedElement() : nullptr;
}

inline bool nsPIDOMWindowOuter::FocusedElementShowedOutline() const {
  return mInnerWindow && mInnerWindow->FocusedElementShowedOutline();
}

#endif
