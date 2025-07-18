/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_IHistory_h_
#define mozilla_IHistory_h_

#include "nsISupports.h"
#include "nsDataHashtable.h"
#include "nsURIHashKey.h"
#include "nsTObserverArray.h"

class nsIURI;
class nsIWidget;

namespace mozilla {

namespace dom {
class Document;
class Link;
}  // namespace dom

// 0057c9d3-b98e-4933-bdc5-0275d06705e1
#define IHISTORY_IID                                 \
  {                                                  \
    0x0057c9d3, 0xb98e, 0x4933, {                    \
      0xbd, 0xc5, 0x02, 0x75, 0xd0, 0x67, 0x05, 0xe1 \
    }                                                \
  }

class IHistory : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(IHISTORY_IID)

  /**
   * Registers the Link for notifications about the visited-ness of aURI.
   * Consumers should assume that the URI is unvisited after calling this, and
   * they will be notified if that state (unvisited) changes by having
   * SetLinkState called on themselves.  This function is guaranteed to run to
   * completion before aLink is notified.  After the node is notified, it will
   * be unregistered.
   *
   * @note SetLinkState must not call RegisterVisitedCallback or
   *       UnregisterVisitedCallback.
   *
   * @pre aURI must not be null.
   * @pre aLink may be null only in the parent (chrome) process.
   *
   * @param aURI
   *        The URI to check.
   * @param aLink
   *        The link to update whenever the history status changes.  The
   *        implementation will only hold onto a raw pointer, so if this
   *        object should be destroyed, be sure to call
   *        UnregisterVistedCallback first.
   */
  virtual nsresult RegisterVisitedCallback(nsIURI* aURI, dom::Link* aLink) = 0;

  /**
   * Unregisters a previously registered Link object.  This must be called
   * before destroying the registered object, and asserts when misused.
   *
   * @pre aURI must not be null.
   * @pre aLink must not be null.
   *
   * @param aURI
   *        The URI that aLink was registered for.
   * @param aLink
   *        The link object to unregister for aURI.
   */
  virtual void UnregisterVisitedCallback(nsIURI* aURI, dom::Link* aLink) = 0;

  enum class VisitedStatus : uint8_t {
    Unknown,
    Visited,
    Unvisited,
  };

  /**
   * Notifies about the visited status of a given URI. The visited status cannot
   * be unknown, otherwise there's no point in notifying of anything.
   */
  virtual void NotifyVisited(nsIURI*, VisitedStatus) = 0;

  enum VisitFlags {
    /**
     * Indicates whether the URI was loaded in a top-level window.
     */
    TOP_LEVEL = 1 << 0,
    /**
     * Indicates whether the URI is the target of a permanent redirect.
     */
    REDIRECT_PERMANENT = 1 << 1,
    /**
     * Indicates whether the URI is the target of a temporary redirect.
     */
    REDIRECT_TEMPORARY = 1 << 2,
    /**
     * Indicates the URI will redirect  (Response code 3xx).
     */
    REDIRECT_SOURCE = 1 << 3,
    /**
     * Indicates the URI caused an error that is unlikely fixable by a
     * retry, like a not found or unfetchable page.
     */
    UNRECOVERABLE_ERROR = 1 << 4,
    /**
     * If REDIRECT_SOURCE is set, this indicates that the redirect is permanent.
     * Note this differs from REDIRECT_PERMANENT because that one refers to how
     * we reached the URI, while this is used when the URI itself redirects.
     */
    REDIRECT_SOURCE_PERMANENT = 1 << 5
  };

  /**
   * Adds a history visit for the URI.
   *
   * @pre aURI must not be null.
   *
   * @param aWidget
   *        The widget for the DocShell.
   * @param aURI
   *        The URI of the page being visited.
   * @param aLastVisitedURI
   *        The URI of the last visit in the chain.
   * @param aFlags
   *        The VisitFlags describing this visit.
   */
  NS_IMETHOD VisitURI(nsIWidget* aWidget, nsIURI* aURI, nsIURI* aLastVisitedURI,
                      uint32_t aFlags) = 0;

  /**
   * Set the title of the URI.
   *
   * @pre aURI must not be null.
   *
   * @param aURI
   *        The URI to set the title for.
   * @param aTitle
   *        The title string.
   */
  NS_IMETHOD SetURITitle(nsIURI* aURI, const nsAString& aTitle) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IHistory, IHISTORY_IID)

}  // namespace mozilla

#endif  // mozilla_IHistory_h_
