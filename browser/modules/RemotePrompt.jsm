/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["RemotePrompt"];

ChromeUtils.defineModuleGetter(
  this,
  "PromptUtils",
  "resource://gre/modules/SharedPromptUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

var RemotePrompt = {
  // Listeners are added in BrowserGlue.jsm
  receiveMessage(message) {
    switch (message.name) {
      case "Prompt:Open":
        const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";
        const SELECT_DIALOG = "chrome://global/content/selectDialog.xhtml";

        if (message.data.tabPrompt) {
          this.openTabPrompt(message.data, message.target);
        } else {
          let uri =
            message.data.promptType == "select" ? SELECT_DIALOG : COMMON_DIALOG;
          this.openModalWindow(uri, message.data, message.target);
        }
        break;
    }
  },

  openTabPrompt(args, browser) {
    let window = browser.ownerGlobal;
    let tabPrompt = window.gBrowser.getTabModalPromptBox(browser);
    let newPrompt;
    let needRemove = false;
    let promptId = args._remoteId;

    function onPromptClose(forceCleanup) {
      // It's possible that we removed the prompt during the
      // appendPrompt call below. In that case, newPrompt will be
      // undefined. We set the needRemove flag to remember to remove
      // it right after we've finished adding it.
      if (newPrompt) {
        tabPrompt.removePrompt(newPrompt);
      } else {
        needRemove = true;
      }

      PromptUtils.fireDialogEvent(window, "DOMModalDialogClosed", browser);
      browser.messageManager.sendAsyncMessage("Prompt:Close", args);
    }

    browser.messageManager.addMessageListener(
      "Prompt:ForceClose",
      function listener(message) {
        // If this was for another prompt in the same tab, ignore it.
        if (message.data._remoteId !== promptId) {
          return;
        }

        browser.messageManager.removeMessageListener(
          "Prompt:ForceClose",
          listener
        );

        if (newPrompt) {
          newPrompt.abortPrompt();
        }
      }
    );

    try {
      let eventDetail = {
        tabPrompt: true,
        promptPrincipal: args.promptPrincipal,
        inPermitUnload: args.inPermitUnload,
      };
      PromptUtils.fireDialogEvent(
        window,
        "DOMWillOpenModalDialog",
        browser,
        eventDetail
      );

      args.promptActive = true;

      newPrompt = tabPrompt.appendPrompt(args, onPromptClose);

      if (needRemove) {
        tabPrompt.removePrompt(newPrompt);
      }

      // TODO since we don't actually open a window, need to check if
      // there's other stuff in nsWindowWatcher::OpenWindowInternal
      // that we might need to do here as well.
    } catch (ex) {
      Cu.reportError(ex);
      onPromptClose(true);
    }
  },

  openModalWindow(uri, args, browser) {
    let window = browser.ownerGlobal;
    try {
      PromptUtils.fireDialogEvent(window, "DOMWillOpenModalDialog", browser);
      let bag = PromptUtils.objectToPropBag(args);

      Services.ww.openWindow(
        window,
        uri,
        "_blank",
        "centerscreen,chrome,modal,titlebar",
        bag
      );

      PromptUtils.propBagToObject(bag, args);
    } finally {
      PromptUtils.fireDialogEvent(window, "DOMModalDialogClosed", browser);
      browser.messageManager.sendAsyncMessage("Prompt:Close", args);
    }
  },
};
