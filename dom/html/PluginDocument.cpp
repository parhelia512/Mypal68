/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDocument.h"
#include "nsIPluginDocument.h"
#include "nsGkAtoms.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "DocumentInlines.h"
#include "nsIDocShellTreeItem.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentPolicyUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/PresShell.h"
#include "nsObjectLoadingContent.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace dom {

class PluginDocument final : public MediaDocument, public nsIPluginDocument {
 public:
  PluginDocument();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPLUGINDOCUMENT

  enum MediaDocumentKind MediaDocumentKind() const override {
    return MediaDocumentKind::Plugin;
  }

  nsresult StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup, nsISupports* aContainer,
                             nsIStreamListener** aDocListener,
                             bool aReset = true,
                             nsIContentSink* aSink = nullptr) override;

  void SetScriptGlobalObject(
      nsIScriptGlobalObject* aScriptGlobalObject) override;
  bool CanSavePresentation(nsIRequest* aNewRequest) override;

  const nsCString& GetType() const { return mMimeType; }
  Element* GetPluginContent() { return mPluginContent; }

  virtual void Destroy() override {
    if (mStreamListener) {
      mStreamListener->DropDocumentRef();
    }
    MediaDocument::Destroy();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PluginDocument, MediaDocument)
 protected:
  ~PluginDocument() override;

  nsresult CreateSyntheticPluginDocument();

  nsCOMPtr<Element> mPluginContent;
  RefPtr<MediaDocumentStreamListener> mStreamListener;
  nsCString mMimeType;
};

class PluginStreamListener : public MediaDocumentStreamListener {
 public:
  explicit PluginStreamListener(PluginDocument* aDoc)
      : MediaDocumentStreamListener(aDoc), mPluginDoc(aDoc) {}
  NS_IMETHOD OnStartRequest(nsIRequest* request) override;

 private:
  RefPtr<PluginDocument> mPluginDoc;
};

NS_IMETHODIMP
PluginStreamListener::OnStartRequest(nsIRequest* request) {
  AUTO_PROFILER_LABEL("PluginStreamListener::OnStartRequest", NETWORK);

  nsCOMPtr<nsIContent> embed = mPluginDoc->GetPluginContent();
  nsCOMPtr<nsIObjectLoadingContent> objlc = do_QueryInterface(embed);
  nsCOMPtr<nsIStreamListener> objListener = do_QueryInterface(objlc);

  if (!objListener) {
    MOZ_ASSERT_UNREACHABLE(
        "PluginStreamListener without appropriate content "
        "node");
    return NS_BINDING_ABORTED;
  }

  SetStreamListener(objListener);

  // Sets up the ObjectLoadingContent tag as if it is waiting for a
  // channel, so it can proceed with a load normally once it gets OnStartRequest
  nsresult rv = objlc->InitializeFromChannel(request);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT_UNREACHABLE("InitializeFromChannel failed");
    return rv;
  }

  // Note that because we're now hooked up to a plugin listener, this will
  // likely spawn a plugin, which may re-enter.
  return MediaDocumentStreamListener::OnStartRequest(request);
}

PluginDocument::PluginDocument() = default;

PluginDocument::~PluginDocument() = default;

NS_IMPL_CYCLE_COLLECTION_INHERITED(PluginDocument, MediaDocument,
                                   mPluginContent)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(PluginDocument, MediaDocument,
                                             nsIPluginDocument)

void PluginDocument::SetScriptGlobalObject(
    nsIScriptGlobalObject* aScriptGlobalObject) {
  // Set the script global object on the superclass before doing
  // anything that might require it....
  MediaDocument::SetScriptGlobalObject(aScriptGlobalObject);

  if (aScriptGlobalObject) {
    if (!InitialSetupHasBeenDone()) {
      // Create synthetic document
#ifdef DEBUG
      nsresult rv =
#endif
          CreateSyntheticPluginDocument();
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create synthetic document");
      InitialSetupDone();
    }
  } else {
    mStreamListener = nullptr;
  }
}

bool PluginDocument::CanSavePresentation(nsIRequest* aNewRequest) {
  // Full-page plugins cannot be cached, currently, because we don't have
  // the stream listener data to feed to the plugin instance.
  return false;
}

