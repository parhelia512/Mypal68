/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/HTMLMetaElement.h"
#include "mozilla/dom/HTMLMetaElementBinding.h"
#include "mozilla/dom/nsCSPService.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/ViewportMetaData.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_security.h"
#include "nsContentUtils.h"
#include "nsStyleConsts.h"
#include "nsIContentSecurityPolicy.h"

static mozilla::LazyLogModule gMetaElementLog("nsMetaElement");
#define LOG(msg) MOZ_LOG(gMetaElementLog, mozilla::LogLevel::Debug, msg)
#define LOG_ENABLED() MOZ_LOG_TEST(gMetaElementLog, mozilla::LogLevel::Debug)

NS_IMPL_NS_NEW_HTML_ELEMENT(Meta)

namespace mozilla {
namespace dom {

HTMLMetaElement::HTMLMetaElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {}

HTMLMetaElement::~HTMLMetaElement() = default;

NS_IMPL_ELEMENT_CLONE(HTMLMetaElement)

void HTMLMetaElement::SetMetaReferrer(Document* aDocument) {
  if (!aDocument || !AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                 nsGkAtoms::referrer, eIgnoreCase)) {
    return;
  }
  nsAutoString content;
  GetContent(content);

  Element* headElt = aDocument->GetHeadElement();
  if (headElt && IsInclusiveDescendantOf(headElt)) {
    content = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
        content);
    aDocument->UpdateReferrerInfoFromMeta(content, false);
  }
}

nsresult HTMLMetaElement::AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                       const nsAttrValue* aValue,
                                       const nsAttrValue* aOldValue,
                                       nsIPrincipal* aSubjectPrincipal,
                                       bool aNotify) {
  if (aNameSpaceID == kNameSpaceID_None) {
    Document* document = GetUncomposedDoc();
    if (aName == nsGkAtoms::content) {
      if (document && AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                  nsGkAtoms::viewport, eIgnoreCase)) {
        ProcessViewportContent(document);
      }
      CreateAndDispatchEvent(document, u"DOMMetaChanged"_ns);
    } else if (document && aName == nsGkAtoms::name) {
      if (aValue && aValue->Equals(nsGkAtoms::viewport, eIgnoreCase)) {
        ProcessViewportContent(document);
      } else if (aOldValue &&
                 aOldValue->Equals(nsGkAtoms::viewport, eIgnoreCase)) {
        DiscardViewportContent(document);
      }
      CreateAndDispatchEvent(document, u"DOMMetaChanged"_ns);
    }
    // Update referrer policy when it got changed from JS
    SetMetaReferrer(document);
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNameSpaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLMetaElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!IsInUncomposedDoc()) {
    return rv;
  }
  Document& doc = aContext.OwnerDoc();
  if (AttrValueIs(kNameSpaceID_None, nsGkAtoms::name, nsGkAtoms::viewport,
                  eIgnoreCase)) {
    ProcessViewportContent(&doc);
  }

  if (AttrValueIs(kNameSpaceID_None, nsGkAtoms::httpEquiv, nsGkAtoms::headerCSP,
                  eIgnoreCase)) {
    // only accept <meta http-equiv="Content-Security-Policy" content=""> if it
    // appears in the <head> element.
    Element* headElt = doc.GetHeadElement();
    if (headElt && IsInclusiveDescendantOf(headElt)) {
      nsAutoString content;
      GetContent(content);

      if (LOG_ENABLED()) {
        nsAutoCString documentURIspec;
        if (nsIURI* documentURI = doc.GetDocumentURI()) {
          documentURI->GetAsciiSpec(documentURIspec);
        }

        LOG(
            ("HTMLMetaElement %p sets CSP '%s' on document=%p, "
             "document-uri=%s",
             this, NS_ConvertUTF16toUTF8(content).get(), &doc,
             documentURIspec.get()));
      }
      CSP_ApplyMetaCSPToDoc(doc, content);
    }
  }

  // Referrer Policy spec requires a <meta name="referrer" tag to be in the
  // <head> element.
  SetMetaReferrer(&doc);
  CreateAndDispatchEvent(&doc, u"DOMMetaAdded"_ns);
  return rv;
}

void HTMLMetaElement::UnbindFromTree(bool aNullParent) {
  nsCOMPtr<Document> oldDoc = GetUncomposedDoc();
  if (oldDoc && AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                            nsGkAtoms::viewport, eIgnoreCase)) {
    DiscardViewportContent(oldDoc);
  }
  CreateAndDispatchEvent(oldDoc, u"DOMMetaRemoved"_ns);
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

void HTMLMetaElement::CreateAndDispatchEvent(Document* aDoc,
                                             const nsAString& aEventName) {
  if (!aDoc) return;

  RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
      this, aEventName, CanBubble::eYes, ChromeOnlyDispatch::eYes);
  asyncDispatcher->RunDOMEventWhenSafe();
}

JSObject* HTMLMetaElement::WrapNode(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return HTMLMetaElement_Binding::Wrap(aCx, this, aGivenProto);
}

void HTMLMetaElement::ProcessViewportContent(Document* aDocument) {
  nsAutoString content;
  GetContent(content);

  aDocument->SetHeaderData(nsGkAtoms::viewport, content);

  ViewportMetaData data(content);
  aDocument->AddMetaViewportElement(this, std::move(data));
}

void HTMLMetaElement::DiscardViewportContent(Document* aDocument) {
  aDocument->RemoveMetaViewportElement(this);
}

}  // namespace dom
}  // namespace mozilla
