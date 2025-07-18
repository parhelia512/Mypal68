/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHyphenationManager_h__
#define nsHyphenationManager_h__

#include "nsInterfaceHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "mozilla/Omnijar.h"
#include "mozilla/ipc/SharedMemoryBasic.h"

class nsHyphenator;
class nsAtom;
class nsIURI;

class nsHyphenationManager {
 public:
  nsHyphenationManager();

  already_AddRefed<nsHyphenator> GetHyphenator(nsAtom* aLocale);

  void ShareHyphDictToProcess(
      nsIURI* aURI, base::ProcessId aPid,
      mozilla::ipc::SharedMemoryBasic::Handle* aOutHandle, uint32_t* aOutSize);

  static nsHyphenationManager* Instance();

  static void Shutdown();

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf);

 private:
  ~nsHyphenationManager();

 protected:
  class MemoryPressureObserver final : public nsIObserver {
    ~MemoryPressureObserver() {}

   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
  };

  void LoadPatternList();
  void LoadPatternListFromOmnijar(mozilla::Omnijar::Type aType);
  void LoadPatternListFromDir(nsIFile* aDir);
  void LoadAliases();

  nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, nsAtom> mHyphAliases;
  nsInterfaceHashtable<nsRefPtrHashKey<nsAtom>, nsIURI> mPatternFiles;
  nsRefPtrHashtable<nsRefPtrHashKey<nsAtom>, nsHyphenator> mHyphenators;

  static nsHyphenationManager* sInstance;
};

#endif  // nsHyphenationManager_h__
