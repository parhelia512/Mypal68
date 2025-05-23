/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutDebuggingTools_h
#define nsLayoutDebuggingTools_h

#include "nsILayoutDebuggingTools.h"
#include "nsIDocShell.h"
#include "nsCOMPtr.h"

class nsLayoutDebuggingTools : public nsILayoutDebuggingTools {
 public:
  nsLayoutDebuggingTools();

  NS_DECL_ISUPPORTS

  NS_DECL_NSILAYOUTDEBUGGINGTOOLS

 protected:
  virtual ~nsLayoutDebuggingTools();

  nsresult SetBoolPrefAndRefresh(const char* aPrefName, bool aNewValue);

  nsCOMPtr<nsIDocShell> mDocShell;

  bool mPaintFlashing;
  bool mPaintDumping;
  bool mInvalidateDumping;
  bool mEventDumping;
  bool mMotionEventDumping;
  bool mCrossingEventDumping;
  bool mReflowCounts;
};

#endif
