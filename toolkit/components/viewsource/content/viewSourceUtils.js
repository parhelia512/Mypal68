/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * To keep the global namespace safe, don't define global variables and
 * functions in this file.
 *
 * This file silently depends on contentAreaUtils.js for
 * getDefaultFileName, getNormalizedLeafName and getDefaultExtension
 */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "ViewSourceBrowser",
  "resource://gre/modules/ViewSourceBrowser.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

var gViewSourceUtils = {
  mnsIWebBrowserPersist: Ci.nsIWebBrowserPersist,
  mnsIWebProgress: Ci.nsIWebProgress,
  mnsIWebPageDescriptor: Ci.nsIWebPageDescriptor,

  /**
   * Opens the view source window.
   *
   * @param aArgs (required)
   *        This Object can include the following properties:
   *
   *        URL (required):
   *          A string URL for the page we'd like to view the source of.
   *        browser (optional):
   *          The browser containing the document that we would like to view the
   *          source of. This is required if outerWindowID is passed.
   *        outerWindowID (optional):
   *          The outerWindowID of the content window containing the document that
   *          we want to view the source of. Pass this if you want to attempt to
   *          load the document source out of the network cache.
   *        lineNumber (optional):
   *          The line number to focus on once the source is loaded.
   */
  async viewSource(aArgs) {
    // Check if external view source is enabled.  If so, try it.  If it fails,
    // fallback to internal view source.
    if (Services.prefs.getBoolPref("view_source.editor.external")) {
      try {
        await this.openInExternalEditor(aArgs);
        return;
      } catch (data) {}
    }
    // Try existing browsers first.
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (browserWin && browserWin.BrowserViewSourceOfDocument) {
      browserWin.BrowserViewSourceOfDocument(aArgs);
      return;
    }
    // No browser window created yet, try to create one.
    let utils = this;
    Services.ww.registerNotification(function onOpen(win, topic) {
      if (
        win.document.documentURI !== "about:blank" ||
        topic !== "domwindowopened"
      ) {
        return;
      }
      Services.ww.unregisterNotification(onOpen);
      win.addEventListener(
        "load",
        () => {
          aArgs.viewSourceBrowser = win.gBrowser.selectedTab.linkedBrowser;
          utils.viewSourceInBrowser(aArgs);
        },
        { once: true }
      );
    });
    window.top.openWebLinkIn("about:blank", "current");
  },

  /**
   * Displays view source in the provided <browser>.  This allows for non-window
   * display methods, such as a tab from Firefox.
   *
   * @param aArgs
   *        An object with the following properties:
   *
   *        URL (required):
   *          A string URL for the page we'd like to view the source of.
   *        viewSourceBrowser (required):
   *          The browser to display the view source in.
   *        browser (optional):
   *          The browser containing the document that we would like to view the
   *          source of. This is required if outerWindowID is passed.
   *        outerWindowID (optional):
   *          The outerWindowID of the content window containing the document that
   *          we want to view the source of. Pass this if you want to attempt to
   *          load the document source out of the network cache.
   *        lineNumber (optional):
   *          The line number to focus on once the source is loaded.
   */
  viewSourceInBrowser(aArgs) {
    let viewSourceBrowser = new ViewSourceBrowser(aArgs.viewSourceBrowser);
    viewSourceBrowser.loadViewSource(aArgs);
  },

  /**
   * Displays view source for a selection from some document in the provided
   * <browser>.  This allows for non-window display methods, such as a tab from
   * Firefox.
   *
   * @param aViewSourceInBrowser
   *        The browser containing the page to view the source of.
   * @param aGetBrowserFn
   *        A function that will return a browser to open the source in.
   */
  viewPartialSourceInBrowser(aViewSourceInBrowser, aGetBrowserFn) {
    let mm = aViewSourceInBrowser.messageManager;
    mm.addMessageListener("ViewSource:GetSelectionDone", function gotSelection(
      message
    ) {
      mm.removeMessageListener("ViewSource:GetSelectionDone", gotSelection);

      if (!message.data) {
        return;
      }

      let viewSourceBrowser = new ViewSourceBrowser(aGetBrowserFn());
      viewSourceBrowser.loadViewSourceFromSelection(
        message.data.uri,
        message.data.drawSelection,
        message.data.baseURI
      );
    });

    mm.sendAsyncMessage("ViewSource:GetSelection");
  },

  buildEditorArgs(aPath, aLineNumber) {
    // Determine the command line arguments to pass to the editor.
    // We currently support a %LINE% placeholder which is set to the passed
    // line number (or to 0 if there's none)
    var editorArgs = [];
    var args = Services.prefs.getCharPref("view_source.editor.args");
    if (args) {
      args = args.replace("%LINE%", aLineNumber || "0");
      // add the arguments to the array (keeping quoted strings intact)
      const argumentRE = /"([^"]+)"|(\S+)/g;
      while (argumentRE.test(args)) {
        editorArgs.push(RegExp.$1 || RegExp.$2);
      }
    }
    editorArgs.push(aPath);
    return editorArgs;
  },

  /**
   * Opens an external editor with the view source content.
   *
   * @param aArgs (required)
   *        This Object can include the following properties:
   *
   *        URL (required):
   *          A string URL for the page we'd like to view the source of.
   *        browser (optional):
   *          The browser containing the document that we would like to view the
   *          source of. This is required if outerWindowID is passed.
   *        outerWindowID (optional):
   *          The outerWindowID of the content window containing the document that
   *          we want to view the source of. Pass this if you want to attempt to
   *          load the document source out of the network cache.
   *        lineNumber (optional):
   *          The line number to focus on once the source is loaded.
   *
   * @return Promise
   *        The promise will be resolved or rejected based on whether the
   *        external editor attempt was successful.  Either way, the data object
   *        is passed along as well.
   */
  openInExternalEditor(aArgs) {
    return new Promise((resolve, reject) => {
      let data;
      let { URL, browser, lineNumber } = aArgs;
      data = {
        url: URL,
        lineNumber,
        isPrivate: false,
      };
      if (browser) {
        data.doc = {
          characterSet: browser.characterSet,
          contentType: browser.documentContentType,
          title: browser.contentTitle,
        };
        data.isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
      }

      try {
        var editor = this.getExternalViewSourceEditor();
        if (!editor) {
          reject(data);
          return;
        }

        // make a uri
        var charset = data.doc ? data.doc.characterSet : null;
        var uri = Services.io.newURI(data.url, charset);
        data.uri = uri;

        var path;
        var contentType = data.doc ? data.doc.contentType : null;
        if (uri.scheme == "file") {
          // it's a local file; we can open it directly
          path = uri.QueryInterface(Ci.nsIFileURL).file.path;

          var editorArgs = this.buildEditorArgs(path, data.lineNumber);
          editor.runw(false, editorArgs, editorArgs.length);
          resolve(data);
        } else {
          // set up the progress listener with what we know so far
          this.viewSourceProgressListener.contentLoaded = false;
          this.viewSourceProgressListener.editor = editor;
          this.viewSourceProgressListener.resolve = resolve;
          this.viewSourceProgressListener.reject = reject;
          this.viewSourceProgressListener.data = data;

          // without a page descriptor, loadPage has no chance of working. download the file.
          var file = this.getTemporaryFile(uri, data.doc, contentType);
          this.viewSourceProgressListener.file = file;

          var webBrowserPersist = Cc[
            "@mozilla.org/embedding/browser/nsWebBrowserPersist;1"
          ].createInstance(this.mnsIWebBrowserPersist);
          // the default setting is to not decode. we need to decode.
          webBrowserPersist.persistFlags = this.mnsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
          webBrowserPersist.progressListener = this.viewSourceProgressListener;
          let ssm = Services.scriptSecurityManager;
          let principal = ssm.createCodebasePrincipal(
            data.uri,
            browser.contentPrincipal.originAttributes
          );
          webBrowserPersist.savePrivacyAwareURI(
            uri,
            principal,
            null,
            null,
            null,
            null,
            file,
            Ci.nsIContentPolicy.TYPE_SAVEAS_DOWNLOAD,
            data.isPrivate
          );

          let helperService = Cc[
            "@mozilla.org/uriloader/external-helper-app-service;1"
          ].getService(Ci.nsPIExternalAppLauncher);
          if (data.isPrivate) {
            // register the file to be deleted when possible
            helperService.deleteTemporaryPrivateFileWhenPossible(file);
          } else {
            // register the file to be deleted on app exit
            helperService.deleteTemporaryFileOnExit(file);
          }
        }
      } catch (ex) {
        // we failed loading it with the external editor.
        Cu.reportError(ex);
        reject(data);
      }
    });
  },

  // Returns nsIProcess of the external view source editor or null
  getExternalViewSourceEditor() {
    try {
      let viewSourceAppPath = Services.prefs.getComplexValue(
        "view_source.editor.path",
        Ci.nsIFile
      );
      let editor = Cc["@mozilla.org/process/util;1"].createInstance(
        Ci.nsIProcess
      );
      editor.init(viewSourceAppPath);

      return editor;
    } catch (ex) {
      Cu.reportError(ex);
    }

    return null;
  },

  viewSourceProgressListener: {
    mnsIWebProgressListener: Ci.nsIWebProgressListener,

    QueryInterface: ChromeUtils.generateQI([
      "nsIWebProgressListener",
      "nsISupportsWeakReference",
    ]),

    destroy() {
      if (this.webShell) {
        this.webShell.QueryInterface(Ci.nsIBaseWindow).destroy();
      }
      this.webShell = null;
      this.editor = null;
      this.resolve = null;
      this.reject = null;
      this.data = null;
      this.file = null;
    },

    // This listener is used both for tracking the progress of an HTML parse
    // in one case and for tracking the progress of nsIWebBrowserPersist in
    // another case.
    onStateChange(aProgress, aRequest, aFlag, aStatus) {
      // once it's done loading...
      if (aFlag & this.mnsIWebProgressListener.STATE_STOP && aStatus == 0) {
        if (!this.webShell) {
          // We aren't waiting for the parser. Instead, we are waiting for
          // an nsIWebBrowserPersist.
          this.onContentLoaded();
          return 0;
        }
        var webNavigation = this.webShell.QueryInterface(Ci.nsIWebNavigation);
        if (webNavigation.document.readyState == "complete") {
          // This branch is probably never taken. Including it for completeness.
          this.onContentLoaded();
        } else {
          webNavigation.document.addEventListener(
            "DOMContentLoaded",
            this.onContentLoaded.bind(this)
          );
        }
      }
      return 0;
    },

    onContentLoaded() {
      // The progress listener may call this multiple times, so be sure we only
      // run once.
      if (this.contentLoaded) {
        return;
      }
      try {
        if (!this.file) {
          // it's not saved to file yet, it's in the webshell

          // get a temporary filename using the attributes from the data object that
          // openInExternalEditor gave us
          this.file = gViewSourceUtils.getTemporaryFile(
            this.data.uri,
            this.data.doc,
            this.data.doc.contentType
          );

          // we have to convert from the source charset.
          var webNavigation = this.webShell.QueryInterface(Ci.nsIWebNavigation);
          var foStream = Cc[
            "@mozilla.org/network/file-output-stream;1"
          ].createInstance(Ci.nsIFileOutputStream);
          foStream.init(this.file, 0x02 | 0x08 | 0x20, -1, 0); // write | create | truncate
          var coStream = Cc[
            "@mozilla.org/intl/converter-output-stream;1"
          ].createInstance(Ci.nsIConverterOutputStream);
          coStream.init(foStream, this.data.doc.characterSet);

          // write the source to the file
          coStream.writeString(webNavigation.document.body.textContent);

          // clean up
          coStream.close();
          foStream.close();

          let helperService = Cc[
            "@mozilla.org/uriloader/external-helper-app-service;1"
          ].getService(Ci.nsPIExternalAppLauncher);
          if (this.data.isPrivate) {
            // register the file to be deleted when possible
            helperService.deleteTemporaryPrivateFileWhenPossible(this.file);
          } else {
            // register the file to be deleted on app exit
            helperService.deleteTemporaryFileOnExit(this.file);
          }
        }

        var editorArgs = gViewSourceUtils.buildEditorArgs(
          this.file.path,
          this.data.lineNumber
        );
        this.editor.runw(false, editorArgs, editorArgs.length);

        this.contentLoaded = true;
        this.resolve(this.data);
      } catch (ex) {
        // we failed loading it with the external editor.
        Cu.reportError(ex);
        this.reject(this.data);
      } finally {
        this.destroy();
      }
    },

    webShell: null,
    editor: null,
    resolve: null,
    reject: null,
    data: null,
    file: null,
  },

  // returns an nsIFile for the passed document in the system temp directory
  getTemporaryFile(aURI, aDocument, aContentType) {
    // include contentAreaUtils.js in our own context when we first need it
    if (!this._caUtils) {
      this._caUtils = {};
      Services.scriptloader.loadSubScript(
        "chrome://global/content/contentAreaUtils.js",
        this._caUtils
      );
    }

    var tempFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
    var fileName = this._caUtils.getDefaultFileName(
      null,
      aURI,
      aDocument,
      aContentType
    );
    var extension = this._caUtils.getDefaultExtension(
      fileName,
      aURI,
      aContentType
    );
    var leafName = this._caUtils.getNormalizedLeafName(fileName, extension);
    tempFile.append(leafName);
    return tempFile;
  },
};
