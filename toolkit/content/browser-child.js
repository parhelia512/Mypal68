/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

try {
  docShell
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIBrowserChild)
    .beginSendingWebProgressEventsToParent();
} catch (e) {
  // In responsive design mode, we do not have a BrowserChild for the in-parent
  // document.
}

// This message is used to measure content process startup performance in Talos
// tests.
sendAsyncMessage("Content:BrowserChildReady", {
  time: Services.telemetry.msSystemNow(),
});

addEventListener(
  "DOMTitleChanged",
  function(aEvent) {
    if (!aEvent.isTrusted || aEvent.target.defaultView != content) {
      return;
    }
    sendAsyncMessage("DOMTitleChanged", { title: content.document.title });
  },
  false
);

// We may not get any responses to Browser:Init if the browser element
// is torn down too quickly.
var outerWindowID = docShell.outerWindowID;
var browsingContextId = docShell.browsingContext.id;
sendAsyncMessage("Browser:Init", { outerWindowID, browsingContextId });
