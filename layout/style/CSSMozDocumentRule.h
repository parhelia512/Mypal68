/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CSSMozDocumentRule_h
#define mozilla_dom_CSSMozDocumentRule_h

#include "mozilla/css/GroupRule.h"
#include "mozilla/css/DocumentMatchingFunction.h"
#include "mozilla/ServoBindingTypes.h"

namespace mozilla {
namespace dom {

class CSSMozDocumentRule final : public css::ConditionRule {
 public:
  CSSMozDocumentRule(RefPtr<RawServoMozDocumentRule> aRawRule,
                     StyleSheet* aSheet, css::Rule* aParentRule, uint32_t aLine,
                     uint32_t aColumn);

  NS_DECL_ISUPPORTS_INHERITED

  static bool Match(const Document*, nsIURI* aDocURI,
                    const nsACString& aDocURISpec, const nsACString& aPattern,
                    css::DocumentMatchingFunction);

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const final;
#endif

  RawServoMozDocumentRule* Raw() const { return mRawRule; }
  void SetRawAfterClone(RefPtr<RawServoMozDocumentRule>);

  // WebIDL interface
  StyleCssRuleType Type() const final;
  void GetCssText(nsACString& aCssText) const final;
  void GetConditionText(nsACString& aConditionText) final;
  void SetConditionText(const nsACString& aConditionText,
                        ErrorResult& aRv) final;

  size_t SizeOfIncludingThis(MallocSizeOf) const override;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~CSSMozDocumentRule() = default;

  RefPtr<RawServoMozDocumentRule> mRawRule;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CSSMozDocumentRule_h
