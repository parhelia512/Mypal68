/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationAvailability_h
#define mozilla_dom_PresentationAvailability_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/WeakPtr.h"
#include "nsIPresentationListener.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class Promise;

class PresentationAvailability final
    : public DOMEventTargetHelper,
      public nsIPresentationAvailabilityListener,
      public SupportsWeakPtr {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PresentationAvailability,
                                           DOMEventTargetHelper)
  NS_DECL_NSIPRESENTATIONAVAILABILITYLISTENER

  static already_AddRefed<PresentationAvailability> Create(
      nsPIDOMWindowInner* aWindow, const nsTArray<nsString>& aUrls,
      RefPtr<Promise>& aPromise);

  virtual void DisconnectFromOwner() override;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool Equals(const uint64_t aWindowID, const nsTArray<nsString>& aUrls) const;

  bool IsCachedValueReady();

  void EnqueuePromise(RefPtr<Promise>& aPromise);

  // WebIDL (public APIs)
  bool Value() const;

  IMPL_EVENT_HANDLER(change);

 private:
  explicit PresentationAvailability(nsPIDOMWindowInner* aWindow,
                                    const nsTArray<nsString>& aUrls);

  virtual ~PresentationAvailability();

  bool Init(RefPtr<Promise>& aPromise);

  void Shutdown();

  void UpdateAvailabilityAndDispatchEvent(bool aIsAvailable);

  bool mIsAvailable;

  nsTArray<RefPtr<Promise>> mPromises;

  nsTArray<nsString> mUrls;
  nsTArray<bool> mAvailabilityOfUrl;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PresentationAvailability_h
