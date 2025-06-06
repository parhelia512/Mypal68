/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMIMEInfoAndroid_h
#define nsMIMEInfoAndroid_h

#include "nsMIMEInfoImpl.h"
#include "nsIMutableArray.h"
#include "nsAndroidHandlerApp.h"

class nsMIMEInfoAndroid final : public nsIMIMEInfo {
 public:
  static MOZ_MUST_USE bool GetMimeInfoForMimeType(
      const nsACString& aMimeType, nsMIMEInfoAndroid** aMimeInfo);
  static MOZ_MUST_USE bool GetMimeInfoForFileExt(const nsACString& aFileExt,
                                                 nsMIMEInfoAndroid** aMimeInfo);

  static MOZ_MUST_USE nsresult GetMimeInfoForURL(const nsACString& aURL,
                                                 bool* found,
                                                 nsIHandlerInfo** info);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMIMEINFO
  NS_DECL_NSIHANDLERINFO

  explicit nsMIMEInfoAndroid(const nsACString& aMIMEType);

 private:
  ~nsMIMEInfoAndroid() {}

  /**
   * Internal helper to avoid adding duplicates.
   */
  void AddUniqueExtension(const nsACString& aExtension);

  virtual MOZ_MUST_USE nsresult LaunchDefaultWithFile(nsIFile* aFile);
  virtual MOZ_MUST_USE nsresult LoadUriInternal(nsIURI* aURI);
  nsCOMPtr<nsIMutableArray> mHandlerApps;
  nsCString mType;
  nsTArray<nsCString> mExtensions;
  bool mAlwaysAsk;
  nsHandlerInfoAction mPrefAction;
  nsString mDescription;
  nsCOMPtr<nsIHandlerApp> mPrefApp;

 public:
  class SystemChooser final : public nsIHandlerApp {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHANDLERAPP
    explicit SystemChooser(nsMIMEInfoAndroid* aOuter) : mOuter(aOuter) {}

   private:
    ~SystemChooser() {}

    nsMIMEInfoAndroid* mOuter;
  };
};

#endif /* nsMIMEInfoAndroid_h */
