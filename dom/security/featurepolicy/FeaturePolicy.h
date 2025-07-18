/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FeaturePolicy_h
#define mozilla_dom_FeaturePolicy_h

#include "nsCycleCollectionParticipant.h"
#include "nsIPrincipal.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

/**
 * FeaturePolicy
 * ~~~~~~~~~~~~~
 *
 * Each document and each HTMLIFrameElement have a FeaturePolicy object which is
 * used to allow or deny features in their contexts. FeaturePolicy is active
 * when pref dom.security.featurePolicy.enabled is set to true.
 *
 * FeaturePolicy is composed by a set of directives configured by the
 * 'Feature-Policy' HTTP Header and the 'allow' attribute in HTMLIFrameElements.
 * Both header and attribute are parsed by FeaturePolicyParser which returns an
 * array of Feature objects. Each Feature object has a feature name and one of
 * these policies:
 * - eNone - the feature is fully disabled.
 * - eAll - the feature is allowed.
 * - eAllowList - the feature is allowed for a list of origins.
 *
 * An interesting element of FeaturePolicy is the inheritance: each context
 * inherits the feature-policy directives from the parent context, if it exists.
 * When a context inherits a policy for feature X, it only knows if that feature
 * is allowed or denied (it ignores the list of allowed origins for instance).
 * This information is stored in an array of inherited feature strings because
 * we care only to know when they are denied.
 *
 * FeaturePolicy can be reset if the 'allow' or 'src' attributes change in
 * HTMLIFrameElements. 'src' attribute is important to compute correcly
 * the features via FeaturePolicy 'src' keyword.
 *
 * When FeaturePolicy must decide if feature X is allowed or denied for the
 * current origin, it checks if the parent context denied that feature.
 * If not, it checks if there is a Feature object for that
 * feature named X and if the origin is allowed or not.
 *
 * From a C++ point of view, use FeaturePolicyUtils to obtain the list of
 * features and to check if they are allowed in the current context.
 *
 * dom.security.featurePolicy.header.enabled pref can be used to disable the
 * HTTP header support.
 **/

class nsINode;

namespace mozilla {
namespace dom {
class Document;
class Feature;

class FeaturePolicyUtils;

class FeaturePolicy final : public nsISupports, public nsWrapperCache {
  friend class FeaturePolicyUtils;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(FeaturePolicy)

  explicit FeaturePolicy(nsINode* aNode);

  // A FeaturePolicy must have a default origin.
  // This method must be called before any other exposed WebIDL method or before
  // checking if a feature is allowed.
  void SetDefaultOrigin(nsIPrincipal* aPrincipal) {
    mDefaultOrigin = aPrincipal;
  }

  nsIPrincipal* DefaultOrigin() const { return mDefaultOrigin; }

  // Inherits the policy from the 'parent' context if it exists.
  void InheritPolicy(FeaturePolicy* aParentFeaturePolicy);

  // Sets the declarative part of the policy. This can be from the HTTP header
  // or for the 'allow' HTML attribute.
  void SetDeclaredPolicy(mozilla::dom::Document* aDocument,
                         const nsAString& aPolicyString,
                         nsIPrincipal* aSelfOrigin, nsIPrincipal* aSrcOrigin);

  // This method creates a policy for aFeatureName allowing it to '*' if it
  // doesn't exist yet. It's used by HTMLIFrameElement to enable features by
  // attributes.
  void MaybeSetAllowedPolicy(const nsAString& aFeatureName);

  // Clears all the declarative policy directives. This is needed when the
  // 'allow' attribute or the 'src' attribute change for HTMLIFrameElement's
  // policy.
  void ResetDeclaredPolicy();

  // WebIDL internal methods.

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsINode* GetParentObject() const { return mParentNode; }

  // WebIDL explosed methods.

  bool AllowsFeature(const nsAString& aFeatureName,
                     const Optional<nsAString>& aOrigin) const;

  void Features(nsTArray<nsString>& aFeatures);

  void AllowedFeatures(nsTArray<nsString>& aAllowedFeatures);

  void GetAllowlistForFeature(const nsAString& aFeatureName,
                              nsTArray<nsString>& aList) const;

 private:
  ~FeaturePolicy() = default;

  // This method returns true if the aFeatureName is allowed for aOrigin,
  // following the feature-policy directives. See the comment at the top of this
  // file.
  bool AllowsFeatureInternal(const nsAString& aFeatureName,
                             nsIPrincipal* aOrigin) const;

  // Inherits a single denied feature from the parent context.
  void SetInheritedDeniedFeature(const nsAString& aFeatureName);

  bool HasInheritedDeniedFeature(const nsAString& aFeatureName) const;

  // This returns true if we have a declared feature policy for aFeatureName.
  bool HasDeclaredFeature(const nsAString& aFeatureName) const;

  nsCOMPtr<nsINode> mParentNode;

  // This is set in sub-contexts when the parent blocks some feature for the
  // current context.
  nsTArray<nsString> mInheritedDeniedFeatureNames;

  // Feature policy for the current context.
  nsTArray<Feature> mFeatures;

  nsCOMPtr<nsIPrincipal> mDefaultOrigin;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_FeaturePolicy_h
