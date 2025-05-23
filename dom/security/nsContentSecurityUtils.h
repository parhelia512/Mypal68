/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static content security utilities. */

#ifndef nsContentSecurityUtils_h___
#define nsContentSecurityUtils_h___

namespace mozilla {
namespace dom {
class Document;
}  // namespace dom
}  // namespace mozilla

class nsContentSecurityUtils {
 public:
  static bool IsEvalAllowed(JSContext* cx, bool aIsSystemPrincipal,
                            const nsAString& aScript);
  static void NotifyEvalUsage(bool aIsSystemPrincipal,
                              NS_ConvertUTF8toUTF16& aFileNameA,
                              uint64_t aWindowID, uint32_t aLineNumber,
                              uint32_t aColumnNumber);

#if defined(DEBUG)
  static void AssertAboutPageHasCSP(mozilla::dom::Document* aDocument);
#endif
};

#endif /* nsContentSecurityUtils_h___ */
