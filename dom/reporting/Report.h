/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Report_h
#define mozilla_dom_Report_h

#include "js/RootingAPI.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class ReportBody;

class Report final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Report)

  Report(nsPIDOMWindowInner* aWindow, const nsAString& aType,
         const nsAString& aURL, ReportBody* aBody);

  already_AddRefed<Report> Clone();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  void GetType(nsAString& aType) const;

  void GetUrl(nsAString& aURL) const;

  ReportBody* GetBody() const;

 private:
  ~Report();

  nsCOMPtr<nsPIDOMWindowInner> mWindow;

  const nsString mType;
  const nsString mURL;
  RefPtr<ReportBody> mBody;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Report_h
