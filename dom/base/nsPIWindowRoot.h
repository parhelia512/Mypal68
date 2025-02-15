/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPIWindowRoot_h__
#define nsPIWindowRoot_h__

#include "nsISupports.h"
#include "mozilla/dom/EventTarget.h"

class nsPIDOMWindowOuter;
class nsIControllers;
class nsIController;
class nsINode;

namespace mozilla {
namespace dom {
class BrowserParent;
}  // namespace dom
}  // namespace mozilla

#define NS_IWINDOWROOT_IID                           \
  {                                                  \
    0xb8724c49, 0xc398, 0x4f9b, {                    \
      0x82, 0x59, 0x87, 0x27, 0xa6, 0x47, 0xdd, 0x0f \
    }                                                \
  }

class nsPIWindowRoot : public mozilla::dom::EventTarget {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IWINDOWROOT_IID)

  virtual nsPIDOMWindowOuter* GetWindow() = 0;

  // get and set the node that is the context of a popup menu
  virtual already_AddRefed<nsINode> GetPopupNode() = 0;
  virtual void SetPopupNode(nsINode* aNode) = 0;

  /**
   * @param aForVisibleWindow   true if caller needs controller which is
   *                            associated with visible window.
   */
  virtual nsresult GetControllerForCommand(const char* aCommand,
                                           bool aForVisibleWindow,
                                           nsIController** aResult) = 0;

  /**
   * @param aForVisibleWindow   true if caller needs controllers which are
   *                            associated with visible window.
   */
  virtual nsresult GetControllers(bool aForVisibleWindow,
                                  nsIControllers** aResult) = 0;

  virtual void GetEnabledDisabledCommands(
      nsTArray<nsCString>& aEnabledCommands,
      nsTArray<nsCString>& aDisabledCommands) = 0;

  virtual void SetParentTarget(mozilla::dom::EventTarget* aTarget) = 0;
  virtual mozilla::dom::EventTarget* GetParentTarget() = 0;

  // Stores a weak reference to the browser.
  virtual void AddBrowser(mozilla::dom::BrowserParent* aBrowser) = 0;
  virtual void RemoveBrowser(mozilla::dom::BrowserParent* aBrowser) = 0;

  typedef void (*BrowserEnumerator)(mozilla::dom::BrowserParent* aTab,
                                    void* aArg);

  // Enumerate all stored browsers that for which the weak reference is valid.
  virtual void EnumerateBrowsers(BrowserEnumerator aEnumFunc, void* aArg) = 0;

  virtual bool ShowFocusRings() = 0;
  virtual void SetShowFocusRings(bool aEnable) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPIWindowRoot, NS_IWINDOWROOT_IID)

#endif  // nsPIWindowRoot_h__
