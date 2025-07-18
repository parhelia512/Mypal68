/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"

#include "nsIFile.h"
#include <algorithm>

#include "nsISearchService.h"
#include "nsIURIFixup.h"
#include "nsIURIMutator.h"
#include "nsIWebNavigation.h"
#include "nsDefaultURIFixup.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/Unused.h"
#include "nsXULAppAPI.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_keyword.h"

// Used to check if external protocol schemes are usable
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

using namespace mozilla;

/* Implementation file */
NS_IMPL_ISUPPORTS(nsDefaultURIFixup, nsIURIFixup)

nsDefaultURIFixup::nsDefaultURIFixup() {}

nsDefaultURIFixup::~nsDefaultURIFixup() {}

NS_IMETHODIMP
nsDefaultURIFixup::CreateFixupURI(const nsACString& aStringURI,
                                  uint32_t aFixupFlags,
                                  nsIInputStream** aPostData, nsIURI** aURI) {
  nsCOMPtr<nsIURIFixupInfo> fixupInfo;
  nsresult rv = GetFixupURIInfo(aStringURI, aFixupFlags, aPostData,
                                getter_AddRefs(fixupInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  fixupInfo->GetPreferredURI(aURI);
  return rv;
}

// Returns true if the URL contains a user:password@ or user@
static bool HasUserPassword(const nsACString& aStringURI) {
  mozilla::Tokenizer parser(aStringURI);
  mozilla::Tokenizer::Token token;

  // May start with any of "protocol:", "protocol://",  "//", "://"
  if (parser.Check(Tokenizer::TOKEN_WORD, token)) {  // Skip protocol if any
  }
  if (parser.CheckChar(':')) {  // Skip colon if found
  }
  while (parser.CheckChar('/')) {  // Skip all of the following slashes
  }

  while (parser.Next(token)) {
    if (token.Type() == Tokenizer::TOKEN_CHAR) {
      if (token.AsChar() == '/') {
        return false;
      }
      if (token.AsChar() == '@') {
        return true;
      }
    }
  }

  return false;
}

// Assume that 1 tab is accidental, but more than 1 implies this is
// supposed to be tab-separated content.
static bool MaybeTabSeparatedContent(const nsCString& aStringURI) {
  auto firstTab = aStringURI.FindChar('\t');
  return firstTab != kNotFound && aStringURI.RFindChar('\t') != firstTab;
}

NS_IMETHODIMP
nsDefaultURIFixup::GetFixupURIInfo(const nsACString& aStringURI,
                                   uint32_t aFixupFlags,
                                   nsIInputStream** aPostData,
                                   nsIURIFixupInfo** aInfo) {
  NS_ENSURE_ARG(!aStringURI.IsEmpty());

  nsresult rv;

  nsAutoCString uriString(aStringURI);

  // Eliminate embedded newlines, which single-line text fields now allow:
  uriString.StripCRLF();
  // Cleanup the empty spaces and tabs that might be on each end:
  uriString.Trim(" \t");

  NS_ENSURE_TRUE(!uriString.IsEmpty(), NS_ERROR_FAILURE);

  RefPtr<nsDefaultURIFixupInfo> info = new nsDefaultURIFixupInfo(uriString);
  NS_ADDREF(*aInfo = info);

  nsCOMPtr<nsIIOService> ioService =
      do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoCString scheme;
  ioService->ExtractScheme(aStringURI, scheme);

  // View-source is a pseudo scheme. We're interested in fixing up the stuff
  // after it. The easiest way to do that is to call this method again with the
  // "view-source:" lopped off and then prepend it again afterwards.

  if (scheme.EqualsLiteral("view-source")) {
    nsCOMPtr<nsIURIFixupInfo> uriInfo;
    // We disable keyword lookup and alternate URIs so that small typos don't
    // cause us to look at very different domains
    uint32_t newFixupFlags = aFixupFlags & ~FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP &
                             ~FIXUP_FLAGS_MAKE_ALTERNATE_URI;

    const uint32_t viewSourceLen = sizeof("view-source:") - 1;
    nsAutoCString innerURIString(Substring(uriString, viewSourceLen,
                                           uriString.Length() - viewSourceLen));
    // Prevent recursion:
    innerURIString.Trim(" ");
    nsAutoCString innerScheme;
    ioService->ExtractScheme(innerURIString, innerScheme);
    if (innerScheme.EqualsLiteral("view-source")) {
      return NS_ERROR_FAILURE;
    }

    rv = GetFixupURIInfo(innerURIString, newFixupFlags, aPostData,
                         getter_AddRefs(uriInfo));
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
    nsAutoCString spec;
    nsCOMPtr<nsIURI> uri;
    uriInfo->GetPreferredURI(getter_AddRefs(uri));
    if (!uri) {
      return NS_ERROR_FAILURE;
    }
    uri->GetSpec(spec);
    uriString.AssignLiteral("view-source:");
    uriString.Append(spec);
  } else {
    // Check for if it is a file URL
    nsCOMPtr<nsIURI> uri;
    FileURIFixup(uriString, getter_AddRefs(uri));
    // NB: FileURIFixup only returns a URI if it had to fix the protocol to
    // do so, so passing in file:///foo/bar will not hit this path:
    if (uri) {
      uri.swap(info->mFixedURI);
      info->mPreferredURI = info->mFixedURI;
      info->mFixupChangedProtocol = true;
      return NS_OK;
    }
  }

  // Fix up common scheme typos.
  if (StaticPrefs::browser_fixup_typo_scheme() &&
      (aFixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS)) {
    // Fast-path for common cases.
    if (scheme.IsEmpty() || scheme.EqualsLiteral("http") ||
        scheme.EqualsLiteral("https") || scheme.EqualsLiteral("ftp") ||
        scheme.EqualsLiteral("file")) {
      // Do nothing.
    } else if (scheme.EqualsLiteral("ttp")) {
      // ttp -> http.
      uriString.ReplaceLiteral(0, 3, "http");
      scheme.AssignLiteral("http");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("htp")) {
      // htp -> http.
      uriString.ReplaceLiteral(0, 3, "http");
      scheme.AssignLiteral("http");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("ttps")) {
      // ttps -> https.
      uriString.ReplaceLiteral(0, 4, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("tps")) {
      // tps -> https.
      uriString.ReplaceLiteral(0, 3, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("ps")) {
      // ps -> https.
      uriString.ReplaceLiteral(0, 2, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("htps")) {
      // htps -> https.
      uriString.ReplaceLiteral(0, 4, "https");
      scheme.AssignLiteral("https");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("ile")) {
      // ile -> file.
      uriString.ReplaceLiteral(0, 3, "file");
      scheme.AssignLiteral("file");
      info->mFixupChangedProtocol = true;
    } else if (scheme.EqualsLiteral("le")) {
      // le -> file.
      uriString.ReplaceLiteral(0, 2, "file");
      scheme.AssignLiteral("file");
      info->mFixupChangedProtocol = true;
    }
  }

  // Now we need to check whether "scheme" is something we don't
  // really know about.
  nsCOMPtr<nsIProtocolHandler> ourHandler, extHandler;

  extHandler = do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "default");
  if (!scheme.IsEmpty()) {
    ioService->GetProtocolHandler(scheme.get(), getter_AddRefs(ourHandler));
  } else {
    ourHandler = extHandler;
  }

  if (ourHandler != extHandler || !PossiblyHostPortUrl(uriString)) {
    // Just try to create an URL out of it
    rv = NS_NewURI(getter_AddRefs(info->mFixedURI), uriString);

    if (!info->mFixedURI && rv != NS_ERROR_MALFORMED_URI) {
      return rv;
    }
  }

  if (info->mFixedURI && ourHandler == extHandler &&
      StaticPrefs::keyword_enabled() &&
      (aFixupFlags & FIXUP_FLAG_FIX_SCHEME_TYPOS)) {
    nsCOMPtr<nsIExternalProtocolService> extProtService =
        do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
    if (extProtService) {
      bool handlerExists = false;
      rv = extProtService->ExternalProtocolHandlerExists(scheme.get(),
                                                         &handlerExists);
      if (NS_FAILED(rv)) {
        return rv;
      }
      // This basically means we're dealing with a theoretically valid
      // URI... but we have no idea how to load it. (e.g. "christmas:humbug")
      // It's more likely the user wants to search, and so we
      // chuck this over to their preferred search provider instead:
      if (!handlerExists) {
        bool hasUserPassword = HasUserPassword(uriString);
        if (!hasUserPassword) {
          TryKeywordFixupForURIInfo(uriString, info, aPostData);
        } else {
          // If the given URL has a user:password we can't just pass it to the
          // external protocol handler; we'll try using it with http instead
          // later
          info->mFixedURI = nullptr;
        }
      }
    }
  }

  if (info->mFixedURI) {
    if (!info->mPreferredURI) {
      if (aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
        info->mFixupCreatedAlternateURI = MakeAlternateURI(info->mFixedURI);
      }
      info->mPreferredURI = info->mFixedURI;
    }
    return NS_OK;
  }

  // Fix up protocol string before calling KeywordURIFixup, because
  // it cares about the hostname of such URIs:
  nsCOMPtr<nsIURI> uriWithProtocol;
  bool inputHadDuffProtocol = false;

  // Prune duff protocol schemes
  //
  //   ://totallybroken.url.com
  //   //shorthand.url.com
  //
  if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("://"))) {
    uriString = StringTail(uriString, uriString.Length() - 3);
    inputHadDuffProtocol = true;
  } else if (StringBeginsWith(uriString, NS_LITERAL_CSTRING("//"))) {
    uriString = StringTail(uriString, uriString.Length() - 2);
    inputHadDuffProtocol = true;
  }

  // Note: this rv gets returned at the end of this method if we don't fix up
  // the protocol and don't do a keyword fixup after this (because the pref
  // or the flags passed might not let us).
  rv = NS_OK;
  // Avoid fixing up content that looks like tab-separated values
  if (!MaybeTabSeparatedContent(uriString)) {
    rv = FixupURIProtocol(uriString, info, getter_AddRefs(uriWithProtocol));
    if (uriWithProtocol) {
      info->mFixedURI = uriWithProtocol;
    }
  }

  // See if it is a keyword
  // Test whether keywords need to be fixed up
  if (StaticPrefs::keyword_enabled() &&
      (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP) &&
      !inputHadDuffProtocol) {
    if (NS_SUCCEEDED(KeywordURIFixup(uriString, info, aPostData)) &&
        info->mPreferredURI) {
      return NS_OK;
    }
  }

  // Did the caller want us to try an alternative URI?
  // If so, attempt to fixup http://foo into http://www.foo.com

  if (info->mFixedURI && aFixupFlags & FIXUP_FLAGS_MAKE_ALTERNATE_URI) {
    info->mFixupCreatedAlternateURI = MakeAlternateURI(info->mFixedURI);
  }

  if (info->mFixedURI) {
    info->mPreferredURI = info->mFixedURI;
    return NS_OK;
  }

  // If we still haven't been able to construct a valid URI, try to force a
  // keyword match.  This catches search strings with '.' or ':' in them.
  if (StaticPrefs::keyword_enabled() &&
      (aFixupFlags & FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP)) {
    rv = TryKeywordFixupForURIInfo(aStringURI, info, aPostData);
  }

  return rv;
}

NS_IMETHODIMP
nsDefaultURIFixup::WebNavigationFlagsToFixupFlags(const nsACString& aStringURI,
                                                  uint32_t aDocShellFlags,
                                                  uint32_t* aFixupFlags) {
  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), aStringURI);
  if (uri) {
    aDocShellFlags &= ~nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
  }

  *aFixupFlags = 0;
  if (aDocShellFlags & nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP) {
    *aFixupFlags |= FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
  }
  if (aDocShellFlags & nsIWebNavigation::LOAD_FLAGS_FIXUP_SCHEME_TYPOS) {
    *aFixupFlags |= FIXUP_FLAG_FIX_SCHEME_TYPOS;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixup::KeywordToURI(const nsACString& aKeyword,
                                nsIInputStream** aPostData,
                                nsIURIFixupInfo** aInfo) {
  RefPtr<nsDefaultURIFixupInfo> info = new nsDefaultURIFixupInfo(aKeyword);
  NS_ADDREF(*aInfo = info);

  if (aPostData) {
    *aPostData = nullptr;
  }
  NS_ENSURE_STATE(Preferences::GetRootBranch());

  // Strip leading "?" and leading/trailing spaces from aKeyword
  nsAutoCString keyword(aKeyword);
  if (StringBeginsWith(keyword, NS_LITERAL_CSTRING("?"))) {
    keyword.Cut(0, 1);
  }
  keyword.Trim(" ");

  if (XRE_IsContentProcess()) {
    dom::ContentChild* contentChild = dom::ContentChild::GetSingleton();
    if (!contentChild) {
      return NS_ERROR_NOT_AVAILABLE;
    }

    RefPtr<nsIInputStream> postData;
    RefPtr<nsIURI> uri;
    nsAutoString providerName;
    if (!contentChild->SendKeywordToURI(keyword, &providerName, &postData,
                                        &uri)) {
      return NS_ERROR_FAILURE;
    }

    CopyUTF8toUTF16(keyword, info->mKeywordAsSent);
    info->mKeywordProviderName = providerName;

    if (aPostData) {
      postData.forget(aPostData);
    }

    info->mPreferredURI = uri.forget();
    return NS_OK;
  }

  // Try falling back to the search service's default search engine
  nsCOMPtr<nsISearchService> searchSvc =
      do_GetService("@mozilla.org/browser/search-service;1");
  if (searchSvc) {
    nsCOMPtr<nsISearchEngine> defaultEngine;
    searchSvc->GetDefaultEngine(getter_AddRefs(defaultEngine));
    if (defaultEngine) {
      nsCOMPtr<nsISearchSubmission> submission;
      nsAutoString responseType;
      // We allow default search plugins to specify alternate
      // parameters that are specific to keyword searches.
      NS_NAMED_LITERAL_STRING(mozKeywordSearch,
                              "application/x-moz-keywordsearch");
      bool supportsResponseType = false;
      defaultEngine->SupportsResponseType(mozKeywordSearch,
                                          &supportsResponseType);
      if (supportsResponseType) {
        responseType.Assign(mozKeywordSearch);
      }

      NS_ConvertUTF8toUTF16 keywordW(keyword);
      defaultEngine->GetSubmission(keywordW, responseType,
                                   NS_LITERAL_STRING("keyword"),
                                   getter_AddRefs(submission));

      if (submission) {
        nsCOMPtr<nsIInputStream> postData;
        submission->GetPostData(getter_AddRefs(postData));
        if (aPostData) {
          postData.forget(aPostData);
        } else if (postData) {
          // The submission specifies POST data (i.e. the search
          // engine's "method" is POST), but our caller didn't allow
          // passing post data back. No point passing back a URL that
          // won't load properly.
          return NS_ERROR_FAILURE;
        }

        defaultEngine->GetName(info->mKeywordProviderName);
        info->mKeywordAsSent = keywordW;
        return submission->GetUri(getter_AddRefs(info->mPreferredURI));
      }
    }
  }

  // out of options
  return NS_ERROR_NOT_AVAILABLE;
}

// Helper to deal with passing around uri fixup stuff
nsresult nsDefaultURIFixup::TryKeywordFixupForURIInfo(
    const nsACString& aURIString, nsDefaultURIFixupInfo* aFixupInfo,
    nsIInputStream** aPostData) {
  nsCOMPtr<nsIURIFixupInfo> keywordInfo;
  nsresult rv =
      KeywordToURI(aURIString, aPostData, getter_AddRefs(keywordInfo));
  if (NS_SUCCEEDED(rv)) {
    keywordInfo->GetKeywordProviderName(aFixupInfo->mKeywordProviderName);
    keywordInfo->GetKeywordAsSent(aFixupInfo->mKeywordAsSent);
    keywordInfo->GetPreferredURI(getter_AddRefs(aFixupInfo->mPreferredURI));
  }
  return rv;
}

bool nsDefaultURIFixup::MakeAlternateURI(nsCOMPtr<nsIURI>& aURI) {
  if (!Preferences::GetRootBranch()) {
    return false;
  }
  if (!Preferences::GetBool("browser.fixup.alternate.enabled", true)) {
    return false;
  }

  // Code only works for http. Not for any other protocol including https!
  if (!net::SchemeIsHTTP(aURI)) {
    return false;
  }

  // Security - URLs with user / password info should NOT be fixed up
  nsAutoCString userpass;
  aURI->GetUserPass(userpass);
  if (!userpass.IsEmpty()) {
    return false;
  }
  // Don't fix up hosts with ports
  int32_t port;
  aURI->GetPort(&port);
  if (port != -1) {
    return false;
  }

  nsAutoCString oldHost;
  aURI->GetHost(oldHost);

  // Don't fix up 'localhost' because that's confusing:
  if (oldHost.EqualsLiteral("localhost")) {
    return false;
  }

  nsAutoCString newHost;
  // Count the dots
  int32_t numDots = 0;
  nsReadingIterator<char> iter;
  nsReadingIterator<char> iterEnd;
  oldHost.BeginReading(iter);
  oldHost.EndReading(iterEnd);
  while (iter != iterEnd) {
    if (*iter == '.') {
      numDots++;
    }
    ++iter;
  }

  // Get the prefix and suffix to stick onto the new hostname. By default these
  // are www. & .com but they could be any other value, e.g. www. & .org

  nsAutoCString prefix("www.");
  nsAutoCString prefPrefix;
  nsresult rv =
      Preferences::GetCString("browser.fixup.alternate.prefix", prefPrefix);
  if (NS_SUCCEEDED(rv)) {
    prefix.Assign(prefPrefix);
  }

  nsAutoCString suffix(".com");
  nsAutoCString prefSuffix;
  rv = Preferences::GetCString("browser.fixup.alternate.suffix", prefSuffix);
  if (NS_SUCCEEDED(rv)) {
    suffix.Assign(prefSuffix);
  }

  if (numDots == 0) {
    newHost.Assign(prefix);
    newHost.Append(oldHost);
    newHost.Append(suffix);
  } else if (numDots == 1) {
    if (!prefix.IsEmpty() &&
        oldHost.EqualsIgnoreCase(prefix.get(), prefix.Length())) {
      newHost.Assign(oldHost);
      newHost.Append(suffix);
    } else if (!suffix.IsEmpty()) {
      newHost.Assign(prefix);
      newHost.Append(oldHost);
    } else {
      // Do nothing
      return false;
    }
  } else {
    // Do nothing
    return false;
  }

  if (newHost.IsEmpty()) {
    return false;
  }

  // Assign the new host string over the old one
  Unused << NS_MutateURI(aURI).SetHost(newHost).Finalize(aURI);

  return true;
}

nsresult nsDefaultURIFixup::FileURIFixup(const nsACString& aStringURI,
                                         nsIURI** aURI) {
  nsAutoCString uriSpecOut;

  nsresult rv = ConvertFileToStringURI(aStringURI, uriSpecOut);
  if (NS_SUCCEEDED(rv)) {
    // if this is file url, uriSpecOut is already in FS charset
    if (NS_SUCCEEDED(NS_NewURI(aURI, uriSpecOut.get()))) {
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::ConvertFileToStringURI(const nsACString& aIn,
                                                   nsCString& aResult) {
  bool attemptFixup = false;

#if defined(XP_WIN)
  // Check for \ in the url-string or just a drive (PC)
  if (aIn.Contains('\\') ||
      (aIn.Length() == 2 && (aIn.Last() == ':' || aIn.Last() == '|'))) {
    attemptFixup = true;
  }
#elif defined(XP_UNIX)
  // Check if it starts with / (UNIX)
  if (aIn.First() == '/') {
    attemptFixup = true;
  }
#else
  // Do nothing (All others for now)
#endif

  if (attemptFixup) {
    // Test if this is a valid path by trying to create a local file
    // object. The URL of that is returned if successful.

    nsCOMPtr<nsIFile> filePath;
    nsresult rv = NS_NewLocalFile(NS_ConvertUTF8toUTF16(aIn), false,
                                  getter_AddRefs(filePath));

    if (NS_SUCCEEDED(rv)) {
      NS_GetURLSpecFromFile(filePath, aResult);
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsDefaultURIFixup::FixupURIProtocol(const nsACString& aURIString,
                                             nsDefaultURIFixupInfo* aFixupInfo,
                                             nsIURI** aURI) {
  nsAutoCString uriString(aURIString);
  *aURI = nullptr;

  // Add ftp:// or http:// to front of url if it has no spec
  //
  // Should fix:
  //
  //   no-scheme.com
  //   ftp.no-scheme.com
  //   ftp4.no-scheme.com
  //   no-scheme.com/query?foo=http://www.foo.com
  //   user:pass@no-scheme.com
  //
  int32_t schemeDelim = uriString.Find("://");
  int32_t firstDelim = uriString.FindCharInSet("/:");
  if (schemeDelim <= 0 || (firstDelim != -1 && schemeDelim > firstDelim)) {
    // find host name
    int32_t hostPos = uriString.FindCharInSet("/:?#");
    if (hostPos == -1) {
      hostPos = uriString.Length();
    }

    // extract host name
    nsAutoCString hostSpec;
    uriString.Left(hostSpec, hostPos);

    // insert url spec corresponding to host name
    uriString.InsertLiteral("http://", 0);
    aFixupInfo->mFixupChangedProtocol = true;
  }  // end if checkprotocol

  return NS_NewURI(aURI, uriString);
}

bool nsDefaultURIFixup::PossiblyHostPortUrl(const nsACString& aUrl) {
  // Oh dear, the protocol is invalid. Test if the protocol might
  // actually be a url without a protocol:
  //
  //   http://www.faqs.org/rfcs/rfc1738.html
  //   http://www.faqs.org/rfcs/rfc2396.html
  //
  // e.g. Anything of the form:
  //
  //   <hostname>:<port> or
  //   <hostname>:<port>/
  //
  // Where <hostname> is a string of alphanumeric characters and dashes
  // separated by dots.
  // and <port> is a 5 or less digits. This actually breaks the rfc2396
  // definition of a scheme which allows dots in schemes.
  //
  // Note:
  //   People expecting this to work with
  //   <user>:<password>@<host>:<port>/<url-path> will be disappointed!
  //
  // Note: Parser could be a lot tighter, tossing out silly hostnames
  //       such as those containing consecutive dots and so on.

  // Read the hostname which should of the form
  // [a-zA-Z0-9\-]+(\.[a-zA-Z0-9\-]+)*:

  nsACString::const_iterator iterBegin;
  nsACString::const_iterator iterEnd;
  aUrl.BeginReading(iterBegin);
  aUrl.EndReading(iterEnd);
  nsACString::const_iterator iter = iterBegin;

  while (iter != iterEnd) {
    uint32_t chunkSize = 0;
    // Parse a chunk of the address
    while (iter != iterEnd &&
           (*iter == '-' || IsAsciiAlpha(*iter) || IsAsciiDigit(*iter))) {
      ++chunkSize;
      ++iter;
    }
    if (chunkSize == 0 || iter == iterEnd) {
      return false;
    }
    if (*iter == ':') {
      // Go onto checking the for the digits
      break;
    }
    if (*iter != '.') {
      // Whatever it is, it ain't a hostname!
      return false;
    }
    ++iter;
  }
  if (iter == iterEnd) {
    // No point continuing since there is no colon
    return false;
  }
  ++iter;

  // Count the number of digits after the colon and before the
  // next forward slash, question mark, hash sign, or end of string.

  uint32_t digitCount = 0;
  while (iter != iterEnd && digitCount <= 5) {
    if (IsAsciiDigit(*iter)) {
      digitCount++;
    } else if (*iter == '/' || *iter == '?' || *iter == '#') {
      break;
    } else {
      // Whatever it is, it ain't a port!
      return false;
    }
    ++iter;
  }
  if (digitCount == 0 || digitCount > 5) {
    // No digits or more digits than a port would have.
    return false;
  }

  // Yes, it's possibly a host:port url
  return true;
}

nsresult nsDefaultURIFixup::KeywordURIFixup(const nsACString& aURIString,
                                            nsDefaultURIFixupInfo* aFixupInfo,
                                            nsIInputStream** aPostData) {
  // These are keyword formatted strings
  // "what is mozilla"
  // "what is mozilla?"
  // "docshell site:mozilla.org" - has no dot/colon in the first space-separated
  // substring
  // "?mozilla" - anything that begins with a question mark
  // "?site:mozilla.org docshell"
  // Things that have a quote before the first dot/colon
  // "mozilla" - checked against a whitelist to see if it's a host or not
  // ".mozilla", "mozilla." - ditto

  // These are not keyword formatted strings
  // "www.blah.com" - first space-separated substring contains a dot, doesn't
  // start with "?" "www.blah.com stuff" "nonQualifiedHost:80" - first
  // space-separated substring contains a colon, doesn't start with "?"
  // "nonQualifiedHost:80 args"
  // "nonQualifiedHost?"
  // "nonQualifiedHost?args"
  // "nonQualifiedHost?some args"
  // "blah.com."

  // Note: uint32_t(kNotFound) is greater than any actual location
  // in practice.  So if we cast all locations to uint32_t, then a <
  // b guarantees that either b is kNotFound and a is found, or both
  // are found and a found before b.

  uint32_t firstDotLoc = uint32_t(kNotFound);
  uint32_t lastDotLoc = uint32_t(kNotFound);
  uint32_t firstColonLoc = uint32_t(kNotFound);
  uint32_t firstQuoteLoc = uint32_t(kNotFound);
  uint32_t firstSpaceLoc = uint32_t(kNotFound);
  uint32_t firstQMarkLoc = uint32_t(kNotFound);
  uint32_t lastLSBracketLoc = uint32_t(kNotFound);
  uint32_t lastSlashLoc = uint32_t(kNotFound);
  uint32_t pos = 0;
  uint32_t foundDots = 0;
  uint32_t foundColons = 0;
  uint32_t foundDigits = 0;
  uint32_t foundRSBrackets = 0;
  bool looksLikeIpv6 = true;
  bool hasAsciiAlpha = false;

  nsACString::const_iterator iterBegin;
  nsACString::const_iterator iterEnd;
  aURIString.BeginReading(iterBegin);
  aURIString.EndReading(iterEnd);
  nsACString::const_iterator iter = iterBegin;

  while (iter != iterEnd) {
    if (pos >= 1 && foundRSBrackets == 0) {
      if (!(lastLSBracketLoc == 0 &&
            (*iter == ':' || *iter == '.' || *iter == ']' ||
             (*iter >= 'a' && *iter <= 'f') || (*iter >= 'A' && *iter <= 'F') ||
             IsAsciiDigit(*iter)))) {
        looksLikeIpv6 = false;
      }
    }

    // If we're at the end of the string or this is the first slash,
    // check if the thing before the slash looks like ipv4:
    if ((iterEnd - iter == 1 ||
         (lastSlashLoc == uint32_t(kNotFound) && *iter == '/')) &&
        // Need 2 or 3 dots + only digits
        (foundDots == 2 || foundDots == 3) &&
        // and they should be all that came before now:
        (foundDots + foundDigits == pos ||
         // or maybe there was also exactly 1 colon that came after the last
         // dot, and the digits, dots and colon were all that came before now:
         (foundColons == 1 && firstColonLoc > lastDotLoc &&
          foundDots + foundDigits + foundColons == pos))) {
      // Hurray, we got ourselves some ipv4!
      // At this point, there's no way we will do a keyword lookup, so just bail
      // immediately:
      return NS_OK;
    }

    if (*iter == '.') {
      ++foundDots;
      lastDotLoc = pos;
      if (firstDotLoc == uint32_t(kNotFound)) {
        firstDotLoc = pos;
      }
    } else if (*iter == ':') {
      ++foundColons;
      if (firstColonLoc == uint32_t(kNotFound)) {
        firstColonLoc = pos;
      }
    } else if (*iter == ' ' && firstSpaceLoc == uint32_t(kNotFound)) {
      firstSpaceLoc = pos;
    } else if (*iter == '?' && firstQMarkLoc == uint32_t(kNotFound)) {
      firstQMarkLoc = pos;
    } else if ((*iter == '\'' || *iter == '"') &&
               firstQuoteLoc == uint32_t(kNotFound)) {
      firstQuoteLoc = pos;
    } else if (*iter == '[') {
      lastLSBracketLoc = pos;
    } else if (*iter == ']') {
      foundRSBrackets++;
    } else if (*iter == '/') {
      lastSlashLoc = pos;
    } else if (IsAsciiAlpha(*iter)) {
      hasAsciiAlpha = true;
    } else if (IsAsciiDigit(*iter)) {
      ++foundDigits;
    }

    pos++;
    iter++;
  }

  if (lastLSBracketLoc > 0 || foundRSBrackets != 1) {
    looksLikeIpv6 = false;
  }

  // If there are only colons and only hexadecimal characters ([a-z][0-9])
  // enclosed in [], then don't do a keyword lookup
  if (looksLikeIpv6) {
    return NS_OK;
  }

  nsAutoCString asciiHost;
  nsAutoCString displayHost;

  bool isValidHost =
      aFixupInfo->mFixedURI &&
      NS_SUCCEEDED(aFixupInfo->mFixedURI->GetAsciiHost(asciiHost)) &&
      !asciiHost.IsEmpty();

  bool isValidDisplayHost =
      aFixupInfo->mFixedURI &&
      NS_SUCCEEDED(aFixupInfo->mFixedURI->GetDisplayHost(displayHost)) &&
      !displayHost.IsEmpty();

  nsresult rv = NS_OK;
  // We do keyword lookups if a space or quote preceded the dot, colon
  // or question mark (or if the latter is not found, or if the input starts
  // with a question mark)
  if (((firstSpaceLoc < firstDotLoc || firstQuoteLoc < firstDotLoc) &&
       (firstSpaceLoc < firstColonLoc || firstQuoteLoc < firstColonLoc) &&
       (firstSpaceLoc < firstQMarkLoc || firstQuoteLoc < firstQMarkLoc)) ||
      firstQMarkLoc == 0) {
    rv = TryKeywordFixupForURIInfo(aFixupInfo->mOriginalInput, aFixupInfo,
                                   aPostData);
    // ... or when the asciiHost is the same as displayHost and there are no
    // characters from [a-z][A-Z]
  } else if (isValidHost && isValidDisplayHost && !hasAsciiAlpha &&
             asciiHost.EqualsIgnoreCase(displayHost.get())) {
    if (!StaticPrefs::browser_fixup_dns_first_for_single_words()) {
      rv = TryKeywordFixupForURIInfo(aFixupInfo->mOriginalInput, aFixupInfo,
                                     aPostData);
    }
  }
  // ... or if there is no question mark or colon, and there is either no
  // dot, or exactly 1 and it is the first or last character of the input:
  else if ((firstDotLoc == uint32_t(kNotFound) ||
            (foundDots == 1 &&
             (firstDotLoc == 0 || firstDotLoc == aURIString.Length() - 1))) &&
           firstColonLoc == uint32_t(kNotFound) &&
           firstQMarkLoc == uint32_t(kNotFound)) {
    if (isValidHost && IsDomainWhitelisted(asciiHost, firstDotLoc)) {
      return NS_OK;
    }

    // ... unless there are no dots, and a slash, and alpha characters, and
    // this is a valid host:
    if (firstDotLoc == uint32_t(kNotFound) &&
        lastSlashLoc != uint32_t(kNotFound) && hasAsciiAlpha && isValidHost) {
      return NS_OK;
    }

    // If we get here, we don't have a valid URI, or we did but the
    // host is not whitelisted, so we do a keyword search *anyway*:
    rv = TryKeywordFixupForURIInfo(aFixupInfo->mOriginalInput, aFixupInfo,
                                   aPostData);
  }
  return rv;
}

bool nsDefaultURIFixup::IsDomainWhitelisted(const nsACString& aAsciiHost,
                                            const uint32_t aDotLoc) {
  if (StaticPrefs::browser_fixup_dns_first_for_single_words()) {
    return true;
  }
  // Check if this domain is whitelisted as an actual
  // domain (which will prevent a keyword query)
  // NB: any processing of the host here should stay in sync with
  // code in the front-end(s) that set the pref.

  nsAutoCString pref("browser.fixup.domainwhitelist.");

  if (aDotLoc == aAsciiHost.Length() - 1) {
    pref.Append(Substring(aAsciiHost, 0, aAsciiHost.Length() - 1));
  } else {
    pref.Append(aAsciiHost);
  }

  return Preferences::GetBool(pref.get(), false);
}

NS_IMETHODIMP
nsDefaultURIFixup::IsDomainWhitelisted(const nsACString& aDomain,
                                       const uint32_t aDotLoc, bool* aResult) {
  *aResult = IsDomainWhitelisted(aDomain, aDotLoc);
  return NS_OK;
}

/* Implementation of nsIURIFixupInfo */
NS_IMPL_ISUPPORTS(nsDefaultURIFixupInfo, nsIURIFixupInfo)

nsDefaultURIFixupInfo::nsDefaultURIFixupInfo(const nsACString& aOriginalInput)
    : mFixupChangedProtocol(false), mFixupCreatedAlternateURI(false) {
  mOriginalInput = aOriginalInput;
}

nsDefaultURIFixupInfo::~nsDefaultURIFixupInfo() {}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetConsumer(nsISupports** aConsumer) {
  *aConsumer = mConsumer;
  NS_IF_ADDREF(*aConsumer);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::SetConsumer(nsISupports* aConsumer) {
  mConsumer = aConsumer;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetPreferredURI(nsIURI** aPreferredURI) {
  *aPreferredURI = mPreferredURI;
  NS_IF_ADDREF(*aPreferredURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetFixedURI(nsIURI** aFixedURI) {
  *aFixedURI = mFixedURI;
  NS_IF_ADDREF(*aFixedURI);
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetKeywordProviderName(nsAString& aResult) {
  aResult = mKeywordProviderName;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetKeywordAsSent(nsAString& aResult) {
  aResult = mKeywordAsSent;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetFixupChangedProtocol(bool* aResult) {
  *aResult = mFixupChangedProtocol;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetFixupCreatedAlternateURI(bool* aResult) {
  *aResult = mFixupCreatedAlternateURI;
  return NS_OK;
}

NS_IMETHODIMP
nsDefaultURIFixupInfo::GetOriginalInput(nsACString& aResult) {
  aResult = mOriginalInput;
  return NS_OK;
}
