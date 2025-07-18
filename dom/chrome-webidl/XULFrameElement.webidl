/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDocShell;
interface nsIWebNavigation;

[ChromeOnly,
 Exposed=Window]
interface XULFrameElement : XULElement
{
  [HTMLConstructor] constructor();

  readonly attribute nsIDocShell? docShell;
  readonly attribute nsIWebNavigation? webNavigation;

  readonly attribute WindowProxy? contentWindow;
  readonly attribute Document? contentDocument; 
};

XULFrameElement includes MozFrameLoaderOwner;
