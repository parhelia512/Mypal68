/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpBaseChannel_h
#define mozilla_net_HttpBaseChannel_h

#include <utility>

#include "mozilla/Atomics.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Tuple.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/net/DNS.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/PrivateBrowsingChannel.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsHashPropertyBag.h"
#include "nsHttp.h"
#include "nsHttpConnectionInfo.h"
#include "nsHttpHandler.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsIApplicationCache.h"
#include "nsIClassOfService.h"
#include "nsIClassifiedChannel.h"
#include "nsIConsoleReportCollector.h"
#include "nsIEncodedChannel.h"
#include "nsIForcePendingChannel.h"
#include "nsIFormPOSTActionChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsILoadInfo.h"
#include "nsIProgressEventSink.h"
#include "nsIResumableChannel.h"
#include "nsISecurityConsoleMessage.h"
#include "nsIStringEnumerator.h"
#include "nsISupportsPriority.h"
#include "nsIThrottledInputChannel.h"
#include "nsITimedChannel.h"
#include "nsITraceableChannel.h"
#include "nsIURI.h"
#include "nsIUploadChannel2.h"
#include "nsProxyInfo.h"
#include "nsStringEnumerator.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#define HTTP_BASE_CHANNEL_IID                        \
  {                                                  \
    0x9d5cde03, 0xe6e9, 0x4612, {                    \
      0xbf, 0xef, 0xbb, 0x66, 0xf3, 0xbb, 0x74, 0x46 \
    }                                                \
  }

class nsISecurityConsoleMessage;
class nsIPrincipal;

namespace mozilla {

namespace dom {
class PerformanceStorage;
}

class LogCollector;

namespace net {
extern mozilla::LazyLogModule gHttpLog;

class PreferredAlternativeDataTypeParams;

enum CacheDisposition : uint8_t {
  kCacheUnresolved = 0,
  kCacheHit = 1,
  kCacheHitViaReval = 2,
  kCacheMissedViaReval = 3,
  kCacheMissed = 4,
  kCacheUnknown = 5
};

/*
 * This class is a partial implementation of nsIHttpChannel.  It contains code
 * shared by nsHttpChannel and HttpChannelChild.
 * - Note that this class has nothing to do with nsBaseChannel, which is an
 *   earlier effort at a base class for channels that somehow never made it all
 *   the way to the HTTP channel.
 */
class HttpBaseChannel : public nsHashPropertyBag,
                        public nsIEncodedChannel,
                        public nsIHttpChannel,
                        public nsIHttpChannelInternal,
                        public nsIFormPOSTActionChannel,
                        public nsIUploadChannel2,
                        public nsISupportsPriority,
                        public nsIClassOfService,
                        public nsIResumableChannel,
                        public nsITraceableChannel,
                        public PrivateBrowsingChannel<HttpBaseChannel>,
                        public nsITimedChannel,
                        public nsIForcePendingChannel,
                        public nsIConsoleReportCollector,
                        public nsIThrottledInputChannel,
                        public nsIClassifiedChannel {
 protected:
  virtual ~HttpBaseChannel();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIUPLOADCHANNEL
  NS_DECL_NSIFORMPOSTACTIONCHANNEL
  NS_DECL_NSIUPLOADCHANNEL2
  NS_DECL_NSITRACEABLECHANNEL
  NS_DECL_NSITIMEDCHANNEL
  NS_DECL_NSITHROTTLEDINPUTCHANNEL
  NS_DECL_NSICLASSIFIEDCHANNEL

  NS_DECLARE_STATIC_IID_ACCESSOR(HTTP_BASE_CHANNEL_IID)

  HttpBaseChannel();

  virtual MOZ_MUST_USE nsresult Init(nsIURI* aURI, uint32_t aCaps,
                                     nsProxyInfo* aProxyInfo,
                                     uint32_t aProxyResolveFlags,
                                     nsIURI* aProxyURI, uint64_t aChannelId,
                                     nsContentPolicyType aContentPolicyType);

  // nsIRequest
  NS_IMETHOD GetName(nsACString& aName) override;
  NS_IMETHOD IsPending(bool* aIsPending) override;
  NS_IMETHOD GetStatus(nsresult* aStatus) override;
  NS_IMETHOD GetLoadGroup(nsILoadGroup** aLoadGroup) override;
  NS_IMETHOD SetLoadGroup(nsILoadGroup* aLoadGroup) override;
  NS_IMETHOD GetLoadFlags(nsLoadFlags* aLoadFlags) override;
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) override;
  NS_IMETHOD SetDocshellUserAgentOverride();

  // nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI** aOriginalURI) override;
  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) override;
  NS_IMETHOD GetURI(nsIURI** aURI) override;
  NS_IMETHOD GetOwner(nsISupports** aOwner) override;
  NS_IMETHOD SetOwner(nsISupports* aOwner) override;
  NS_IMETHOD GetLoadInfo(nsILoadInfo** aLoadInfo) override;
  NS_IMETHOD SetLoadInfo(nsILoadInfo* aLoadInfo) override;
  NS_IMETHOD GetIsDocument(bool* aIsDocument) override;
  NS_IMETHOD GetNotificationCallbacks(
      nsIInterfaceRequestor** aCallbacks) override;
  NS_IMETHOD SetNotificationCallbacks(
      nsIInterfaceRequestor* aCallbacks) override;
  NS_IMETHOD GetContentType(nsACString& aContentType) override;
  NS_IMETHOD SetContentType(const nsACString& aContentType) override;
  NS_IMETHOD GetContentCharset(nsACString& aContentCharset) override;
  NS_IMETHOD SetContentCharset(const nsACString& aContentCharset) override;
  NS_IMETHOD GetContentDisposition(uint32_t* aContentDisposition) override;
  NS_IMETHOD SetContentDisposition(uint32_t aContentDisposition) override;
  NS_IMETHOD GetContentDispositionFilename(
      nsAString& aContentDispositionFilename) override;
  NS_IMETHOD SetContentDispositionFilename(
      const nsAString& aContentDispositionFilename) override;
  NS_IMETHOD GetContentDispositionHeader(
      nsACString& aContentDispositionHeader) override;
  NS_IMETHOD GetContentLength(int64_t* aContentLength) override;
  NS_IMETHOD SetContentLength(int64_t aContentLength) override;
  NS_IMETHOD Open(nsIInputStream** aResult) override;
  NS_IMETHOD GetBlockAuthPrompt(bool* aValue) override;
  NS_IMETHOD SetBlockAuthPrompt(bool aValue) override;

