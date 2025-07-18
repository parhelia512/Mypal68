/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieServiceChild_h__
#define mozilla_net_CookieServiceChild_h__

#include "CookieKey.h"
#include "mozilla/net/PCookieServiceChild.h"
#include "nsClassHashtable.h"
#include "nsICookieService.h"
#include "nsIObserver.h"
#include "nsIPrefBranch.h"
#include "mozIThirdPartyUtil.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"

class nsIEffectiveTLDService;
class nsILoadInfo;

namespace mozilla {
namespace net {

class Cookie;
class CookieStruct;

class CookieServiceChild final : public PCookieServiceChild,
                                 public nsICookieService,
                                 public nsIObserver,
                                 public nsITimerCallback,
                                 public nsSupportsWeakReference {
  friend class PCookieServiceChild;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIESERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  typedef nsTArray<RefPtr<Cookie>> CookiesList;
  typedef nsClassHashtable<CookieKey, CookiesList> CookiesMap;

  CookieServiceChild();

  static already_AddRefed<CookieServiceChild> GetSingleton();

  void TrackCookieLoad(nsIChannel* aChannel);

 private:
  ~CookieServiceChild();
  void MoveCookies();

  nsresult SetCookieStringInternal(nsIURI* aHostURI, nsIChannel* aChannel,
                                   const nsACString& aCookieString,
                                   bool aFromHttp);

  void RecordDocumentCookie(Cookie* aCookie, const OriginAttributes& aAttrs);

  uint32_t CountCookiesFromHashTable(const nsCString& aBaseDomain,
                                     const OriginAttributes& aOriginAttrs);

  void PrefChanged(nsIPrefBranch* aPrefBranch);

  static bool RequireThirdPartyCheck(nsILoadInfo* aLoadInfo);

  mozilla::ipc::IPCResult RecvTrackCookiesLoad(
      nsTArray<CookieStruct>&& aCookiesList, const OriginAttributes& aAttrs);

  mozilla::ipc::IPCResult RecvRemoveAll();

  mozilla::ipc::IPCResult RecvRemoveBatchDeletedCookies(
      nsTArray<CookieStruct>&& aCookiesList,
      nsTArray<OriginAttributes>&& aAttrsList);

  mozilla::ipc::IPCResult RecvRemoveCookie(const CookieStruct& aCookie,
                                           const OriginAttributes& aAttrs);

  mozilla::ipc::IPCResult RecvAddCookie(const CookieStruct& aCookie,
                                        const OriginAttributes& aAttrs);

  CookiesMap mCookiesMap;
  nsCOMPtr<nsITimer> mCookieTimer;
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;
  nsCOMPtr<nsIEffectiveTLDService> mTLDService;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieServiceChild_h__