nsresult PluginDocument::StartDocumentLoad(const char* aCommand,
                                           nsIChannel* aChannel,
                                           nsILoadGroup* aLoadGroup,
                                           nsISupports* aContainer,
                                           nsIStreamListener** aDocListener,
                                           bool aReset, nsIContentSink* aSink) {
  // do not allow message panes to host full-page plugins
  // returning an error causes helper apps to take over
  nsCOMPtr<nsIDocShellTreeItem> dsti(do_QueryInterface(aContainer));
  if (dsti) {
    bool isMsgPane = false;
    dsti->NameEquals(u"messagepane"_ns, &isMsgPane);
    if (isMsgPane) {
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv = MediaDocument::StartDocumentLoad(
      aCommand, aChannel, aLoadGroup, aContainer, aDocListener, aReset, aSink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = aChannel->GetContentType(mMimeType);
  if (NS_FAILED(rv)) {
    return rv;
  }

  MediaDocument::UpdateTitleAndCharset(mMimeType, aChannel);

  mStreamListener = new PluginStreamListener(this);
  NS_ASSERTION(aDocListener, "null aDocListener");
  NS_ADDREF(*aDocListener = mStreamListener);

  return rv;
}

nsresult PluginDocument::CreateSyntheticPluginDocument() {
  NS_ASSERTION(!GetPresShell() || !GetPresShell()->DidInitialize(),
               "Creating synthetic plugin document content too late");

  // make our generic document
  nsresult rv = MediaDocument::CreateSyntheticDocument();
  NS_ENSURE_SUCCESS(rv, rv);
  // then attach our plugin

  RefPtr<Element> body = GetBodyElement();
  if (!body) {
    NS_WARNING("no body on plugin document!");
    return NS_ERROR_FAILURE;
  }

  // remove margins from body
  constexpr auto zero = u"0"_ns;
  body->SetAttr(kNameSpaceID_None, nsGkAtoms::marginwidth, zero, false);
  body->SetAttr(kNameSpaceID_None, nsGkAtoms::marginheight, zero, false);

  // make plugin content
  RefPtr<mozilla::dom::NodeInfo> nodeInfo;
  nodeInfo = mNodeInfoManager->GetNodeInfo(
      nsGkAtoms::embed, nullptr, kNameSpaceID_XHTML, nsINode::ELEMENT_NODE);
  rv = NS_NewHTMLElement(getter_AddRefs(mPluginContent), nodeInfo.forget(),
                         NOT_FROM_PARSER);
  NS_ENSURE_SUCCESS(rv, rv);

  // make it a named element
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::name, u"plugin"_ns,
                          false);

  // fill viewport and auto-resize
  constexpr auto percent100 = u"100%"_ns;
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::width, percent100,
                          false);
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::height, percent100,
                          false);

  // set URL
  nsAutoCString src;
  mDocumentURI->GetSpec(src);
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::src,
                          NS_ConvertUTF8toUTF16(src), false);

  // set mime type
  mPluginContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                          NS_ConvertUTF8toUTF16(mMimeType), false);

  // nsHTML(Shared)ObjectElement does not kick off a load on BindToTree if it is
  // to a PluginDocument
  body->AppendChildTo(mPluginContent, false);

  return NS_OK;
}

NS_IMETHODIMP
PluginDocument::Print() {
  NS_ENSURE_TRUE(mPluginContent, NS_ERROR_FAILURE);

  nsIObjectFrame* objectFrame =
      do_QueryFrame(mPluginContent->GetPrimaryFrame());
  if (objectFrame) {
    RefPtr<nsNPAPIPluginInstance> pi = objectFrame->GetPluginInstance();
    if (pi) {
      NPPrint npprint;
      npprint.mode = NP_FULL;
      npprint.print.fullPrint.pluginPrinted = false;
      npprint.print.fullPrint.printOne = false;
      npprint.print.fullPrint.platformPrint = nullptr;

      pi->Print(&npprint);
    }
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla

nsresult NS_NewPluginDocument(mozilla::dom::Document** aResult) {
  auto* doc = new mozilla::dom::PluginDocument();

  NS_ADDREF(doc);
  nsresult rv = doc->Init();

  if (NS_FAILED(rv)) {
    NS_RELEASE(doc);
  }

  *aResult = doc;

  return rv;
}
