/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorageManager_h
#define mozilla_dom_SessionStorageManager_h

#include "nsIDOMStorageManager.h"
#include "nsClassHashtable.h"
#include "nsRefPtrHashtable.h"
#include "StorageObserver.h"

namespace mozilla {
namespace dom {

class SessionStorageCache;
class SessionStorageObserver;

class SessionStorageManager final : public nsIDOMSessionStorageManager,
                                    public StorageObserverSink {
 public:
  SessionStorageManager();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEMANAGER
  NS_DECL_NSIDOMSESSIONSTORAGEMANAGER

 private:
  ~SessionStorageManager();

  // StorageObserverSink, handler to various chrome clearing notification
  nsresult Observe(const char* aTopic,
                   const nsAString& aOriginAttributesPattern,
                   const nsACString& aOriginScope) override;

  enum ClearStorageType {
    eAll,
    eSessionOnly,
  };

  void ClearStorages(ClearStorageType aType,
                     const OriginAttributesPattern& aPattern,
                     const nsACString& aOriginScope);

  nsresult GetSessionStorageCacheHelper(nsIPrincipal* aPrincipal,
                                        bool aMakeIfNeeded,
                                        SessionStorageCache* aCloneFrom,
                                        RefPtr<SessionStorageCache>* aRetVal);

  typedef nsRefPtrHashtable<nsCStringHashKey, SessionStorageCache>
      OriginKeyHashTable;
  nsClassHashtable<nsCStringHashKey, OriginKeyHashTable> mOATable;

  RefPtr<SessionStorageObserver> mObserver;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStorageManager_h