  // nsIEncodedChannel
  NS_IMETHOD GetApplyConversion(bool* value) override;
  NS_IMETHOD SetApplyConversion(bool value) override;
  NS_IMETHOD GetContentEncodings(nsIUTF8StringEnumerator** aEncodings) override;
  NS_IMETHOD DoApplyContentConversions(nsIStreamListener* aNextListener,
                                       nsIStreamListener** aNewNextListener,
                                       nsISupports* aCtxt) override;

  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD GetRequestMethod(nsACString& aMethod) override;
  NS_IMETHOD SetRequestMethod(const nsACString& aMethod) override;
  NS_IMETHOD GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) override;
  NS_IMETHOD SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) override;
  NS_IMETHOD SetReferrerInfoWithoutClone(
      nsIReferrerInfo* aReferrerInfo) override;
  NS_IMETHOD GetRequestHeader(const nsACString& aHeader,
                              nsACString& aValue) override;
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader,
                              const nsACString& aValue, bool aMerge) override;
  NS_IMETHOD SetEmptyRequestHeader(const nsACString& aHeader) override;
  NS_IMETHOD VisitRequestHeaders(nsIHttpHeaderVisitor* visitor) override;
  NS_IMETHOD VisitNonDefaultRequestHeaders(
      nsIHttpHeaderVisitor* visitor) override;
  NS_IMETHOD ShouldStripRequestBodyHeader(const nsACString& aMethod,
                                          bool* aResult) override;
  NS_IMETHOD GetResponseHeader(const nsACString& header,
                               nsACString& value) override;
  NS_IMETHOD SetResponseHeader(const nsACString& header,
                               const nsACString& value, bool merge) override;
  NS_IMETHOD VisitResponseHeaders(nsIHttpHeaderVisitor* visitor) override;
  NS_IMETHOD GetOriginalResponseHeader(const nsACString& aHeader,
                                       nsIHttpHeaderVisitor* aVisitor) override;
  NS_IMETHOD VisitOriginalResponseHeaders(
      nsIHttpHeaderVisitor* aVisitor) override;
  NS_IMETHOD GetAllowPipelining(bool* value) override;  // deprecated
  NS_IMETHOD SetAllowPipelining(bool value) override;   // deprecated
  NS_IMETHOD GetAllowSTS(bool* value) override;
  NS_IMETHOD SetAllowSTS(bool value) override;
  NS_IMETHOD GetRedirectionLimit(uint32_t* value) override;
  NS_IMETHOD SetRedirectionLimit(uint32_t value) override;
  NS_IMETHOD IsNoStoreResponse(bool* value) override;
  NS_IMETHOD IsNoCacheResponse(bool* value) override;
  NS_IMETHOD IsPrivateResponse(bool* value) override;
  NS_IMETHOD GetResponseStatus(uint32_t* aValue) override;
  NS_IMETHOD GetResponseStatusText(nsACString& aValue) override;
  NS_IMETHOD GetRequestSucceeded(bool* aValue) override;
  NS_IMETHOD RedirectTo(nsIURI* newURI) override;
  NS_IMETHOD SwitchProcessTo(mozilla::dom::Promise* aBrowserParent,
                             uint64_t aIdentifier) override;
  NS_IMETHOD HasCrossOriginOpenerPolicyMismatch(bool* aMismatch) override;
  NS_IMETHOD UpgradeToSecure() override;
  NS_IMETHOD GetRequestContextID(uint64_t* aRCID) override;
  NS_IMETHOD GetTransferSize(uint64_t* aTransferSize) override;
  NS_IMETHOD GetDecodedBodySize(uint64_t* aDecodedBodySize) override;
  NS_IMETHOD GetEncodedBodySize(uint64_t* aEncodedBodySize) override;
  NS_IMETHOD SetRequestContextID(uint64_t aRCID) override;
  NS_IMETHOD GetIsMainDocumentChannel(bool* aValue) override;
  NS_IMETHOD SetIsMainDocumentChannel(bool aValue) override;
  NS_IMETHOD GetProtocolVersion(nsACString& aProtocolVersion) override;
  NS_IMETHOD GetChannelId(uint64_t* aChannelId) override;
  NS_IMETHOD SetChannelId(uint64_t aChannelId) override;
  NS_IMETHOD GetTopLevelContentWindowId(uint64_t* aContentWindowId) override;
  NS_IMETHOD SetTopLevelContentWindowId(uint64_t aContentWindowId) override;
  NS_IMETHOD GetTopLevelOuterContentWindowId(uint64_t* aWindowId) override;
  NS_IMETHOD SetTopLevelOuterContentWindowId(uint64_t aWindowId) override;

  NS_IMETHOD GetFlashPluginState(
      nsIHttpChannel::FlashPluginState* aState) override;

  using nsIClassifiedChannel::IsThirdPartyTrackingResource;

  // nsIHttpChannelInternal
  NS_IMETHOD GetDocumentURI(nsIURI** aDocumentURI) override;
  NS_IMETHOD SetDocumentURI(nsIURI* aDocumentURI) override;
  NS_IMETHOD GetRequestVersion(uint32_t* major, uint32_t* minor) override;
  NS_IMETHOD GetResponseVersion(uint32_t* major, uint32_t* minor) override;
  NS_IMETHOD SetCookie(const nsACString& aCookieHeader) override;
  NS_IMETHOD GetThirdPartyFlags(uint32_t* aForce) override;
  NS_IMETHOD SetThirdPartyFlags(uint32_t aForce) override;
  NS_IMETHOD GetForceAllowThirdPartyCookie(bool* aForce) override;
  NS_IMETHOD SetForceAllowThirdPartyCookie(bool aForce) override;
  NS_IMETHOD GetCanceled(bool* aCanceled) override;
  NS_IMETHOD GetChannelIsForDownload(bool* aChannelIsForDownload) override;
  NS_IMETHOD SetChannelIsForDownload(bool aChannelIsForDownload) override;
  NS_IMETHOD SetCacheKeysRedirectChain(nsTArray<nsCString>* cacheKeys) override;
  NS_IMETHOD GetLocalAddress(nsACString& addr) override;
  NS_IMETHOD GetLocalPort(int32_t* port) override;
  NS_IMETHOD GetRemoteAddress(nsACString& addr) override;
  NS_IMETHOD GetRemotePort(int32_t* port) override;
  NS_IMETHOD GetOnlyConnect(bool* aOnlyConnect) override;
  NS_IMETHOD SetConnectOnly() override;
  NS_IMETHOD GetAllowSpdy(bool* aAllowSpdy) override;
  NS_IMETHOD SetAllowSpdy(bool aAllowSpdy) override;
  NS_IMETHOD GetAllowAltSvc(bool* aAllowAltSvc) override;
  NS_IMETHOD SetAllowAltSvc(bool aAllowAltSvc) override;
  NS_IMETHOD GetBeConservative(bool* aBeConservative) override;
  NS_IMETHOD SetBeConservative(bool aBeConservative) override;
  NS_IMETHOD GetIsTRRServiceChannel(bool* aTRR) override;
  NS_IMETHOD SetIsTRRServiceChannel(bool aTRR) override;
  NS_IMETHOD GetIsResolvedByTRR(bool* aResolvedByTRR) override;
  NS_IMETHOD GetTlsFlags(uint32_t* aTlsFlags) override;
  NS_IMETHOD SetTlsFlags(uint32_t aTlsFlags) override;
  NS_IMETHOD GetApiRedirectToURI(nsIURI** aApiRedirectToURI) override;
  virtual MOZ_MUST_USE nsresult AddSecurityMessage(
      const nsAString& aMessageTag, const nsAString& aMessageCategory);
  NS_IMETHOD TakeAllSecurityMessages(
      nsCOMArray<nsISecurityConsoleMessage>& aMessages) override;
  NS_IMETHOD GetResponseTimeoutEnabled(bool* aEnable) override;
  NS_IMETHOD SetResponseTimeoutEnabled(bool aEnable) override;
  NS_IMETHOD GetInitialRwin(uint32_t* aRwin) override;
  NS_IMETHOD SetInitialRwin(uint32_t aRwin) override;
  NS_IMETHOD ForcePending(bool aForcePending) override;
  NS_IMETHOD GetLastModifiedTime(PRTime* lastModifiedTime) override;
  NS_IMETHOD GetCorsIncludeCredentials(bool* aInclude) override;
  NS_IMETHOD SetCorsIncludeCredentials(bool aInclude) override;
  NS_IMETHOD GetCorsMode(uint32_t* aCorsMode) override;
  NS_IMETHOD SetCorsMode(uint32_t aCorsMode) override;
  NS_IMETHOD GetRedirectMode(uint32_t* aRedirectMode) override;
  NS_IMETHOD SetRedirectMode(uint32_t aRedirectMode) override;
  NS_IMETHOD GetFetchCacheMode(uint32_t* aFetchCacheMode) override;
  NS_IMETHOD SetFetchCacheMode(uint32_t aFetchCacheMode) override;
  NS_IMETHOD GetTopWindowURI(nsIURI** aTopWindowURI) override;
  NS_IMETHOD GetContentBlockingAllowListPrincipal(
      nsIPrincipal** aPrincipal) override;
  NS_IMETHOD SetTopWindowURIIfUnknown(nsIURI* aTopWindowURI) override;
  NS_IMETHOD GetProxyURI(nsIURI** proxyURI) override;
  virtual void SetCorsPreflightParameters(
      const nsTArray<nsCString>& unsafeHeaders,
      bool aShouldStripRequestBodyHeader) override;
  virtual void SetAltDataForChild(bool aIsForChild) override;
  virtual void DisableAltDataCache() override { mDisableAltDataCache = true; };

  NS_IMETHOD GetConnectionInfoHashKey(
      nsACString& aConnectionInfoHashKey) override;
  NS_IMETHOD GetIntegrityMetadata(nsAString& aIntegrityMetadata) override;
  NS_IMETHOD SetIntegrityMetadata(const nsAString& aIntegrityMetadata) override;
  NS_IMETHOD GetLastRedirectFlags(uint32_t* aValue) override;
  NS_IMETHOD SetLastRedirectFlags(uint32_t aValue) override;
  NS_IMETHOD GetNavigationStartTimeStamp(TimeStamp* aTimeStamp) override;
  NS_IMETHOD SetNavigationStartTimeStamp(TimeStamp aTimeStamp) override;
  NS_IMETHOD CancelByURLClassifier(nsresult aErrorCode) override;
  virtual void SetIPv4Disabled(void) override;
  virtual void SetIPv6Disabled(void) override;
  NS_IMETHOD GetCrossOriginOpenerPolicy(
      nsILoadInfo::CrossOriginOpenerPolicy* aPolicy) override;

  inline void CleanRedirectCacheChainIfNecessary() {
    mRedirectedCachekeys = nullptr;
  }
  NS_IMETHOD HTTPUpgrade(const nsACString& aProtocolName,
                         nsIHttpUpgradeListener* aListener) override;

  // nsISupportsPriority
  NS_IMETHOD GetPriority(int32_t* value) override;
  NS_IMETHOD AdjustPriority(int32_t delta) override;

  // nsIClassOfService
  NS_IMETHOD GetClassFlags(uint32_t* outFlags) override {
    *outFlags = mClassOfService;
    return NS_OK;
  }

  // nsIResumableChannel
  NS_IMETHOD GetEntityID(nsACString& aEntityID) override;

  // nsIConsoleReportCollector
  void AddConsoleReport(uint32_t aErrorFlags, const nsACString& aCategory,
                        nsContentUtils::PropertiesFile aPropertiesFile,
                        const nsACString& aSourceFileURI, uint32_t aLineNumber,
                        uint32_t aColumnNumber, const nsACString& aMessageName,
                        const nsTArray<nsString>& aStringParams) override;

  void FlushReportsToConsole(
      uint64_t aInnerWindowID,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushReportsToConsoleForServiceWorkerScope(
      const nsACString& aScope,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushConsoleReports(
      dom::Document* aDocument,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushConsoleReports(
      nsILoadGroup* aLoadGroup,
      ReportAction aAction = ReportAction::Forget) override;

  void FlushConsoleReports(nsIConsoleReportCollector* aCollector) override;

  void ClearConsoleReports() override;

  class nsContentEncodings : public nsStringEnumeratorBase {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIUTF8STRINGENUMERATOR

    using nsStringEnumeratorBase::GetNext;

    nsContentEncodings(nsIHttpChannel* aChannel, const char* aEncodingHeader);

   private:
    virtual ~nsContentEncodings() = default;

    MOZ_MUST_USE nsresult PrepareForNext(void);

    // We do not own the buffer.  The channel owns it.
    const char* mEncodingHeader;
    const char* mCurStart;  // points to start of current header
    const char* mCurEnd;    // points to end of current header

    // Hold a ref to our channel so that it can't go away and take the
    // header with it.
    nsCOMPtr<nsIHttpChannel> mChannel;

    bool mReady;
  };

  nsHttpResponseHead* GetResponseHead() const { return mResponseHead.get(); }
  nsHttpRequestHead* GetRequestHead() { return &mRequestHead; }
  nsHttpHeaderArray* GetResponseTrailers() const {
    return mResponseTrailers.get();
  }

  const NetAddr& GetSelfAddr() { return mSelfAddr; }
  const NetAddr& GetPeerAddr() { return mPeerAddr; }

  MOZ_MUST_USE nsresult OverrideSecurityInfo(nsISupports* aSecurityInfo);

 public: /* Necko internal use only... */
  int64_t GetAltDataLength() { return mAltDataLength; }
  bool IsNavigation();

  bool IsDeliveringAltData() const { return mDeliveringAltData; }

  static void PropagateReferenceIfNeeded(nsIURI* aURI,
                                         nsCOMPtr<nsIURI>& aRedirectURI);

  // Return whether upon a redirect code of httpStatus for method, the
  // request method should be rewritten to GET.
  static bool ShouldRewriteRedirectToGET(
      uint32_t httpStatus, nsHttpRequestHead::ParsedMethodType method);

  // Like nsIEncodedChannel::DoApplyConversions except context is set to
  // mListenerContext.
  MOZ_MUST_USE nsresult DoApplyContentConversions(
      nsIStreamListener* aNextListener, nsIStreamListener** aNewNextListener);

  // Callback on STS thread called by CopyComplete when NS_AsyncCopy()
  // is finished. This function works as a proxy function to dispatch
  // |EnsureUploadStreamIsCloneableComplete| to main thread.
  virtual void OnCopyComplete(nsresult aStatus);

  void AddClassificationFlags(uint32_t aFlags, bool aIsThirdParty);

  void SetFlashPluginState(nsIHttpChannel::FlashPluginState aState);

  const uint64_t& ChannelId() const { return mChannelId; }

  void InternalSetUploadStream(nsIInputStream* uploadStream) {
    mUploadStream = uploadStream;
  }

  void InternalSetUploadStreamLength(uint64_t aLength) {
    mReqContentLength = aLength;
  }

  void SetUploadStreamHasHeaders(bool hasHeaders) {
    mUploadStreamHasHeaders = hasHeaders;
  }

  virtual nsresult SetReferrerHeader(const nsACString& aReferrer,
                                     bool aRespectBeforeConnect = true) {
    if (aRespectBeforeConnect) {
      ENSURE_CALLED_BEFORE_CONNECT();
    }
    return mRequestHead.SetHeader(nsHttp::Referer, aReferrer);
  }

  nsresult ClearReferrerHeader() {
    ENSURE_CALLED_BEFORE_CONNECT();
    return mRequestHead.ClearHeader(nsHttp::Referer);
  }

  void SetTopWindowURI(nsIURI* aTopWindowURI) { mTopWindowURI = aTopWindowURI; }

  void SetContentBlockingAllowListPrincipal(nsIPrincipal* aPrincipal) {
    mContentBlockingAllowListPrincipal = aPrincipal;
  }

  // Set referrerInfo and compute the referrer header if neccessary.
  nsresult SetReferrerInfoInternal(nsIReferrerInfo* aReferrerInfo, bool aClone,
                                   bool aCompute, bool aRespectBeforeConnect);

 protected:
  nsresult GetTopWindowURI(nsIURI* aURIBeingLoaded, nsIURI** aTopWindowURI);

  // Handle notifying listener, removing from loadgroup if request failed.
  void DoNotifyListener();
  virtual void DoNotifyListenerCleanup() = 0;

  // drop reference to listener, its callbacks, and the progress sink
  virtual void ReleaseListeners();

  // Call AsyncAbort().
  virtual void DoAsyncAbort(nsresult aStatus) = 0;

  // This is fired only when a cookie is created due to the presence of
  // Set-Cookie header in the response header of any network request.
  // This notification will come only after the "http-on-examine-response"
  // was fired.
  void NotifySetCookie(const nsACString& aCookie);

  mozilla::dom::PerformanceStorage* GetPerformanceStorage();
  void MaybeReportTimingData();
  nsIURI* GetReferringPage();
  nsPIDOMWindowInner* GetInnerDOMWindow();

  void AddCookiesToRequest();
  virtual MOZ_MUST_USE nsresult SetupReplacementChannel(nsIURI*, nsIChannel*,
                                                        bool preserveMethod,
                                                        uint32_t redirectFlags);

  // bundle calling OMR observers and marking flag into one function
  inline void CallOnModifyRequestObservers() {
    gHttpHandler->OnModifyRequest(this);
    MOZ_ASSERT(!mRequestObserversCalled);
    mRequestObserversCalled = true;
  }

  // Helper function to simplify getting notification callbacks.
  template <class T>
  void GetCallback(nsCOMPtr<T>& aResult) {
    NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                  NS_GET_TEMPLATE_IID(T),
                                  getter_AddRefs(aResult));
  }

  // Redirect tracking
  // Checks whether or not aURI and mOriginalURI share the same domain.
  bool SameOriginWithOriginalUri(nsIURI* aURI);

  // GetPrincipal Returns the channel's URI principal.
  nsIPrincipal* GetURIPrincipal();

  MOZ_MUST_USE bool BypassServiceWorker() const;

  // Returns true if this channel should intercept the network request and
  // prepare for a possible synthesized response instead.
  bool ShouldIntercept(nsIURI* aURI = nullptr);

  // Callback on main thread when NS_AsyncCopy() is finished populating
  // the new mUploadStream.
  void EnsureUploadStreamIsCloneableComplete(nsresult aStatus);

#ifdef DEBUG
  // Check if mPrivateBrowsingId matches between LoadInfo and LoadContext.
  void AssertPrivateBrowsingId();
#endif

  // Called before we create the redirect target channel.
  already_AddRefed<nsILoadInfo> CloneLoadInfoForRedirect(
      nsIURI* newURI, uint32_t redirectFlags);

  static void CallTypeSniffers(void* aClosure, const uint8_t* aData,
                               uint32_t aCount);

  nsresult CheckRedirectLimit(uint32_t aRedirectFlags) const;

  bool MaybeWaitForUploadStreamLength(nsIStreamListener* aListener,
                                      nsISupports* aContext);

  bool IsBrowsingContextDiscarded() const;

  friend class PrivateBrowsingChannel<HttpBaseChannel>;
  friend class InterceptFailedOnStop;

 protected:
  // this section is for main-thread-only object
  // all the references need to be proxy released on main thread.
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsIProgressEventSink> mProgressSink;
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;
  nsCOMPtr<nsIApplicationCache> mApplicationCache;
  nsCOMPtr<nsIURI> mAPIRedirectToURI;
  nsCOMPtr<nsIURI> mProxyURI;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIURI> mTopWindowURI;
  nsCOMPtr<nsIPrincipal> mContentBlockingAllowListPrincipal;
  nsCOMPtr<nsIStreamListener> mListener;
  // An instance of nsHTTPCompressConv
  nsCOMPtr<nsIStreamListener> mCompressListener;

 private:
  // Proxy release all members above on main thread.
  void ReleaseMainThreadOnlyReferences();

  void ExplicitSetUploadStreamLength(uint64_t aContentLength,
                                     bool aStreamHasHeaders);

  void MaybeResumeAsyncOpen();

 protected:
  nsCString mSpec;  // ASCII encoded URL spec
  nsCString mContentTypeHint;
  nsCString mContentCharsetHint;
  nsCString mUserSetCookieHeader;
  // HTTP Upgrade Data
  nsCString mUpgradeProtocol;
  // Resumable channel specific data
  nsCString mEntityID;
  // The initiator type (for this resource) - how was the resource referenced in
  // the HTML file.
  nsString mInitiatorType;
  // Holds the name of the preferred alt-data type for each contentType.
  nsTArray<PreferredAlternativeDataTypeParams> mPreferredCachedAltDataTypes;
  // Holds the name of the alternative data type the channel returned.
  nsCString mAvailableCachedAltDataType;
  nsString mIntegrityMetadata;

  // Classified channel's matched information
  nsCString mMatchedList;
  nsCString mMatchedProvider;
  nsCString mMatchedFullHash;

  nsTArray<nsCString> mMatchedTrackingLists;
  nsTArray<nsCString> mMatchedTrackingFullHashes;

  nsCOMPtr<nsISupports> mOwner;

  nsHttpRequestHead mRequestHead;
  // Upload throttling.
  nsCOMPtr<nsIInputChannelThrottleQueue> mThrottleQueue;
  nsCOMPtr<nsIInputStream> mUploadStream;
  nsCOMPtr<nsIRunnable> mUploadCloneableCallback;
  UniquePtr<nsHttpResponseHead> mResponseHead;
  UniquePtr<nsHttpHeaderArray> mResponseTrailers;
  RefPtr<nsHttpConnectionInfo> mConnectionInfo;
  nsCOMPtr<nsIProxyInfo> mProxyInfo;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsCOMPtr<nsIHttpUpgradeListener> mUpgradeProtocolCallback;
  UniquePtr<nsString> mContentDispositionFilename;
  nsCOMPtr<nsIConsoleReportCollector> mReportCollector;

  RefPtr<nsHttpHandler> mHttpHandler;  // keep gHttpHandler alive
  UniquePtr<nsTArray<nsCString>> mRedirectedCachekeys;
  nsCOMPtr<nsIRequestContext> mRequestContext;

  NetAddr mSelfAddr;
  NetAddr mPeerAddr;

  nsTArray<std::pair<nsString, nsString>> mSecurityConsoleMessages;
  nsTArray<nsCString> mUnsafeHeaders;

  // A time value equal to the starting time of the fetch that initiates the
  // redirect.
  mozilla::TimeStamp mRedirectStartTimeStamp;
  // A time value equal to the time immediately after receiving the last byte of
  // the response of the last redirect.
  mozilla::TimeStamp mRedirectEndTimeStamp;

  PRTime mChannelCreationTime;
  TimeStamp mChannelCreationTimestamp;
  TimeStamp mAsyncOpenTime;
  TimeStamp mCacheReadStart;
  TimeStamp mCacheReadEnd;
  TimeStamp mLaunchServiceWorkerStart;
  TimeStamp mLaunchServiceWorkerEnd;
  TimeStamp mDispatchFetchEventStart;
  TimeStamp mDispatchFetchEventEnd;
  TimeStamp mHandleFetchEventStart;
  TimeStamp mHandleFetchEventEnd;
  // copied from the transaction before we null out mTransaction
  // so that the timing can still be queried from OnStopRequest
  TimingStruct mTransactionTimings;

  uint64_t mStartPos;
  uint64_t mTransferSize;
  uint64_t mDecodedBodySize;
  uint64_t mEncodedBodySize;
  uint64_t mRequestContextID;
  // ID of the top-level document's inner window this channel is being
  // originated from.
  uint64_t mContentWindowId;
  uint64_t mTopLevelOuterContentWindowId;
  int64_t mAltDataLength;
  uint64_t mChannelId;
  uint64_t mReqContentLength;

  Atomic<nsresult, ReleaseAcquire> mStatus;

  // Use Release-Acquire ordering to ensure the OMT ODA is ignored while channel
  // is canceled on main thread.
  Atomic<bool, ReleaseAcquire> mCanceled;
  Atomic<uint32_t, ReleaseAcquire> mFirstPartyClassificationFlags;
  Atomic<uint32_t, ReleaseAcquire> mThirdPartyClassificationFlags;
  Atomic<uint32_t, ReleaseAcquire> mFlashPluginState;

  uint32_t mLoadFlags;
  uint32_t mCaps;
  uint32_t mClassOfService;

  uint32_t mUpgradeToSecure : 1;
  uint32_t mApplyConversion : 1;
  uint32_t mIsPending : 1;
  uint32_t mWasOpened : 1;
  // if 1 all "http-on-{opening|modify|etc}-request" observers have been called
  uint32_t mRequestObserversCalled : 1;
  uint32_t mResponseHeadersModified : 1;
  uint32_t mAllowSTS : 1;
  uint32_t mThirdPartyFlags : 3;
  uint32_t mUploadStreamHasHeaders : 1;
  uint32_t mInheritApplicationCache : 1;
  uint32_t mChooseApplicationCache : 1;
  uint32_t mLoadedFromApplicationCache : 1;
  uint32_t mChannelIsForDownload : 1;
  uint32_t mTracingEnabled : 1;
  // True if timing collection is enabled
  uint32_t mTimingEnabled : 1;
  uint32_t mReportTiming : 1;
  uint32_t mAllowSpdy : 1;
  uint32_t mAllowAltSvc : 1;
  // !!! This is also used by the URL classifier to exempt channels from
  // classification. If this is changed or removed, make sure we also update
  // NS_ShouldClassifyChannel accordingly !!!
  uint32_t mBeConservative : 1;
  // If the current channel is used to as a TRR connection.
  uint32_t mIsTRRServiceChannel : 1;
  // If the request was performed to a TRR resolved IP address.
  // Will be false if loading the resource does not create a connection
  // (for example when it's loaded from the cache).
  uint32_t mResolvedByTRR : 1;
  uint32_t mResponseTimeoutEnabled : 1;
  // A flag that should be false only if a cross-domain redirect occurred
  uint32_t mAllRedirectsSameOrigin : 1;

  // Is 1 if no redirects have occured or if all redirects
  // pass the Resource Timing timing-allow-check
  uint32_t mAllRedirectsPassTimingAllowCheck : 1;

  // True if this channel was intercepted and could receive a synthesized
  // response.
  uint32_t mResponseCouldBeSynthesized : 1;

  uint32_t mBlockAuthPrompt : 1;

  // If true, we behave as if the LOAD_FROM_CACHE flag has been set.
  // Used to enforce that flag's behavior but not expose it externally.
  uint32_t mAllowStaleCacheContent : 1;

  // If true, we prefer the LOAD_FROM_CACHE flag over LOAD_BYPASS_CACHE or
  // LOAD_BYPASS_LOCAL_CACHE.
  uint32_t mPreferCacheLoadOverBypass : 1;

  // True iff this request has been calculated in its request context as
  // a non tail request.  We must remove it again when this channel is done.
  uint32_t mAddedAsNonTailRequest : 1;

  // True if AsyncOpen() is called when the stream length is still unknown.
  // AsyncOpen() will be retriggered when InputStreamLengthHelper execs the
  // callback, passing the stream length value.
  uint32_t mAsyncOpenWaitingForStreamLength : 1;

  // Defaults to true.  This is set to false when it is no longer possible
  // to upgrade the request to a secure channel.
  uint32_t mUpgradableToSecure : 1;

  // An opaque flags for non-standard behavior of the TLS system.
  // It is unlikely this will need to be set outside of telemetry studies
  // relating to the TLS implementation.
  uint32_t mTlsFlags;

  // Current suspension depth for this channel object
  uint32_t mSuspendCount;

  // Per channel transport window override (0 means no override)
  uint32_t mInitialRwin;

  uint32_t mProxyResolveFlags;

  uint32_t mContentDispositionHint;

  uint32_t mCorsMode;
  uint32_t mRedirectMode;

  // If this channel was created as the result of a redirect, then this value
  // will reflect the redirect flags passed to the SetupReplacementChannel()
  // method.
  uint32_t mLastRedirectFlags;

  int16_t mPriority;
  uint8_t mRedirectionLimit;

  // Performance tracking
  // Number of redirects that has occurred.
  int8_t mRedirectCount;
  // Number of internal redirects that has occurred.
  int8_t mInternalRedirectCount;

  bool mAsyncOpenTimeOverriden;
  bool mForcePending;

  // true if the channel is deliving alt-data.
  bool mDeliveringAltData;

  bool mCorsIncludeCredentials;

  // These parameters are used to ensure that we do not call OnStartRequest and
  // OnStopRequest more than once.
  bool mOnStartRequestCalled;
  bool mOnStopRequestCalled;

  // Defaults to false. Is set to true at the begining of OnStartRequest.
  // Used to ensure methods can't be called before OnStartRequest.
  bool mAfterOnStartRequestBegun;

  bool mRequireCORSPreflight;

  // This flag will be true if the consumer is requesting alt-data AND the
  // consumer is in the child process.
  bool mAltDataForChild;

  // This flag will be true if the consumer cannot process alt-data.  This
  // is used in the webextension StreamFilter handler.  If true, we bypass
  // using alt-data for the request.
  bool mDisableAltDataCache;

  bool mForceMainDocumentChannel;
  // This is set true if the channel is waiting for the
  // InputStreamLengthHelper::GetAsyncLength callback.
  bool mPendingInputStreamLengthOperation;

  bool EnsureRequestContextID();
  bool EnsureRequestContext();

  // Adds/removes this channel as a non-tailed request in its request context
  // these helpers ensure we add it only once and remove it only when added
  // via mAddedAsNonTailRequest member tracking.
  void AddAsNonTailRequest();
  void RemoveAsNonTailRequest();

  void EnsureTopLevelOuterContentWindowId();
};

NS_DEFINE_STATIC_IID_ACCESSOR(HttpBaseChannel, HTTP_BASE_CHANNEL_IID)

// Share some code while working around C++'s absurd inability to handle casting
// of member functions between base/derived types.
// - We want to store member function pointer to call at resume time, but one
//   such function--HandleAsyncAbort--we want to share between the
//   nsHttpChannel/HttpChannelChild.  Can't define it in base class, because
//   then we'd have to cast member function ptr between base/derived class
//   types.  Sigh...
template <class T>
class HttpAsyncAborter {
 public:
  explicit HttpAsyncAborter(T* derived)
      : mThis(derived), mCallOnResume(nullptr) {}

  // Aborts channel: calls OnStart/Stop with provided status, removes channel
  // from loadGroup.
  MOZ_MUST_USE nsresult AsyncAbort(nsresult status);

  // Does most the actual work.
  void HandleAsyncAbort();

  // AsyncCall calls a member function asynchronously (via an event).
  // retval isn't refcounted and is set only when event was successfully
  // posted, the event is returned for the purpose of cancelling when needed
  MOZ_MUST_USE virtual nsresult AsyncCall(
      void (T::*funcPtr)(), nsRunnableMethod<T>** retval = nullptr);

 private:
  T* mThis;

 protected:
  // Function to be called at resume time
  std::function<nsresult(T*)> mCallOnResume;
};

template <class T>
MOZ_MUST_USE nsresult HttpAsyncAborter<T>::AsyncAbort(nsresult status) {
  MOZ_LOG(gHttpLog, LogLevel::Debug,
          ("HttpAsyncAborter::AsyncAbort [this=%p status=%" PRIx32 "]\n", mThis,
           static_cast<uint32_t>(status)));

  mThis->mStatus = status;

  // if this fails?  Callers ignore our return value anyway....
  return AsyncCall(&T::HandleAsyncAbort);
}

// Each subclass needs to define its own version of this (which just calls this
// base version), else we wind up casting base/derived member function ptrs
template <class T>
inline void HttpAsyncAborter<T>::HandleAsyncAbort() {
  MOZ_ASSERT(!mCallOnResume, "How did that happen?");

  if (mThis->mSuspendCount) {
    MOZ_LOG(
        gHttpLog, LogLevel::Debug,
        ("Waiting until resume to do async notification [this=%p]\n", mThis));
    mCallOnResume = [](T* self) {
      self->HandleAsyncAbort();
      return NS_OK;
    };
    return;
  }

  mThis->DoNotifyListener();

  // finally remove ourselves from the load group.
  if (mThis->mLoadGroup)
    mThis->mLoadGroup->RemoveRequest(mThis, nullptr, mThis->mStatus);
}

template <class T>
nsresult HttpAsyncAborter<T>::AsyncCall(void (T::*funcPtr)(),
                                        nsRunnableMethod<T>** retval) {
  nsresult rv;

  RefPtr<nsRunnableMethod<T>> event =
      NewRunnableMethod("net::HttpAsyncAborter::AsyncCall", mThis, funcPtr);
  rv = NS_DispatchToCurrentThread(event);
  if (NS_SUCCEEDED(rv) && retval) {
    *retval = event;
  }

  return rv;
}

class ProxyReleaseRunnable final : public mozilla::Runnable {
 public:
  explicit ProxyReleaseRunnable(nsTArray<nsCOMPtr<nsISupports>>&& aDoomed)
      : Runnable("ProxyReleaseRunnable"), mDoomed(std::move(aDoomed)) {}

  NS_IMETHOD
  Run() override {
    mDoomed.Clear();
    return NS_OK;
  }

 private:
  virtual ~ProxyReleaseRunnable() = default;

  nsTArray<nsCOMPtr<nsISupports>> mDoomed;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_HttpBaseChannel_h
