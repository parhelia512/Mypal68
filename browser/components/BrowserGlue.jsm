/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BrowserGlue", "ContentPermissionPrompt"];

const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ActorManagerParent",
  "resource://gre/modules/ActorManagerParent.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm"
);

const PREF_PDFJS_ISDEFAULT_CACHE_STATE = "pdfjs.enabledCache.state";

let ACTORS = {
  SubframeCrash: {
    parent: {
      moduleURI: "resource:///actors/SubframeCrashParent.jsm",
    },

    child: {
      moduleURI: "resource:///actors/SubframeCrashChild.jsm",
    },

    allFrames: true,
  },
};

let LEGACY_ACTORS = {
  AboutLogins: {
    child: {
      matches: ["about:logins", "about:logins?*"],
      module: "resource:///actors/AboutLoginsChild.jsm",
      events: {
        AboutLoginsCopyLoginDetail: { wantUntrusted: true },
        AboutLoginsCreateLogin: { wantUntrusted: true },
        AboutLoginsDeleteLogin: { wantUntrusted: true },
        AboutLoginsImport: { wantUntrusted: true },
        AboutLoginsInit: { wantUntrusted: true },
        AboutLoginsOpenPreferences: { wantUntrusted: true },
        AboutLoginsOpenSite: { wantUntrusted: true },
        AboutLoginsUpdateLogin: { wantUntrusted: true },
      },
      messages: [
        "AboutLogins:AllLogins",
        "AboutLogins:LoginAdded",
        "AboutLogins:LoginModified",
        "AboutLogins:LoginRemoved",
        "AboutLogins:MasterPasswordResponse",
      ],
    },
  },

  AboutReader: {
    child: {
      module: "resource:///actors/AboutReaderChild.jsm",
      group: "browsers",
      events: {
        AboutReaderContentLoaded: { wantUntrusted: true },
        DOMContentLoaded: {},
        pageshow: { mozSystemGroup: true },
        pagehide: { mozSystemGroup: true },
      },
      messages: ["Reader:ToggleReaderMode", "Reader:PushState"],
    },
  },

  BrowserTab: {
    child: {
      module: "resource:///actors/BrowserTabChild.jsm",
      group: "browsers",

      events: {
        DOMWindowCreated: { once: true },
        MozAfterPaint: { once: true },
        "MozDOMPointerLock:Entered": {},
        "MozDOMPointerLock:Exited": {},
      },

      messages: [
        "AllowScriptsToClose",
        "Browser:AppTab",
        "Browser:HasSiblings",
        "Browser:Reload",
        "MixedContent:ReenableProtection",
        "SwitchDocumentDirection",
        "UpdateCharacterSet",
      ],
    },
  },

  ClickHandler: {
    child: {
      module: "resource:///actors/ClickHandlerChild.jsm",
      events: {
        click: { capture: true, mozSystemGroup: true },
        auxclick: { capture: true, mozSystemGroup: true },
      },
    },
  },

  ContextMenu: {
    child: {
      module: "resource:///actors/ContextMenuChild.jsm",
      events: {
        contextmenu: { mozSystemGroup: true },
      },
    },
  },

  ContentSearch: {
    child: {
      module: "resource:///actors/ContentSearchChild.jsm",
      group: "browsers",
      matches: [
        "about:home",
        "about:newtab",
        "about:welcome",
        "about:privatebrowsing",
        "chrome://mochitests/content/*",
      ],
      events: {
        ContentSearchClient: { capture: true, wantUntrusted: true },
      },
      messages: ["ContentSearch"],
    },
  },

  DOMFullscreen: {
    child: {
      module: "resource:///actors/DOMFullscreenChild.jsm",
      group: "browsers",
      events: {
        "MozDOMFullscreen:Request": {},
        "MozDOMFullscreen:Entered": {},
        "MozDOMFullscreen:NewOrigin": {},
        "MozDOMFullscreen:Exit": {},
        "MozDOMFullscreen:Exited": {},
      },
      messages: ["DOMFullscreen:Entered", "DOMFullscreen:CleanUp"],
    },
  },

  FormValidation: {
    child: {
      module: "resource:///actors/FormValidationChild.jsm",
      events: {
        MozInvalidForm: {},
      },
    },
  },

  LightweightTheme: {
    child: {
      module: "resource:///actors/LightweightThemeChild.jsm",
      matches: ["about:home", "about:newtab", "about:welcome"],
      events: {
        pageshow: { mozSystemGroup: true },
      },
    },
  },

  LinkHandler: {
    child: {
      module: "resource:///actors/LinkHandlerChild.jsm",
      events: {
        DOMHeadElementParsed: {},
        DOMLinkAdded: {},
        DOMLinkChanged: {},
        pageshow: {},
        pagehide: {},
      },
    },
  },

  NetError: {
    child: {
      module: "resource:///actors/NetErrorChild.jsm",
      events: {
        AboutNetErrorLoad: { wantUntrusted: true },
        AboutNetErrorSetAutomatic: { wantUntrusted: true },
        AboutNetErrorResetPreferences: { wantUntrusted: true },
        click: {},
      },
      matches: ["about:certerror?*", "about:neterror?*"],
      allFrames: true,
      messages: ["CertErrorDetails"],
    },
  },

  OfflineApps: {
    child: {
      module: "resource:///actors/OfflineAppsChild.jsm",
      events: {
        MozApplicationManifest: {},
      },
      messages: ["OfflineApps:StartFetching"],
    },
  },

  PageInfo: {
    child: {
      module: "resource:///actors/PageInfoChild.jsm",
      messages: ["PageInfo:getData"],
    },
  },

  PageStyle: {
    child: {
      module: "resource:///actors/PageStyleChild.jsm",
      group: "browsers",
      events: {
        pageshow: {},
      },
      messages: ["PageStyle:Switch", "PageStyle:Disable"],
      // Only matching web pages, as opposed to internal about:, chrome: or
      // resource: pages. See https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns
      matches: ["*://*/*"],
    },
  },

  Plugin: {
    child: {
      module: "resource:///actors/PluginChild.jsm",
      events: {
        PluginBindingAttached: { capture: true, wantUntrusted: true },
        PluginCrashed: { capture: true },
        PluginOutdated: { capture: true },
        PluginInstantiated: { capture: true },
        PluginRemoved: { capture: true },
        HiddenPlugin: { capture: true },
      },

      messages: [
        "BrowserPlugins:ActivatePlugins",
        "BrowserPlugins:ContextMenuCommand",
        "BrowserPlugins:NPAPIPluginProcessCrashed",
        "BrowserPlugins:CrashReportSubmitted",
        "BrowserPlugins:Test:ClearCrashData",
      ],

      observers: ["decoder-doctor-notification"],
    },
  },

  RFPHelper: {
    child: {
      module: "resource:///actors/RFPHelperChild.jsm",
      group: "browsers",
      events: {
        resize: {},
      },
    },
  },

  URIFixup: {
    child: {
      module: "resource:///actors/URIFixupChild.jsm",
      group: "browsers",
      observers: ["keyword-uri-fixup"],
    },
  },

  WebRTC: {
    child: {
      module: "resource:///actors/WebRTCChild.jsm",
      messages: [
        "rtcpeer:Allow",
        "rtcpeer:Deny",
        "webrtc:Allow",
        "webrtc:Deny",
        "webrtc:StopSharing",
      ],
    },
  },
};

(function earlyBlankFirstPaint() {
  if (!Services.prefs.getBoolPref("browser.startup.blankWindow", false)) {
    return;
  }

  // Until bug 1450626 and bug 1488384 are fixed, skip the blank window when
  // using a non-default theme.
  if (
    Services.prefs.getCharPref(
      "extensions.activeThemeID",
      "default-theme@mozilla.org"
    ) != "default-theme@mozilla.org"
  ) {
    return;
  }

  let store = Services.xulStore;
  let getValue = attr =>
    store.getValue(AppConstants.BROWSER_CHROME_URL, "main-window", attr);
  let width = getValue("width");
  let height = getValue("height");

  // The clean profile case isn't handled yet. Return early for now.
  if (!width || !height) {
    return;
  }

  let browserWindowFeatures =
    "chrome,all,dialog=no,extrachrome,menubar,resizable,scrollbars,status," +
    "location,toolbar,personalbar";
  let win = Services.ww.openWindow(
    null,
    "about:blank",
    null,
    browserWindowFeatures,
    null
  );

  // Hide the titlebar if the actual browser window will draw in it.
  let hiddenTitlebar = Services.prefs.getBoolPref(
    "browser.tabs.drawInTitlebar",
    win.matchMedia("(-moz-gtk-csd-hide-titlebar-by-default)").matches
  );
  if (hiddenTitlebar) {
    win.windowUtils.setChromeMargin(0, 2, 2, 2);
  }

  let docElt = win.document.documentElement;
  docElt.setAttribute("screenX", getValue("screenX"));
  docElt.setAttribute("screenY", getValue("screenY"));

  // The sizemode="maximized" attribute needs to be set before first paint.
  let sizemode = getValue("sizemode");
  if (sizemode == "maximized") {
    docElt.setAttribute("sizemode", sizemode);

    // Set the size to use when the user leaves the maximized mode.
    // The persisted size is the outer size, but the height/width
    // attributes set the inner size.
    let appWin = win.docShell.treeOwner
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIAppWindow);
    height -= appWin.outerToInnerHeightDifferenceInCSSPixels;
    width -= appWin.outerToInnerWidthDifferenceInCSSPixels;
    docElt.setAttribute("height", height);
    docElt.setAttribute("width", width);
  } else {
    // Setting the size of the window in the features string instead of here
    // causes the window to grow by the size of the titlebar.
    win.resizeTo(width, height);
  }

  // Set this before showing the window so that graphics code can use it to
  // decide to skip some expensive code paths (eg. starting the GPU process).
  docElt.setAttribute("windowtype", "navigator:blank");

  // The window becomes visible after OnStopRequest, so make this happen now.
  win.stop();
})();

XPCOMUtils.defineLazyServiceGetters(this, {
  aboutNewTabService: [
    "@mozilla.org/browser/aboutnewtab-service;1",
    "nsIAboutNewTabService",
  ],
});
XPCOMUtils.defineLazyGetter(
  this,
  "WeaveService",
  () => Cc["@mozilla.org/weave/service;1"].getService().wrappedJSObject
);

// lazy module getters

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNetErrorHandler:
    "resource:///modules/aboutpages/AboutNetErrorHandler.jsm",
  AboutPrivateBrowsingHandler:
    "resource:///modules/aboutpages/AboutPrivateBrowsingHandler.jsm",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  AutoCompletePopup: "resource://gre/modules/AutoCompletePopup.jsm",
  Blocklist: "resource://gre/modules/Blocklist.jsm",
  BookmarkHTMLUtils: "resource://gre/modules/BookmarkHTMLUtils.jsm",
  BookmarkJSONUtils: "resource://gre/modules/BookmarkJSONUtils.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.jsm",
  DateTimePickerParent: "resource://gre/modules/DateTimePickerParent.jsm",
  ExtensionsUI: "resource:///modules/ExtensionsUI.jsm",
  FxAccounts: "resource://gre/modules/FxAccounts.jsm",
  HomePage: "resource:///modules/HomePage.jsm",
  Integration: "resource://gre/modules/Integration.jsm",
  LiveBookmarkMigrator: "resource:///modules/LiveBookmarkMigrator.jsm",
  NewTabUtils: "resource://gre/modules/NewTabUtils.jsm",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PageActions: "resource:///modules/PageActions.jsm",
  PageThumbs: "resource://gre/modules/PageThumbs.jsm",
  PdfJs: "resource://pdf.js/PdfJs.jsm",
  PermissionUI: "resource:///modules/PermissionUI.jsm",
  PlacesBackups: "resource://gre/modules/PlacesBackups.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PluralForm: "resource://gre/modules/PluralForm.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  ProcessHangMonitor: "resource:///modules/ProcessHangMonitor.jsm",
  RemoteSecuritySettings: "resource://gre/modules/psm/RemoteSecuritySettings.jsm",
  RFPHelper: "resource://gre/modules/RFPHelper.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  ShellService: "resource:///modules/ShellService.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  TabUnloader: "resource:///modules/TabUnloader.jsm",
  UIState: "resource://services-sync/UIState.jsm",
  WebChannel: "resource://gre/modules/WebChannel.jsm",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.jsm",
});

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  AboutLoginsParent: "resource:///modules/AboutLoginsParent.jsm",
  AsyncPrefs: "resource://gre/modules/AsyncPrefs.jsm",
  ContentClick: "resource:///modules/ContentClick.jsm",
  FormValidationHandler: "resource:///modules/FormValidationHandler.jsm",
  LoginManagerParent: "resource://gre/modules/LoginManagerParent.jsm",
  PictureInPicture: "resource://gre/modules/PictureInPicture.jsm",
  ReaderParent: "resource:///modules/ReaderParent.jsm",
  RemotePrompt: "resource:///modules/RemotePrompt.jsm",
});

/**
 * IF YOU ADD OR REMOVE FROM THIS LIST, PLEASE UPDATE THE LIST ABOVE AS WELL.
 * XXX Bug 1325373 is for making eslint detect these automatically.
 */

let initializedModules = {};

[
  [
    "ContentPrefServiceParent",
    "resource://gre/modules/ContentPrefServiceParent.jsm",
    "alwaysInit",
  ],
  ["ContentSearch", "resource:///modules/ContentSearch.jsm", "init"],
  ["UpdateListener", "resource://gre/modules/UpdateListener.jsm", "init"],
  ["webrtcUI", "resource:///modules/webrtcUI.jsm", "init"],
].forEach(([name, resource, init]) => {
  XPCOMUtils.defineLazyGetter(this, name, () => {
    ChromeUtils.import(resource, initializedModules);
    initializedModules[name][init]();
    return initializedModules[name];
  });
});

if (AppConstants.MOZ_CRASHREPORTER) {
  XPCOMUtils.defineLazyModuleGetters(this, {
    PluginCrashReporter: "resource:///modules/ContentCrashHandlers.jsm",
    UnsubmittedCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
  });
}

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

XPCOMUtils.defineLazyGetter(this, "gTabbrowserBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/tabbrowser.properties"
  );
});

const global = this;

const listeners = {
  observers: {
    "update-staged": ["UpdateListener"],
    "update-downloaded": ["UpdateListener"],
    "update-available": ["UpdateListener"],
    "update-error": ["UpdateListener"],
  },

  ppmm: {
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN ContentPrefServiceParent.init
    "ContentPrefs:FunctionCall": ["ContentPrefServiceParent"],
    "ContentPrefs:AddObserverForName": ["ContentPrefServiceParent"],
    "ContentPrefs:RemoveObserverForName": ["ContentPrefServiceParent"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN ContentPrefServiceParent.init

    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN AsyncPrefs.init
    "AsyncPrefs:SetPref": ["AsyncPrefs"],
    "AsyncPrefs:ResetPref": ["AsyncPrefs"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE LISTENERS ADDED IN AsyncPrefs.init

    "webrtc:UpdateGlobalIndicators": ["webrtcUI"],
    "webrtc:UpdatingIndicators": ["webrtcUI"],
  },

  mm: {
    "AboutLogins:CreateLogin": ["AboutLoginsParent"],
    "AboutLogins:DeleteLogin": ["AboutLoginsParent"],
    "AboutLogins:Import": ["AboutLoginsParent"],
    "AboutLogins:MasterPasswordRequest": ["AboutLoginsParent"],
    "AboutLogins:OpenPreferences": ["AboutLoginsParent"],
    "AboutLogins:OpenSite": ["AboutLoginsParent"],
    "AboutLogins:Subscribe": ["AboutLoginsParent"],
    "AboutLogins:UpdateLogin": ["AboutLoginsParent"],
    "Content:Click": ["ContentClick"],
    ContentSearch: ["ContentSearch"],
    "FormValidation:ShowPopup": ["FormValidationHandler"],
    "FormValidation:HidePopup": ["FormValidationHandler"],
    "PictureInPicture:Request": ["PictureInPicture"],
    "PictureInPicture:Close": ["PictureInPicture"],
    "PictureInPicture:Playing": ["PictureInPicture"],
    "PictureInPicture:Paused": ["PictureInPicture"],
    "PictureInPicture:OpenToggleContextMenu": ["PictureInPicture"],
    "Prompt:Open": ["RemotePrompt"],
    "Reader:FaviconRequest": ["ReaderParent"],
    "Reader:UpdateReaderButton": ["ReaderParent"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE MOBILE LISTENERS IN BrowserCLH.js
    "PasswordManager:findLogins": ["LoginManagerParent"],
    "PasswordManager:findRecipes": ["LoginManagerParent"],
    "PasswordManager:onFormSubmit": ["LoginManagerParent"],
    "PasswordManager:onGeneratedPasswordFilledOrEdited": ["LoginManagerParent"],
    "PasswordManager:autoCompleteLogins": ["LoginManagerParent"],
    "PasswordManager:removeLogin": ["LoginManagerParent"],
    "PasswordManager:insecureLoginFormPresent": ["LoginManagerParent"],
    "PasswordManager:OpenPreferences": ["LoginManagerParent"],
    // PLEASE KEEP THIS LIST IN SYNC WITH THE MOBILE LISTENERS IN BrowserCLH.js
    "rtcpeer:CancelRequest": ["webrtcUI"],
    "rtcpeer:Request": ["webrtcUI"],
    "webrtc:CancelRequest": ["webrtcUI"],
    "webrtc:Request": ["webrtcUI"],
    "webrtc:StopRecording": ["webrtcUI"],
    "webrtc:UpdateBrowserIndicators": ["webrtcUI"],
  },

  observe(subject, topic, data) {
    for (let module of this.observers[topic]) {
      try {
        global[module].observe(subject, topic, data);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  },

  receiveMessage(modules, data) {
    let val;
    for (let module of modules[data.name]) {
      try {
        val = global[module].receiveMessage(data) || val;
      } catch (e) {
        Cu.reportError(e);
      }
    }
    return val;
  },

  init() {
    for (let observer of Object.keys(this.observers)) {
      Services.obs.addObserver(this, observer);
    }

    let receiveMessageMM = this.receiveMessage.bind(this, this.mm);
    for (let message of Object.keys(this.mm)) {
      Services.mm.addMessageListener(message, receiveMessageMM);
    }

    let receiveMessagePPMM = this.receiveMessage.bind(this, this.ppmm);
    for (let message of Object.keys(this.ppmm)) {
      Services.ppmm.addMessageListener(message, receiveMessagePPMM);
    }
  },
};

// Seconds of idle before trying to create a bookmarks backup.
const BOOKMARKS_BACKUP_IDLE_TIME_SEC = 8 * 60;
// Minimum interval between backups.  We try to not create more than one backup
// per interval.
const BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS = 1;
// Maximum interval between backups.  If the last backup is older than these
// days we will try to create a new one more aggressively.
const BOOKMARKS_BACKUP_MAX_INTERVAL_DAYS = 3;
// Seconds of idle time before the late idle tasks will be scheduled.
const LATE_TASKS_IDLE_TIME_SEC = 20;
// Time after we stop tracking startup crashes.
const STARTUP_CRASHES_END_DELAY_MS = 30 * 1000;

/*
 * OS X has the concept of zero-window sessions and therefore ignores the
 * browser-lastwindow-close-* topics.
 */
const OBSERVE_LASTWINDOW_CLOSE_TOPICS = AppConstants.platform != "macosx";

function BrowserGlue() {
  XPCOMUtils.defineLazyServiceGetter(
    this,
    "_idleService",
    "@mozilla.org/widget/idleservice;1",
    "nsIIdleService"
  );

  XPCOMUtils.defineLazyGetter(this, "_distributionCustomizer", function() {
    const { DistributionCustomizer } = ChromeUtils.import(
      "resource:///modules/distribution.js"
    );
    return new DistributionCustomizer();
  });

  XPCOMUtils.defineLazyServiceGetter(
    this,
    "AlertsService",
    "@mozilla.org/alerts-service;1",
    "nsIAlertsService"
  );

  this._init();
}

BrowserGlue.prototype = {
  _saveSession: false,
  _migrationImportsDefaultBookmarks: false,
  _placesBrowserInitComplete: false,
  _isNewProfile: undefined,

  _setPrefToSaveSession: function BG__setPrefToSaveSession(aForce) {
    if (!this._saveSession && !aForce) {
      return;
    }

    if (!PrivateBrowsingUtils.permanentPrivateBrowsing) {
      Services.prefs.setBoolPref(
        "browser.sessionstore.resume_session_once",
        true
      );
    }

    // This method can be called via [NSApplication terminate:] on Mac, which
    // ends up causing prefs not to be flushed to disk, so we need to do that
    // explicitly here. See bug 497652.
    Services.prefs.savePrefFile(null);
  },

  _setSyncAutoconnectDelay: function BG__setSyncAutoconnectDelay() {
    // Assume that a non-zero value for services.sync.autoconnectDelay should override
    if (Services.prefs.prefHasUserValue("services.sync.autoconnectDelay")) {
      let prefDelay = Services.prefs.getIntPref(
        "services.sync.autoconnectDelay"
      );

      if (prefDelay > 0) {
        return;
      }
    }

    // delays are in seconds
    const MAX_DELAY = 300;
    let delay = 3;
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      // browser windows without a gBrowser almost certainly means we are
      // shutting down, so instead of just ignoring that window we abort.
      if (win.closed || !win.gBrowser) {
        return;
      }
      delay += win.gBrowser.tabs.length;
    }
    delay = delay <= MAX_DELAY ? delay : MAX_DELAY;

    const { Weave } = ChromeUtils.import("resource://services-sync/main.js");
    Weave.Service.scheduler.delayedAutoConnect(delay);
  },

  // nsIObserver implementation
  observe: async function BG_observe(subject, topic, data) {
    switch (topic) {
      case "notifications-open-settings":
        this._openPreferences("privacy-permissions");
        break;
      case "final-ui-startup":
        this._beforeUIStartup();
        break;
      case "browser-delayed-startup-finished":
        this._onFirstWindowLoaded(subject);
        Services.obs.removeObserver(this, "browser-delayed-startup-finished");
        break;
      case "sessionstore-windows-restored":
        this._onWindowsRestored();
        break;
      case "browser:purge-session-history":
        // reset the console service's error buffer
        Services.console.logStringMessage(null); // clear the console (in case it's open)
        Services.console.reset();
        break;
      case "restart-in-safe-mode":
        this._onSafeModeRestart();
        break;
      case "quit-application-requested":
        this._onQuitRequest(subject, data);
        break;
      case "quit-application-granted":
        this._onQuitApplicationGranted();
        break;
      case "browser-lastwindow-close-requested":
        if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
          // The application is not actually quitting, but the last full browser
          // window is about to be closed.
          this._onQuitRequest(subject, "lastwindow");
        }
        break;
      case "browser-lastwindow-close-granted":
        if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
          this._setPrefToSaveSession();
        }
        break;
      case "weave:service:ready":
        this._setSyncAutoconnectDelay();
        break;
      case "fxaccounts:onverified":
        this._showSyncStartedDoorhanger();
        break;
      case "fxaccounts:device_connected":
        this._onDeviceConnected(data);
        break;
      case "fxaccounts:verify_login":
        this._onVerifyLoginNotification(JSON.parse(data));
        break;
      case "fxaccounts:device_disconnected":
        data = JSON.parse(data);
        if (data.isLocalDevice) {
          this._onDeviceDisconnected();
        }
        break;
      case "fxaccounts:commands:open-uri":
      case "weave:engine:clients:display-uris":
        this._onDisplaySyncURIs(subject);
        break;
      case "session-save":
        this._setPrefToSaveSession(true);
        subject.QueryInterface(Ci.nsISupportsPRBool);
        subject.data = true;
        break;
      case "places-init-complete":
        Services.obs.removeObserver(this, "places-init-complete");
        if (!this._migrationImportsDefaultBookmarks) {
          this._initPlaces(false);
        }
        break;
      case "idle":
        this._backupBookmarks();
        break;
      case "distribution-customization-complete":
        Services.obs.removeObserver(
          this,
          "distribution-customization-complete"
        );
        // Customization has finished, we don't need the customizer anymore.
        delete this._distributionCustomizer;
        break;
      case "browser-glue-test": // used by tests
        if (data == "post-update-notification") {
          if (Services.prefs.prefHasUserValue("app.update.postupdate")) {
            this._showUpdateNotification();
          }
        } else if (data == "force-ui-migration") {
          this._migrateUI();
        } else if (data == "force-distribution-customization") {
          this._distributionCustomizer.applyCustomizations();
          // To apply distribution bookmarks use "places-init-complete".
        } else if (data == "force-places-init") {
          this._initPlaces(false);
        } else if (data == "mock-alerts-service") {
          Object.defineProperty(this, "AlertsService", {
            value: subject.wrappedJSObject,
          });
        } else if (data == "places-browser-init-complete") {
          if (this._placesBrowserInitComplete) {
            Services.obs.notifyObservers(null, "places-browser-init-complete");
          }
        } else if (data == "migrateMatchBucketsPrefForUI66") {
          this._migrateMatchBucketsPrefForUI66().then(() => {
            Services.obs.notifyObservers(
              null,
              "browser-glue-test",
              "migrateMatchBucketsPrefForUI66-done"
            );
          });
        }
        break;
      case "initial-migration-will-import-default-bookmarks":
        this._migrationImportsDefaultBookmarks = true;
        break;
      case "initial-migration-did-import-default-bookmarks":
        this._initPlaces(true);
        break;
      case "handle-xul-text-link":
        let linkHandled = subject.QueryInterface(Ci.nsISupportsPRBool);
        if (!linkHandled.data) {
          let win = BrowserWindowTracker.getTopWindow();
          if (win) {
            data = JSON.parse(data);
            let where = win.whereToOpenLink(data);
            // Preserve legacy behavior of non-modifier left-clicks
            // opening in a new selected tab.
            if (where == "current") {
              where = "tab";
            }
            win.openTrustedLinkIn(data.href, where);
            linkHandled.data = true;
          }
        }
        break;
      case "profile-before-change":
        // Any component depending on Places should be finalized in
        // _onPlacesShutdown.  Any component that doesn't need to act after
        // the UI has gone should be finalized in _onQuitApplicationGranted.
        this._dispose();
        break;
      case "keyword-search":
        // This notification is broadcast by the docshell when it "fixes up" a
        // URI that it's been asked to load into a keyword search.
        let engine = null;
        try {
          engine = subject.QueryInterface(Ci.nsISearchEngine);
        } catch (ex) {
          Cu.reportError(ex);
        }
        let win = BrowserWindowTracker.getTopWindow();
        break;
      case "browser-search-engine-modified":
        // Ensure we cleanup the hiddenOneOffs pref when removing
        // an engine, and that newly added engines are visible.
        if (data == "engine-added" || data == "engine-removed") {
          let engineName = subject.QueryInterface(Ci.nsISearchEngine).name;
          let pref = Services.prefs.getStringPref(
            "browser.search.hiddenOneOffs"
          );
          let hiddenList = pref ? pref.split(",") : [];
          hiddenList = hiddenList.filter(x => x !== engineName);
          Services.prefs.setStringPref(
            "browser.search.hiddenOneOffs",
            hiddenList.join(",")
          );
        }
        break;
      case "xpi-signature-changed":
        let disabledAddons = JSON.parse(data).disabled;
        let addons = await AddonManager.getAddonsByIDs(disabledAddons);
        if (addons.some(addon => addon)) {
          this._notifyUnsignedAddonsDisabled();
        }
        break;
      case "sync-ui-state:update":
        this._updateFxaBadges();
        break;
      case "handlersvc-store-initialized":
        // Initialize PdfJs when running in-process and remote. This only
        // happens once since PdfJs registers global hooks. If the PdfJs
        // extension is installed the init method below will be overridden
        // leaving initialization to the extension.
        // parent only: configure default prefs, set up pref observers, register
        // pdf content handler, and initializes parent side message manager
        // shim for privileged api access.
        PdfJs.init(this._isNewProfile);
        break;
    }
  },

  // initialization (called on application startup)
  _init: function BG__init() {
    let os = Services.obs;
    os.addObserver(this, "notifications-open-settings");
    os.addObserver(this, "final-ui-startup");
    os.addObserver(this, "browser-delayed-startup-finished");
    os.addObserver(this, "sessionstore-windows-restored");
    os.addObserver(this, "browser:purge-session-history");
    os.addObserver(this, "quit-application-requested");
    os.addObserver(this, "quit-application-granted");
    if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
      os.addObserver(this, "browser-lastwindow-close-requested");
      os.addObserver(this, "browser-lastwindow-close-granted");
    }
    os.addObserver(this, "weave:service:ready");
    os.addObserver(this, "fxaccounts:onverified");
    os.addObserver(this, "fxaccounts:device_connected");
    os.addObserver(this, "fxaccounts:verify_login");
    os.addObserver(this, "fxaccounts:device_disconnected");
    os.addObserver(this, "fxaccounts:commands:open-uri");
    os.addObserver(this, "weave:engine:clients:display-uris");
    os.addObserver(this, "session-save");
    os.addObserver(this, "places-init-complete");
    os.addObserver(this, "distribution-customization-complete");
    os.addObserver(this, "handle-xul-text-link");
    os.addObserver(this, "profile-before-change");
    os.addObserver(this, "keyword-search");
    os.addObserver(this, "browser-search-engine-modified");
    os.addObserver(this, "restart-in-safe-mode");
    os.addObserver(this, "xpi-signature-changed");
    os.addObserver(this, "sync-ui-state:update");
    os.addObserver(this, "handlersvc-store-initialized");

    ActorManagerParent.addActors(ACTORS);
    ActorManagerParent.addLegacyActors(LEGACY_ACTORS);
    ActorManagerParent.flush();

    this._firstWindowReady = new Promise(
      resolve => (this._firstWindowLoaded = resolve)
    );
    if (AppConstants.platform == "win") {
      JawsScreenReaderVersionCheck.init();
    }
  },

  // cleanup (called on application shutdown)
  _dispose: function BG__dispose() {
    let os = Services.obs;
    os.removeObserver(this, "notifications-open-settings");
    os.removeObserver(this, "final-ui-startup");
    os.removeObserver(this, "sessionstore-windows-restored");
    os.removeObserver(this, "browser:purge-session-history");
    os.removeObserver(this, "quit-application-requested");
    os.removeObserver(this, "quit-application-granted");
    os.removeObserver(this, "restart-in-safe-mode");
    if (OBSERVE_LASTWINDOW_CLOSE_TOPICS) {
      os.removeObserver(this, "browser-lastwindow-close-requested");
      os.removeObserver(this, "browser-lastwindow-close-granted");
    }
    os.removeObserver(this, "weave:service:ready");
    os.removeObserver(this, "fxaccounts:onverified");
    os.removeObserver(this, "fxaccounts:device_connected");
    os.removeObserver(this, "fxaccounts:verify_login");
    os.removeObserver(this, "fxaccounts:device_disconnected");
    os.removeObserver(this, "fxaccounts:commands:open-uri");
    os.removeObserver(this, "weave:engine:clients:display-uris");
    os.removeObserver(this, "session-save");
    if (this._bookmarksBackupIdleTime) {
      this._idleService.removeIdleObserver(this, this._bookmarksBackupIdleTime);
      delete this._bookmarksBackupIdleTime;
    }
    if (this._lateTasksIdleObserver) {
      this._idleService.removeIdleObserver(
        this._lateTasksIdleObserver,
        LATE_TASKS_IDLE_TIME_SEC
      );
      delete this._lateTasksIdleObserver;
    }
    if (this._gmpInstallManager) {
      this._gmpInstallManager.uninit();
      delete this._gmpInstallManager;
    }
    try {
      os.removeObserver(this, "places-init-complete");
    } catch (ex) {
      /* Could have been removed already */
    }
    os.removeObserver(this, "handle-xul-text-link");
    os.removeObserver(this, "profile-before-change");
    os.removeObserver(this, "keyword-search");
    os.removeObserver(this, "browser-search-engine-modified");
    os.removeObserver(this, "xpi-signature-changed");
    os.removeObserver(this, "sync-ui-state:update");
  },

  // runs on startup, before the first command line handler is invoked
  // (i.e. before the first window is opened)
  _beforeUIStartup: function BG__beforeUIStartup() {
    SessionStartup.init();

    // check if we're in safe mode
    if (Services.appinfo.inSafeMode) {
      Services.ww.openWindow(
        null,
        "chrome://browser/content/safeMode.xhtml",
        "_blank",
        "chrome,centerscreen,modal,resizable=no",
        null
      );
    }

    // apply distribution customizations
    this._distributionCustomizer.applyCustomizations();

    // handle any UI migration
    this._migrateUI();

    if (!Services.prefs.prefHasUserValue(PREF_PDFJS_ISDEFAULT_CACHE_STATE)) {
      PdfJs.checkIsDefault(this._isNewProfile);
    }

    listeners.init();

    SessionStore.init();

    AddonManager.maybeInstallBuiltinAddon(
      "firefox-compact-light@mozilla.org",
      "1.0",
      "resource:///modules/themes/light/"
    );
    AddonManager.maybeInstallBuiltinAddon(
      "firefox-compact-dark@mozilla.org",
      "1.0",
      "resource:///modules/themes/dark/"
    );

    Services.obs.notifyObservers(null, "browser-ui-startup-complete");
  },

  _checkForOldBuildUpdates() {
    // check for update if our build is old
    if (
      AppConstants.MOZ_UPDATER &&
      Services.prefs.getBoolPref("app.update.checkInstallTime")
    ) {
      let buildID = Services.appinfo.appBuildID;
      let today = new Date().getTime();
      /* eslint-disable no-multi-spaces */
      let buildDate = new Date(
        buildID.slice(0, 4), // year
        buildID.slice(4, 6) - 1, // months are zero-based.
        buildID.slice(6, 8), // day
        buildID.slice(8, 10), // hour
        buildID.slice(10, 12), // min
        buildID.slice(12, 14)
      ) // ms
        .getTime();
      /* eslint-enable no-multi-spaces */

      const millisecondsIn24Hours = 86400000;
      let acceptableAge =
        Services.prefs.getIntPref("app.update.checkInstallTime.days") *
        millisecondsIn24Hours;

      if (buildDate + acceptableAge < today) {
        Cc["@mozilla.org/updates/update-service;1"]
          .getService(Ci.nsIApplicationUpdateService)
          .checkForBackgroundUpdates();
      }
    }
  },

  _onSafeModeRestart: function BG_onSafeModeRestart() {
    // prompt the user to confirm
    let strings = gBrowserBundle;
    let promptTitle = strings.GetStringFromName("safeModeRestartPromptTitle");
    let promptMessage = strings.GetStringFromName(
      "safeModeRestartPromptMessage"
    );
    let restartText = strings.GetStringFromName("safeModeRestartButton");
    let buttonFlags =
      Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
      Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL +
      Services.prompt.BUTTON_POS_0_DEFAULT;

    let rv = Services.prompt.confirmEx(
      null,
      promptTitle,
      promptMessage,
      buttonFlags,
      restartText,
      null,
      null,
      null,
      {}
    );
    if (rv != 0) {
      return;
    }

    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );

    if (!cancelQuit.data) {
      Services.startup.restartInSafeMode(Ci.nsIAppStartup.eAttemptQuit);
    }
  },

  _trackSlowStartup() {
    if (
      Services.startup.interrupted ||
      Services.prefs.getBoolPref("browser.slowStartup.notificationDisabled")
    ) {
      return;
    }

    let currentTime = Date.now() - Services.startup.getStartupInfo().process;
    let averageTime = 0;
    let samples = 0;
    try {
      averageTime = Services.prefs.getIntPref(
        "browser.slowStartup.averageTime"
      );
      samples = Services.prefs.getIntPref("browser.slowStartup.samples");
    } catch (e) {}

    let totalTime = averageTime * samples + currentTime;
    samples++;
    averageTime = totalTime / samples;

    if (
      samples >= Services.prefs.getIntPref("browser.slowStartup.maxSamples")
    ) {
      if (
        averageTime >
        Services.prefs.getIntPref("browser.slowStartup.timeThreshold")
      ) {
        this._calculateProfileAgeInDays().then(
          this._showSlowStartupNotification,
          null
        );
      }
      averageTime = 0;
      samples = 0;
    }

    Services.prefs.setIntPref("browser.slowStartup.averageTime", averageTime);
    Services.prefs.setIntPref("browser.slowStartup.samples", samples);
  },

  async _calculateProfileAgeInDays() {
    let ProfileAge = ChromeUtils.import(
      "resource://gre/modules/ProfileAge.jsm",
      {}
    ).ProfileAge;
    let profileAge = await ProfileAge();

    let creationDate = await profileAge.created;
    let resetDate = await profileAge.reset;

    // if the profile was reset, consider the
    // reset date for its age.
    let profileDate = resetDate || creationDate;

    const ONE_DAY = 24 * 60 * 60 * 1000;
    return (Date.now() - profileDate) / ONE_DAY;
  },

  _showSlowStartupNotification(profileAge) {
    if (profileAge < 90) {
      // 3 months
      return;
    }

    let win = BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }

    let productName = gBrandBundle.GetStringFromName("brandFullName");
    let message = win.gNavigatorBundle.getFormattedString(
      "slowStartup.message",
      [productName]
    );

    let buttons = [
      {
        label: win.gNavigatorBundle.getString("slowStartup.helpButton.label"),
        accessKey: win.gNavigatorBundle.getString(
          "slowStartup.helpButton.accesskey"
        ),
        callback() {
          win.openTrustedLinkIn(
            "https://support.mozilla.org/kb/reset-firefox-easily-fix-most-problems",
            "tab"
          );
        },
      },
      {
        label: win.gNavigatorBundle.getString(
          "slowStartup.disableNotificationButton.label"
        ),
        accessKey: win.gNavigatorBundle.getString(
          "slowStartup.disableNotificationButton.accesskey"
        ),
        callback() {
          Services.prefs.setBoolPref(
            "browser.slowStartup.notificationDisabled",
            true
          );
        },
      },
    ];

    win.gNotificationBox.appendNotification(
      message,
      "slow-startup",
      "chrome://browser/skin/slowStartup-16.png",
      win.gNotificationBox.PRIORITY_INFO_LOW,
      buttons
    );
  },

  /**
   * Show a notification bar offering a reset.
   *
   * @param reason
   *        String of either "unused" or "uninstall", specifying the reason
   *        why a profile reset is offered.
   */
  _resetProfileNotification(reason) {
    let win = BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }

    const { ResetProfile } = ChromeUtils.import(
      "resource://gre/modules/ResetProfile.jsm"
    );
    if (!ResetProfile.resetSupported()) {
      return;
    }

    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let resetBundle = Services.strings.createBundle(
      "chrome://global/locale/resetProfile.properties"
    );

    let message;
    if (reason == "unused") {
      message = resetBundle.formatStringFromName(
        "resetUnusedProfile.message",
        [productName]
      );
    } else if (reason == "uninstall") {
      message = resetBundle.formatStringFromName(
        "resetUninstalled.message",
        [productName]
      );
    } else {
      throw new Error(
        `Unknown reason (${reason}) given to _resetProfileNotification.`
      );
    }
    let buttons = [
      {
        label: resetBundle.formatStringFromName(
          "refreshProfile.resetButton.label",
          [productName]
        ),
        accessKey: resetBundle.GetStringFromName(
          "refreshProfile.resetButton.accesskey"
        ),
        callback() {
          ResetProfile.openConfirmationDialog(win);
        },
      },
    ];

    win.gNotificationBox.appendNotification(
      message,
      "reset-profile-notification",
      "chrome://global/skin/icons/question-16.png",
      win.gNotificationBox.PRIORITY_INFO_LOW,
      buttons
    );
  },

  _notifyUnsignedAddonsDisabled() {
    let win = BrowserWindowTracker.getTopWindow();
    if (!win) {
      return;
    }

    let message = win.gNavigatorBundle.getString(
      "unsignedAddonsDisabled.message"
    );
    let buttons = [
      {
        label: win.gNavigatorBundle.getString(
          "unsignedAddonsDisabled.learnMore.label"
        ),
        accessKey: win.gNavigatorBundle.getString(
          "unsignedAddonsDisabled.learnMore.accesskey"
        ),
        callback() {
          win.BrowserOpenAddonsMgr("addons://list/extension?unsigned=true");
        },
      },
    ];

    win.gHighPriorityNotificationBox.appendNotification(
      message,
      "unsigned-addons-disabled",
      "",
      win.gHighPriorityNotificationBox.PRIORITY_WARNING_MEDIUM,
      buttons
    );
  },

  // the first browser window has finished initializing
  _onFirstWindowLoaded: function BG__onFirstWindowLoaded(aWindow) {
    TabCrashHandler.init();
    if (AppConstants.MOZ_CRASHREPORTER) {
      PluginCrashReporter.init();
    }

    ProcessHangMonitor.init();

    // A channel for "remote troubleshooting" code...
    let channel = new WebChannel(
      "remote-troubleshooting",
      "remote-troubleshooting"
    );
    channel.listen((id, data, target) => {
      if (data.command == "request") {
        let { Troubleshoot } = ChromeUtils.import(
          "resource://gre/modules/Troubleshoot.jsm"
        );
        Troubleshoot.snapshot(snapshotData => {
          // for privacy we remove crash IDs and all preferences (but bug 1091944
          // exists to expose prefs once we are confident of privacy implications)
          delete snapshotData.crashes;
          delete snapshotData.modifiedPreferences;
          channel.send(snapshotData, target);
        });
      }
    });

    this._trackSlowStartup();

    // Offer to reset a user's profile if it hasn't been used for 60 days.
    const OFFER_PROFILE_RESET_INTERVAL_MS = 60 * 24 * 60 * 60 * 1000;
    let lastUse = Services.appinfo.replacedLockTime;
    let disableResetPrompt = Services.prefs.getBoolPref(
      "browser.disableResetPrompt",
      false
    );

    if (
      !disableResetPrompt &&
      lastUse &&
      Date.now() - lastUse >= OFFER_PROFILE_RESET_INTERVAL_MS
    ) {
      this._resetProfileNotification("unused");
    } else if (AppConstants.platform == "win" && !disableResetPrompt) {
      // Check if we were just re-installed and offer Firefox Reset
      let updateChannel;
      try {
        updateChannel = ChromeUtils.import(
          "resource://gre/modules/UpdateUtils.jsm",
          {}
        ).UpdateUtils.UpdateChannel;
      } catch (ex) {}
      if (updateChannel) {
        let uninstalledValue = WindowsRegistry.readRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          "Software\\Mozilla\\Firefox",
          `Uninstalled-${updateChannel}`
        );
        let removalSuccessful = WindowsRegistry.removeRegKey(
          Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          "Software\\Mozilla\\Firefox",
          `Uninstalled-${updateChannel}`
        );
        if (removalSuccessful && uninstalledValue == "True") {
          this._resetProfileNotification("uninstall");
        }
      }
    }

    this._checkForOldBuildUpdates();

    AutoCompletePopup.init();
    DateTimePickerParent.init();
    // Check if Sync is configured
    if (Services.prefs.prefHasUserValue("services.sync.username")) {
      WeaveService.init();
    }

    PageThumbs.init();

    NewTabUtils.init();

    AboutNetErrorHandler.init();

    AboutPrivateBrowsingHandler.init();

    PageActions.init();

    this._firstWindowLoaded();

    // Set the default favicon size for UI views that use the page-icon protocol.
    PlacesUtils.favicons.setDefaultIconURIPreferredSize(
      16 * aWindow.devicePixelRatio
    );

    // This observes the entire privacy.trackingprotection.* pref tree.
    Services.prefs.addObserver(
      "media.autoplay.default",
      this._updateAutoplayPref
    );
  },

  _updateAutoplayPref() {
    let blocked = Services.prefs.getIntPref("media.autoplay.default", 1);
  },

  /**
   * Application shutdown handler.
   */
  _onQuitApplicationGranted() {
    // This pref must be set here because SessionStore will use its value
    // on quit-application.
    this._setPrefToSaveSession();

    // Call trackStartupCrashEnd here in case the delayed call on startup hasn't
    // yet occurred (see trackStartupCrashEnd caller in browser.js).
    try {
      Services.startup.trackStartupCrashEnd();
    } catch (e) {
      Cu.reportError(
        "Could not end startup crash tracking in quit-application-granted: " + e
      );
    }

    if (this._bookmarksBackupIdleTime) {
      this._idleService.removeIdleObserver(this, this._bookmarksBackupIdleTime);
      delete this._bookmarksBackupIdleTime;
    }

    for (let mod of Object.values(initializedModules)) {
      if (mod.uninit) {
        mod.uninit();
      }
    }

    PageThumbs.uninit();
    NewTabUtils.uninit();
    AboutNetErrorHandler.uninit();
    AboutPrivateBrowsingHandler.uninit();
    AutoCompletePopup.uninit();
    DateTimePickerParent.uninit();

    RFPHelper.uninit();
  },

  _showNewInstallModal() {
    // Allow other observers of the same topic to run while we open the dialog.
    Services.tm.dispatchToMainThread(() => {
      let win = BrowserWindowTracker.getTopWindow();

      let stack = win.gBrowser.getPanel().querySelector(".browserStack");
      let mask = win.document.createElementNS(XULNS, "box");
      mask.setAttribute("id", "content-mask");
      stack.appendChild(mask);

      Services.ww.openWindow(
        win,
        "chrome://browser/content/newInstall.xhtml",
        "_blank",
        "chrome,modal,resizable=no,centerscreen",
        null
      );
      mask.remove();
    });
  },

  // All initial windows have opened.
  _onWindowsRestored: function BG__onWindowsRestored() {
    if (this._windowsWereRestored) {
      return;
    }
    this._windowsWereRestored = true;

    // Show update notification, if needed.
    if (Services.prefs.prefHasUserValue("app.update.postupdate")) {
      this._showUpdateNotification();
    }

    ExtensionsUI.init();

    if (AppConstants.MOZ_CRASHREPORTER) {
      UnsubmittedCrashHandler.init();
    }

    Sanitizer.onStartup();
    this._scheduleStartupIdleTasks();
    this._lateTasksIdleObserver = (idleService, topic, data) => {
      if (topic == "idle") {
        idleService.removeIdleObserver(
          this._lateTasksIdleObserver,
          LATE_TASKS_IDLE_TIME_SEC
        );
        delete this._lateTasksIdleObserver;
        this._scheduleArbitrarilyLateIdleTasks();
      }
    };
    this._idleService.addIdleObserver(
      this._lateTasksIdleObserver,
      LATE_TASKS_IDLE_TIME_SEC
    );

    let pService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
      Ci.nsIToolkitProfileService
    );
    if (pService.createdAlternateProfile) {
      this._showNewInstallModal();
    }
  },

  /**
   * Use this function as an entry point to schedule tasks that
   * need to run only once after startup, and can be scheduled
   * by using an idle callback.
   *
   * The functions scheduled here will fire from idle callbacks
   * once every window has finished being restored by session
   * restore, and it's guaranteed that they will run before
   * the equivalent per-window idle tasks
   * (from _schedulePerWindowIdleTasks in browser.js).
   *
   * If you have something that can wait even further than the
   * per-window initialization, please schedule them using
   * _scheduleArbitrarilyLateIdleTasks.
   * Don't be fooled by thinking that the use of the timeout parameter
   * will delay your function: it will just ensure that it potentially
   * happens _earlier_ than expected (when the timeout limit has been reached),
   * but it will not make it happen later (and out of order) compared
   * to the other ones scheduled together.
   */
  _scheduleStartupIdleTasks() {
    Services.tm.idleDispatchToMainThread(async () => {
      await ContextualIdentityService.load();
    });

    // Load the Login Manager data from disk off the main thread, some time
    // after startup.  If the data is required before this runs, for example
    // because a restored page contains a password field, it will be loaded on
    // the main thread, and this initialization request will be ignored.
    Services.tm.idleDispatchToMainThread(() => {
      try {
        Services.logins;
      } catch (ex) {
        Cu.reportError(ex);
      }
    }, 3000);

    if (AppConstants.MOZ_CRASHREPORTER) {
      UnsubmittedCrashHandler.scheduleCheckForUnsubmittedCrashReports();
    }

    if (AppConstants.ASAN_REPORTER) {
      var { AsanReporter } = ChromeUtils.import(
        "resource:///modules/AsanReporter.jsm"
      );
      AsanReporter.init();
    }

    if (AppConstants.platform == "win") {
      Services.tm.idleDispatchToMainThread(() => {
        // For Windows 7, initialize the jump list module.
        const WINTASKBAR_CONTRACTID = "@mozilla.org/windows-taskbar;1";
        if (
          WINTASKBAR_CONTRACTID in Cc &&
          Cc[WINTASKBAR_CONTRACTID].getService(Ci.nsIWinTaskbar).available
        ) {
          let temp = {};
          ChromeUtils.import("resource:///modules/WindowsJumpLists.jsm", temp);
          temp.WinTaskbarJumpList.startup();
        }
      });
    }

    Services.tm.idleDispatchToMainThread(() => {
      this._checkForDefaultBrowser();
    });

    Services.tm.idleDispatchToMainThread(() => {
      let { setTimeout } = ChromeUtils.import(
        "resource://gre/modules/Timer.jsm"
      );
      setTimeout(function() {
        Services.tm.idleDispatchToMainThread(
          Services.startup.trackStartupCrashEnd
        );
      }, STARTUP_CRASHES_END_DELAY_MS);
    });

    Services.tm.idleDispatchToMainThread(() => {
      let handlerService = Cc[
        "@mozilla.org/uriloader/handler-service;1"
      ].getService(Ci.nsIHandlerService);
      handlerService.asyncInit();
    });

    if (AppConstants.platform == "win") {
      Services.tm.idleDispatchToMainThread(() => {
        JawsScreenReaderVersionCheck.onWindowsRestored();
      });
    }

    Services.tm.idleDispatchToMainThread(() => {
      RFPHelper.init();
    });

    ChromeUtils.idleDispatch(() => {
      Blocklist.loadBlocklistAsync();
    });

    if (
      Services.prefs.getIntPref(
        "browser.livebookmarks.migrationAttemptsLeft",
        0
      ) > 0
    ) {
      Services.tm.idleDispatchToMainThread(() => {
        LiveBookmarkMigrator.migrate().catch(Cu.reportError);
      });
    }

    Services.tm.idleDispatchToMainThread(() => {
      TabUnloader.init();
    });

    // Marionette needs to be initialized as very last step
    Services.tm.idleDispatchToMainThread(() => {
      Services.obs.notifyObservers(null, "marionette-startup-requested");
    });
  },

  /**
   * Use this function as an entry point to schedule tasks that need
   * to run once per session, at any arbitrary point in time.
   * This function will be called from an idle observer. Check the value of
   * LATE_TASKS_IDLE_TIME_SEC to see the current value for this idle
   * observer.
   *
   * Note: this function may never be called if the user is never idle for the
   * full length of the period of time specified. But given a reasonably low
   * value, this is unlikely.
   */
  _scheduleArbitrarilyLateIdleTasks() {
    Services.tm.idleDispatchToMainThread(() => {
      let obj = {};
      ChromeUtils.import("resource://gre/modules/GMPInstallManager.jsm", obj);
      this._gmpInstallManager = new obj.GMPInstallManager();
      // We don't really care about the results, if someone is interested they
      // can check the log.
      this._gmpInstallManager.simpleCheckAndInstall().catch(() => {});
    });
  },

  _onQuitRequest: function BG__onQuitRequest(aCancelQuit, aQuitType) {
    // If user has already dismissed quit request, then do nothing
    if (aCancelQuit instanceof Ci.nsISupportsPRBool && aCancelQuit.data) {
      return;
    }

    // There are several cases where we won't show a dialog here:
    // 1. There is only 1 tab open in 1 window
    // 2. browser.warnOnQuit == false
    // 3. The browser is currently in Private Browsing mode
    // 4. The browser will be restarted.
    // 5. The user has automatic session restore enabled and
    //    browser.sessionstore.warnOnQuit is not set to true.
    // 6. The user doesn't have automatic session restore enabled
    //    and browser.tabs.warnOnClose is not set to true.
    //
    // Otherwise, we will show the "closing multiple tabs" dialog.
    //
    // aQuitType == "lastwindow" is overloaded. "lastwindow" is used to indicate
    // "the last window is closing but we're not quitting (a non-browser window is open)"
    // and also "we're quitting by closing the last window".

    if (aQuitType == "restart" || aQuitType == "os-restart") {
      return;
    }

    var windowcount = 0;
    var pagecount = 0;
    const prfwrn = "browser.tabs.warnOnClose";
    for (let win of BrowserWindowTracker.orderedWindows) {
      if (win.closed) {
        continue;
      }
      windowcount++;
      let tabbrowser = win.gBrowser;
      if (tabbrowser) {
        pagecount +=
          tabbrowser.browsers.length -
          tabbrowser._numPinnedTabs -
          tabbrowser._removingTabs.length;
      }
    }

    if (!aQuitType) {
      aQuitType = "quit";
    }

    // browser.warnOnQuit is a hidden global boolean to override all quit prompts
    if (!Services.prefs.getBoolPref("browser.warnOnQuit")) {
      return;
    }

    // If we're going to automatically restore the session, only warn if the user asked for that.
    let sessionWillBeRestored =
      Services.prefs.getIntPref("browser.startup.page") == 3 ||
      Services.prefs.getBoolPref("browser.sessionstore.resume_session_once");

    // In the sessionWillBeRestored case, we only check the sessionstore-specific pref:
    if (sessionWillBeRestored) {
      if (
        !Services.prefs.getBoolPref("browser.sessionstore.warnOnQuit", false)
      ) {
        return;
      }
      // Otherwise, we check browser.tabs.warnOnClose
    } else {
      var p = Services.prefs.getIntPref(prfwrn);
      if (p == 0 || p > pagecount) return;
    }

    let win = BrowserWindowTracker.getTopWindow();

    let warningMessage;
    // More than 1 window. Compose our own message.
    if (windowcount > 1) {
      let tabSubstring = gTabbrowserBundle.GetStringFromName(
        "tabs.closeWarningMultipleWindowsTabSnippet"
      );
      tabSubstring = PluralForm.get(pagecount, tabSubstring).replace(
        /#1/,
        pagecount
      );

      let stringID = sessionWillBeRestored
        ? "tabs.closeWarningMultipleWindowsSessionRestore2"
        : "tabs.closeWarningMultipleWindows";
      let windowString = gTabbrowserBundle.GetStringFromName(stringID);
      windowString = PluralForm.get(windowcount, windowString).replace(
        /#1/,
        windowcount
      );
      warningMessage = windowString.replace(/%(?:1\$)?S/i, tabSubstring);
    } else {
      let stringID = sessionWillBeRestored
        ? "tabs.closeWarningMultipleSessionRestore2"
        : "tabs.closeWarningMultiple";
      warningMessage = gTabbrowserBundle.GetStringFromName(stringID);
      warningMessage = PluralForm.get(pagecount, warningMessage).replace(
        "#1",
        pagecount
      );
    }

    let warnOnClose = { value: true };
    let titleId =
      AppConstants.platform == "win"
        ? "tabs.closeAndQuitTitleTabsWin"
        : "tabs.closeAndQuitTitleTabs";
    let flags =
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
      Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1;
    // Only display the checkbox in the non-sessionrestore case.
    let checkboxLabel = !sessionWillBeRestored
      ? gTabbrowserBundle.GetStringFromName("tabs.closeWarningPromptMe")
      : null;

    // buttonPressed will be 0 for closing, 1 for cancel (don't close/quit)
    let buttonPressed = Services.prompt.confirmEx(
      win,
      gTabbrowserBundle.GetStringFromName(titleId),
      warningMessage,
      flags,
      gTabbrowserBundle.GetStringFromName("tabs.closeButtonMultiple"),
      null,
      null,
      checkboxLabel,
      warnOnClose
    );
    // If the user has unticked the box, and has confirmed closing, stop showing
    // the warning.
    if (!sessionWillBeRestored && buttonPressed == 0 && !warnOnClose.value) {
      Services.prefs.setIntPref(prfwrn, 0);
    }
    aCancelQuit.data = buttonPressed != 0;
  },

  _showUpdateNotification: function BG__showUpdateNotification() {
    Services.prefs.clearUserPref("app.update.postupdate");

    var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    try {
      // If the updates.xml file is deleted then getUpdateAt will throw.
      var update = um.getUpdateAt(0).QueryInterface(Ci.nsIWritablePropertyBag);
    } catch (e) {
      // This should never happen.
      Cu.reportError("Unable to find update: " + e);
      return;
    }

    var actions = update.getProperty("actions");
    if (!actions || actions.includes("silent")) {
      return;
    }

    var appName = gBrandBundle.GetStringFromName("brandShortName");

    function getNotifyString(aPropData) {
      var propValue = update.getProperty(aPropData.propName);
      if (!propValue) {
        if (aPropData.prefName) {
          propValue = Services.urlFormatter.formatURLPref(aPropData.prefName);
        } else if (aPropData.stringParams) {
          propValue = gBrowserBundle.formatStringFromName(
            aPropData.stringName,
            aPropData.stringParams,
            aPropData.stringParams.length
          );
        } else {
          propValue = gBrowserBundle.GetStringFromName(aPropData.stringName);
        }
      }
      return propValue;
    }

    if (actions.includes("showNotification")) {
      let text = getNotifyString({
        propName: "notificationText",
        stringName: "puNotifyText",
        stringParams: [appName],
      });
      let url = getNotifyString({
        propName: "notificationURL",
        prefName: "startup.homepage_override_url",
      });
      let label = getNotifyString({
        propName: "notificationButtonLabel",
        stringName: "pu.notifyButton.label",
      });
      let key = getNotifyString({
        propName: "notificationButtonAccessKey",
        stringName: "pu.notifyButton.accesskey",
      });

      let win = BrowserWindowTracker.getTopWindow();

      let buttons = [
        {
          label,
          accessKey: key,
          popup: null,
          callback(aNotificationBar, aButton) {
            win.openTrustedLinkIn(url, "tab");
          },
        },
      ];

      win.gHighPriorityNotificationBox.appendNotification(
        text,
        "post-update-notification",
        null,
        win.gHighPriorityNotificationBox.PRIORITY_INFO_LOW,
        buttons
      );
    }

    if (!actions.includes("showAlert")) {
      return;
    }

    let title = getNotifyString({
      propName: "alertTitle",
      stringName: "puAlertTitle",
      stringParams: [appName],
    });
    let text = getNotifyString({
      propName: "alertText",
      stringName: "puAlertText",
      stringParams: [appName],
    });
    let url = getNotifyString({
      propName: "alertURL",
      prefName: "startup.homepage_override_url",
    });

    function clickCallback(subject, topic, data) {
      // This callback will be called twice but only once with this topic
      if (topic != "alertclickcallback") {
        return;
      }
      let win = BrowserWindowTracker.getTopWindow();
      win.openTrustedLinkIn(data, "tab");
    }

    try {
      // This will throw NS_ERROR_NOT_AVAILABLE if the notification cannot
      // be displayed per the idl.
      this.AlertsService.showAlertNotification(
        null,
        title,
        text,
        true,
        url,
        clickCallback
      );
    } catch (e) {
      Cu.reportError(e);
    }
  },

  /**
   * Initialize Places
   * - imports the bookmarks html file if bookmarks database is empty, try to
   *   restore bookmarks from a JSON backup if the backend indicates that the
   *   database was corrupt.
   *
   * These prefs can be set up by the frontend:
   *
   * WARNING: setting these preferences to true will overwite existing bookmarks
   *
   * - browser.places.importBookmarksHTML
   *   Set to true will import the bookmarks.html file from the profile folder.
   * - browser.bookmarks.restore_default_bookmarks
   *   Set to true by safe-mode dialog to indicate we must restore default
   *   bookmarks.
   */
  _initPlaces: function BG__initPlaces(aInitialMigrationPerformed) {
    // We must instantiate the history service since it will tell us if we
    // need to import or restore bookmarks due to first-run, corruption or
    // forced migration (due to a major schema change).
    // If the database is corrupt or has been newly created we should
    // import bookmarks.
    let dbStatus = PlacesUtils.history.databaseStatus;

    // Show a notification with a "more info" link for a locked places.sqlite.
    if (dbStatus == PlacesUtils.history.DATABASE_STATUS_LOCKED) {
      // Note: initPlaces should always happen when the first window is ready,
      // in any case, better safe than sorry.
      this._firstWindowReady.then(() => {
        this._showPlacesLockedNotificationBox();
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
      return;
    }

    let importBookmarks =
      !aInitialMigrationPerformed &&
      (dbStatus == PlacesUtils.history.DATABASE_STATUS_CREATE ||
        dbStatus == PlacesUtils.history.DATABASE_STATUS_CORRUPT);

    // Check if user or an extension has required to import bookmarks.html
    let importBookmarksHTML = false;
    try {
      importBookmarksHTML = Services.prefs.getBoolPref(
        "browser.places.importBookmarksHTML"
      );
      if (importBookmarksHTML) {
        importBookmarks = true;
      }
    } catch (ex) {}

    // Support legacy bookmarks.html format for apps that depend on that format.
    let autoExportHTML = Services.prefs.getBoolPref(
      "browser.bookmarks.autoExportHTML",
      false
    ); // Do not export.
    if (autoExportHTML) {
      // Sqlite.jsm and Places shutdown happen at profile-before-change, thus,
      // to be on the safe side, this should run earlier.
      AsyncShutdown.profileChangeTeardown.addBlocker(
        "Places: export bookmarks.html",
        () => BookmarkHTMLUtils.exportToFile(BookmarkHTMLUtils.defaultPath)
      );
    }

    (async () => {
      // Check if Safe Mode or the user has required to restore bookmarks from
      // default profile's bookmarks.html
      let restoreDefaultBookmarks = false;
      try {
        restoreDefaultBookmarks = Services.prefs.getBoolPref(
          "browser.bookmarks.restore_default_bookmarks"
        );
        if (restoreDefaultBookmarks) {
          // Ensure that we already have a bookmarks backup for today.
          await this._backupBookmarks();
          importBookmarks = true;
        }
      } catch (ex) {}

      // This may be reused later, check for "=== undefined" to see if it has
      // been populated already.
      let lastBackupFile;

      // If the user did not require to restore default bookmarks, or import
      // from bookmarks.html, we will try to restore from JSON
      if (importBookmarks && !restoreDefaultBookmarks && !importBookmarksHTML) {
        // get latest JSON backup
        lastBackupFile = await PlacesBackups.getMostRecentBackup();
        if (lastBackupFile) {
          // restore from JSON backup
          await BookmarkJSONUtils.importFromFile(lastBackupFile, {
            replace: true,
            source: PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
          });
          importBookmarks = false;
        } else {
          // We have created a new database but we don't have any backup available
          importBookmarks = true;
          if (await OS.File.exists(BookmarkHTMLUtils.defaultPath)) {
            // If bookmarks.html is available in current profile import it...
            importBookmarksHTML = true;
          } else {
            // ...otherwise we will restore defaults
            restoreDefaultBookmarks = true;
          }
        }
      }

      // Import default bookmarks when necessary.
      // Otherwise, if any kind of import runs, default bookmarks creation should be
      // delayed till the import operations has finished.  Not doing so would
      // cause them to be overwritten by the newly imported bookmarks.
      if (!importBookmarks) {
        // Now apply distribution customized bookmarks.
        // This should always run after Places initialization.
        try {
          await this._distributionCustomizer.applyBookmarks();
        } catch (e) {
          Cu.reportError(e);
        }
      } else {
        // An import operation is about to run.
        let bookmarksUrl = null;
        if (restoreDefaultBookmarks) {
          // User wants to restore bookmarks.html file from default profile folder
          bookmarksUrl = "chrome://browser/locale/bookmarks.html";
        } else if (await OS.File.exists(BookmarkHTMLUtils.defaultPath)) {
          bookmarksUrl = OS.Path.toFileURI(BookmarkHTMLUtils.defaultPath);
        }

        if (bookmarksUrl) {
          // Import from bookmarks.html file.
          try {
            if (Services.policies.isAllowed("defaultBookmarks")) {
              await BookmarkHTMLUtils.importFromURL(bookmarksUrl, {
                replace: true,
                source: PlacesUtils.bookmarks.SOURCES.RESTORE_ON_STARTUP,
              });
            }
          } catch (e) {
            Cu.reportError("Bookmarks.html file could be corrupt. " + e);
          }
          try {
            // Now apply distribution customized bookmarks.
            // This should always run after Places initialization.
            await this._distributionCustomizer.applyBookmarks();
          } catch (e) {
            Cu.reportError(e);
          }
        } else {
          Cu.reportError(new Error("Unable to find bookmarks.html file."));
        }

        // Reset preferences, so we won't try to import again at next run
        if (importBookmarksHTML) {
          Services.prefs.setBoolPref(
            "browser.places.importBookmarksHTML",
            false
          );
        }
        if (restoreDefaultBookmarks) {
          Services.prefs.setBoolPref(
            "browser.bookmarks.restore_default_bookmarks",
            false
          );
        }
      }

      // Initialize bookmark archiving on idle.
      if (!this._bookmarksBackupIdleTime) {
        this._bookmarksBackupIdleTime = BOOKMARKS_BACKUP_IDLE_TIME_SEC;

        // If there is no backup, or the last bookmarks backup is too old, use
        // a more aggressive idle observer.
        if (lastBackupFile === undefined) {
          lastBackupFile = await PlacesBackups.getMostRecentBackup();
        }
        if (!lastBackupFile) {
          this._bookmarksBackupIdleTime /= 2;
        } else {
          let lastBackupTime = PlacesBackups.getDateForFile(lastBackupFile);
          let profileLastUse = Services.appinfo.replacedLockTime || Date.now();

          // If there is a backup after the last profile usage date it's fine,
          // regardless its age.  Otherwise check how old is the last
          // available backup compared to that session.
          if (profileLastUse > lastBackupTime) {
            let backupAge = Math.round(
              (profileLastUse - lastBackupTime) / 86400000
            );

            if (backupAge > BOOKMARKS_BACKUP_MAX_INTERVAL_DAYS) {
              this._bookmarksBackupIdleTime /= 2;
            }
          }
        }
        this._idleService.addIdleObserver(this, this._bookmarksBackupIdleTime);
      }

      if (this._isNewProfile) {
        try {
          // New profiles may have existing bookmarks (imported from another browser or
          // copied into the profile) and we want to show the bookmark toolbar for them
          // in some cases.
          this._maybeToggleBookmarkToolbarVisibility();
        } catch (ex) {
          Cu.reportError(ex);
        }
      }
    })()
      .catch(ex => {
        Cu.reportError(ex);
      })
      .then(() => {
        // NB: deliberately after the catch so that we always do this, even if
        // we threw halfway through initializing in the Task above.
        this._placesBrowserInitComplete = true;
        Services.obs.notifyObservers(null, "places-browser-init-complete");
      });
  },

  /**
   * If a backup for today doesn't exist, this creates one.
   */
  _backupBookmarks: function BG__backupBookmarks() {
    return (async function() {
      let lastBackupFile = await PlacesBackups.getMostRecentBackup();
      // Should backup bookmarks if there are no backups or the maximum
      // interval between backups elapsed.
      if (
        !lastBackupFile ||
        new Date() - PlacesBackups.getDateForFile(lastBackupFile) >
          BOOKMARKS_BACKUP_MIN_INTERVAL_DAYS * 86400000
      ) {
        let maxBackups = Services.prefs.getIntPref(
          "browser.bookmarks.max_backups"
        );
        await PlacesBackups.create(maxBackups);
      }
    })();
  },

  /**
   * Show the notificationBox for a locked places database.
   */
  _showPlacesLockedNotificationBox: function BG__showPlacesLockedNotificationBox() {
    var applicationName = gBrandBundle.GetStringFromName("brandShortName");
    var placesBundle = Services.strings.createBundle(
      "chrome://browser/locale/places/places.properties"
    );
    var title = placesBundle.GetStringFromName("lockPrompt.title");
    var text = placesBundle.formatStringFromName(
      "lockPrompt.text",
      [applicationName]
    );
    var buttonText = placesBundle.GetStringFromName(
      "lockPromptInfoButton.label"
    );
    var accessKey = placesBundle.GetStringFromName(
      "lockPromptInfoButton.accessKey"
    );

    var helpTopic = "places-locked";
    var url = Services.urlFormatter.formatURLPref("app.support.baseURL");
    url += helpTopic;

    var win = BrowserWindowTracker.getTopWindow();

    var buttons = [
      {
        label: buttonText,
        accessKey,
        popup: null,
        callback(aNotificationBar, aButton) {
          win.openTrustedLinkIn(url, "tab");
        },
      },
    ];

    var notifyBox = win.gBrowser.getNotificationBox();
    var notification = notifyBox.appendNotification(
      text,
      title,
      null,
      notifyBox.PRIORITY_CRITICAL_MEDIUM,
      buttons
    );
    notification.persistence = -1; // Until user closes it
  },

  _showSyncStartedDoorhanger() {
    let bundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let productName = gBrandBundle.GetStringFromName("brandShortName");
    let title = bundle.GetStringFromName("syncStartNotification.title");
    let body = bundle.formatStringFromName(
      "syncStartNotification.body2",
      [productName]
    );

    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      this._openPreferences("sync");
    };
    this.AlertsService.showAlertNotification(
      null,
      title,
      body,
      true,
      null,
      clickCallback
    );
  },

  /**
   * Uncollapses PersonalToolbar if its collapsed status is not
   * persisted, and user customized it or changed default bookmarks.
   *
   * If the user does not have a persisted value for the toolbar's
   * "collapsed" attribute, try to determine whether it's customized.
   */
  _maybeToggleBookmarkToolbarVisibility() {
    const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;
    const NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE = 3;
    let xulStore = Services.xulStore;

    if (!xulStore.hasValue(BROWSER_DOCURL, "PersonalToolbar", "collapsed")) {
      // We consider the toolbar customized if it has more than NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE
      // children, or if it has a persisted currentset value.
      let toolbarIsCustomized = xulStore.hasValue(
        BROWSER_DOCURL,
        "PersonalToolbar",
        "currentset"
      );
      let getToolbarFolderCount = () => {
        let toolbarFolder = PlacesUtils.getFolderContents(
          PlacesUtils.bookmarks.toolbarGuid
        ).root;
        let toolbarChildCount = toolbarFolder.childCount;
        toolbarFolder.containerOpen = false;
        return toolbarChildCount;
      };

      if (
        toolbarIsCustomized ||
        getToolbarFolderCount() > NUM_TOOLBAR_BOOKMARKS_TO_UNHIDE
      ) {
        CustomizableUI.setToolbarVisibility(
          CustomizableUI.AREA_BOOKMARKS,
          true
        );
      }
    }
  },

  _migrateXULStoreForDocument(fromURL, toURL) {
    Array.from(Services.xulStore.getIDsEnumerator(fromURL)).forEach(id => {
      Array.from(Services.xulStore.getAttributeEnumerator(fromURL, id)).forEach(
        attr => {
          let value = Services.xulStore.getValue(fromURL, id, attr);
          Services.xulStore.setValue(toURL, id, attr, value);
        }
      );
    });
  },

  // eslint-disable-next-line complexity
  _migrateUI: function BG__migrateUI() {
    // Use an increasing number to keep track of the current migration state.
    // Completely unrelated to the current Firefox release number.
    const UI_VERSION = 94;
    const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;

    if (!Services.prefs.prefHasUserValue("browser.migration.version")) {
      // This is a new profile, nothing to migrate.
      Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
      this._isNewProfile = true;
      return;
    }

    this._isNewProfile = false;
    let currentUIVersion = Services.prefs.getIntPref(
      "browser.migration.version"
    );
    if (currentUIVersion >= UI_VERSION) {
      return;
    }

    let xulStore = Services.xulStore;

    if (currentUIVersion < 52) {
      // Keep old devtools log persistence behavior after splitting netmonitor and
      // webconsole prefs (bug 1307881).
      if (Services.prefs.getBoolPref("devtools.webconsole.persistlog", false)) {
        Services.prefs.setBoolPref("devtools.netmonitor.persistlog", true);
      }
    }

    // Update user customizations that will interfere with the Safe Browsing V2
    // to V4 migration (bug 1395419).
    if (currentUIVersion < 53) {
      const MALWARE_PREF = "urlclassifier.malwareTable";
      if (Services.prefs.prefHasUserValue(MALWARE_PREF)) {
        let malwareList = Services.prefs.getCharPref(MALWARE_PREF);
        if (malwareList.includes("goog-malware-shavar")) {
          malwareList.replace("goog-malware-shavar", "goog-malware-proto");
          Services.prefs.setCharPref(MALWARE_PREF, malwareList);
        }
      }
    }

    if (currentUIVersion < 55) {
      Services.prefs.clearUserPref("browser.customizemode.tip0.shown");
    }

    if (currentUIVersion < 56) {
      // Prior to the end of the Firefox 57 cycle, the sidebarcommand being present
      // or not was the only thing that distinguished whether the sidebar was open.
      // Now, the sidebarcommand always indicates the last opened sidebar, and we
      // correctly persist the checked attribute to indicate whether or not the
      // sidebar was open. We should set the checked attribute in case it wasn't:
      if (xulStore.getValue(BROWSER_DOCURL, "sidebar-box", "sidebarcommand")) {
        xulStore.setValue(BROWSER_DOCURL, "sidebar-box", "checked", "true");
      }
    }

    if (currentUIVersion < 58) {
      // With Firefox 57, we are doing a one time reset of the geo prefs due to bug 1413652
      Services.prefs.clearUserPref("browser.search.countryCode");
      Services.prefs.clearUserPref("browser.search.region");
      Services.prefs.clearUserPref("browser.search.isUS");
    }

    if (currentUIVersion < 59) {
      let searchInitializedPromise = new Promise(resolve => {
        if (Services.search.isInitialized) {
          resolve();
        }
        const SEARCH_SERVICE_TOPIC = "browser-search-service";
        Services.obs.addObserver(function observer(subject, topic, data) {
          if (data != "init-complete") {
            return;
          }
          Services.obs.removeObserver(observer, SEARCH_SERVICE_TOPIC);
          resolve();
        }, SEARCH_SERVICE_TOPIC);
      });
      searchInitializedPromise.then(() => {
        let currentEngine = Services.search.defaultEngine.wrappedJSObject;
        // Only reset the current engine if it wasn't set by a WebExtension
        // and it is not one of the default engines.
        // If the original default is not a default, the user has a weird
        // configuration probably involving langpacks, it's not worth
        // attempting to reset their settings.
        if (
          currentEngine._extensionID ||
          currentEngine._isDefault ||
          !Services.search.originalDefaultEngine.wrappedJSObject._isDefault
        ) {
          return;
        }

        if (!currentEngine._loadPath.startsWith("[https]")) {
          Services.search.resetToOriginalDefaultEngine();
        }
      });

      // Migrate the old requested locales prefs to use the new model
      const SELECTED_LOCALE_PREF = "general.useragent.locale";
      const MATCHOS_LOCALE_PREF = "intl.locale.matchOS";

      if (
        Services.prefs.prefHasUserValue(MATCHOS_LOCALE_PREF) ||
        Services.prefs.prefHasUserValue(SELECTED_LOCALE_PREF)
      ) {
        if (Services.prefs.getBoolPref(MATCHOS_LOCALE_PREF, false)) {
          Services.locale.requestedLocales = [];
        } else {
          let locale = Services.prefs.getComplexValue(
            SELECTED_LOCALE_PREF,
            Ci.nsIPrefLocalizedString
          );
          if (locale) {
            try {
              Services.locale.requestedLocales = [locale.data];
            } catch (e) {
              /* Don't panic if the value is not a valid locale code. */
            }
          }
        }
        Services.prefs.clearUserPref(SELECTED_LOCALE_PREF);
        Services.prefs.clearUserPref(MATCHOS_LOCALE_PREF);
      }
    }

    if (currentUIVersion < 61) {
      // Remove persisted toolbarset from navigator toolbox
      xulStore.removeValue(BROWSER_DOCURL, "navigator-toolbox", "toolbarset");
    }

    if (currentUIVersion < 62) {
      // Remove iconsize and mode from all the toolbars
      let toolbars = [
        "navigator-toolbox",
        "nav-bar",
        "PersonalToolbar",
        "TabsToolbar",
        "toolbar-menubar",
      ];
      for (let resourceName of ["mode", "iconsize"]) {
        for (let toolbarId of toolbars) {
          xulStore.removeValue(BROWSER_DOCURL, toolbarId, resourceName);
        }
      }
    }

    if (currentUIVersion < 64) {
      OS.File.remove(
        OS.Path.join(OS.Constants.Path.profileDir, "directoryLinks.json"),
        { ignoreAbsent: true }
      );
    }

    if (
      currentUIVersion < 65 &&
      Services.prefs.getCharPref("general.config.filename", "") ==
        "dsengine.cfg"
    ) {
      let searchInitializedPromise = new Promise(resolve => {
        if (Services.search.isInitialized) {
          resolve();
        }
        const SEARCH_SERVICE_TOPIC = "browser-search-service";
        Services.obs.addObserver(function observer(subject, topic, data) {
          if (data != "init-complete") {
            return;
          }
          Services.obs.removeObserver(observer, SEARCH_SERVICE_TOPIC);
          resolve();
        }, SEARCH_SERVICE_TOPIC);
      });
      searchInitializedPromise.then(() => {
        let engineNames = [
          "Bing Search Engine",
          "Yahoo! Search Engine",
          "Yandex Search Engine",
        ];
        for (let engineName of engineNames) {
          let engine = Services.search.getEngineByName(engineName);
          if (engine) {
            Services.search.removeEngine(engine);
          }
        }
      });
    }

    if (currentUIVersion < 66) {
      // Set whether search suggestions or history/bookmarks results come first
      // in the urlbar results, and uninstall a related Shield study.
      this._migrateMatchBucketsPrefForUI66();
    }

    if (currentUIVersion < 67) {
      // Migrate devtools firebug theme users to light theme (bug 1378108):
      if (Services.prefs.getCharPref("devtools.theme") == "firebug") {
        Services.prefs.setCharPref("devtools.theme", "light");
      }
    }

    if (currentUIVersion < 68) {
      // Remove blocklists legacy storage, now relying on IndexedDB.
      OS.File.remove(
        OS.Path.join(OS.Constants.Path.profileDir, "kinto.sqlite"),
        { ignoreAbsent: true }
      );
    }

    if (currentUIVersion < 69) {
      // Clear old social prefs from profile (bug 1460675)
      let socialPrefs = Services.prefs.getBranch("social.");
      if (socialPrefs) {
        let socialPrefsArray = socialPrefs.getChildList("");
        for (let item of socialPrefsArray) {
          Services.prefs.clearUserPref("social." + item);
        }
      }
    }

    if (currentUIVersion < 70) {
      // Migrate old ctrl-tab pref to new one in existing profiles. (This code
      // doesn't run at all in new profiles.)
      Services.prefs.setBoolPref(
        "browser.ctrlTab.recentlyUsedOrder",
        Services.prefs.getBoolPref("browser.ctrlTab.previews", false)
      );
      Services.prefs.clearUserPref("browser.ctrlTab.previews");
      // Remember that we migrated the pref in case we decide to flip it for
      // these users.
      Services.prefs.setBoolPref("browser.ctrlTab.migrated", true);
    }

    if (currentUIVersion < 71) {
      // Clear legacy saved prefs for content handlers.
      let savedContentHandlers = Services.prefs.getChildList(
        "browser.contentHandlers.types"
      );
      for (let savedHandlerPref of savedContentHandlers) {
        Services.prefs.clearUserPref(savedHandlerPref);
      }
    }

    if (currentUIVersion < 72) {
      // Migrate performance tool's recording interval value from msec to usec.
      let pref = "devtools.performance.recording.interval";
      Services.prefs.setIntPref(
        pref,
        Services.prefs.getIntPref(pref, 1) * 1000
      );
    }

    if (currentUIVersion < 73) {
      // Remove blocklist JSON local dumps in profile.
      OS.File.removeDir(
        OS.Path.join(OS.Constants.Path.profileDir, "blocklists"),
        { ignoreAbsent: true }
      );
      OS.File.removeDir(
        OS.Path.join(OS.Constants.Path.profileDir, "blocklists-preview"),
        { ignoreAbsent: true }
      );
      for (const filename of ["addons.json", "plugins.json", "gfx.json"]) {
        // Some old versions used to dump without subfolders. Clean them while we are at it.
        const path = OS.Path.join(
          OS.Constants.Path.profileDir,
          `blocklists-${filename}`
        );
        OS.File.remove(path, { ignoreAbsent: true });
      }
    }

    if (currentUIVersion < 75) {
      // Ensure we try to migrate any live bookmarks the user might have, trying up to
      // 5 times. We set this early, and here, to avoid running the migration on
      // new profile (or, indeed, ever creating the pref there).
      Services.prefs.setIntPref(
        "browser.livebookmarks.migrationAttemptsLeft",
        5
      );
    }

    if (currentUIVersion < 77) {
      // Remove currentset from all the toolbars
      let toolbars = [
        "nav-bar",
        "PersonalToolbar",
        "TabsToolbar",
        "toolbar-menubar",
      ];
      for (let toolbarId of toolbars) {
        xulStore.removeValue(BROWSER_DOCURL, toolbarId, "currentset");
      }
    }

    if (currentUIVersion < 78) {
      Services.prefs.clearUserPref("browser.search.region");
    }

    if (currentUIVersion < 79) {
      // The handler app service will read this. We need to wait with migrating
      // until the handler service has started up, so just set a pref here.
      Services.prefs.setCharPref("browser.handlers.migrations", "30boxes");
    }

    if (currentUIVersion < 80) {
      let hosts = Services.prefs.getCharPref("network.proxy.no_proxies_on");
      // remove "localhost" and "127.0.0.1" from the no_proxies_on list
      const kLocalHosts = new Set(["localhost", "127.0.0.1"]);
      hosts = hosts
        .split(/[ ,]+/)
        .filter(host => !kLocalHosts.has(host))
        .join(", ");
      Services.prefs.setCharPref("network.proxy.no_proxies_on", hosts);
    }

    if (currentUIVersion < 81) {
      // Reset homepage pref for users who have it set to a default from before Firefox 4:
      //   <locale>.(start|start2|start3).mozilla.(com|org)
      if (HomePage.overridden) {
        const DEFAULT = HomePage.getDefault();
        let value = HomePage.get();
        let updated = value.replace(
          /https?:\/\/([\w\-]+\.)?start\d*\.mozilla\.(org|com)[^|]*/gi,
          DEFAULT
        );
        if (updated != value) {
          if (updated == DEFAULT) {
            HomePage.reset();
          } else {
            value = updated;
            HomePage.set(value);
          }
        }
      }
    }

    if (currentUIVersion < 82) {
      this._migrateXULStoreForDocument("chrome://browser/content/browser.xul",
                                       "chrome://browser/content/browser.xhtml");
    }

    if (currentUIVersion < 89) {
      // This file was renamed in https://bugzilla.mozilla.org/show_bug.cgi?id=1595636.
      this._migrateXULStoreForDocument(
        "chrome://devtools/content/framework/toolbox-window.xul",
        "chrome://devtools/content/framework/toolbox-window.xhtml"
      );
    }

    if (currentUIVersion < 90) {
      this._migrateXULStoreForDocument(
        "chrome://browser/content/places/historySidebar.xul",
        "chrome://browser/content/places/historySidebar.xhtml"
      );
      this._migrateXULStoreForDocument(
        "chrome://browser/content/places/places.xul",
        "chrome://browser/content/places/places.xhtml"
      );
      this._migrateXULStoreForDocument(
        "chrome://browser/content/places/bookmarksSidebar.xul",
        "chrome://browser/content/places/bookmarksSidebar.xhtml"
      );
    }

    // Update the migration version.
    Services.prefs.setIntPref("browser.migration.version", UI_VERSION);
  },

  _checkForDefaultBrowser() {
    // Perform default browser checking.
    if (!ShellService) {
      return;
    }

    let shouldCheck =
      !AppConstants.DEBUG && ShellService.shouldCheckDefaultBrowser;

    const skipDefaultBrowserCheck =
      Services.prefs.getBoolPref(
        "browser.shell.skipDefaultBrowserCheckOnFirstRun"
      ) &&
      !Services.prefs.getBoolPref(
        "browser.shell.didSkipDefaultBrowserCheckOnFirstRun"
      );

    const usePromptLimit = !AppConstants.RELEASE_OR_BETA;
    let promptCount = usePromptLimit
      ? Services.prefs.getIntPref("browser.shell.defaultBrowserCheckCount")
      : 0;

    let willRecoverSession =
      SessionStartup.sessionType == SessionStartup.RECOVER_SESSION;

    // startup check, check all assoc
    let isDefault = false;
    let isDefaultError = false;
    try {
      isDefault = ShellService.isDefaultBrowser(true, false);
    } catch (ex) {
      isDefaultError = true;
    }

    let pService = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
      Ci.nsIToolkitProfileService
    );

    if (pService.portable() !=1 ) {
      if (isDefault) {
        let now = Math.floor(Date.now() / 1000).toString();
        Services.prefs.setCharPref(
          "browser.shell.mostRecentDateSetAsDefault",
          now
        );
      }

      let willPrompt = shouldCheck && !isDefault && !willRecoverSession;

      // Skip the "Set Default Browser" check during first-run or after the
      // browser has been run a few times.
      if (willPrompt) {
        if (skipDefaultBrowserCheck) {
          Services.prefs.setBoolPref(
            "browser.shell.didSkipDefaultBrowserCheckOnFirstRun",
            true
          );
          willPrompt = false;
        } else {
          promptCount++;
        }
        if (usePromptLimit && promptCount > 3) {
          willPrompt = false;
        }
      }

      if (usePromptLimit && willPrompt) {
        Services.prefs.setIntPref(
          "browser.shell.defaultBrowserCheckCount",
          promptCount
        );
      }

      if (willPrompt) {
        DefaultBrowserCheck.prompt(BrowserWindowTracker.getTopWindow());
      }
    }
  },

  async _migrateMatchBucketsPrefForUI66() {
    // This does two related things.
    //
    // (1) Profiles created on or after Firefox 57's release date were eligible
    // for a Shield study that changed the browser.urlbar.matchBuckets pref in
    // order to show search suggestions above history/bookmarks in the urlbar
    // popup.  This uninstalls that study.  (It's actually slightly more
    // complex.  The study set the pref to several possible values, but the
    // overwhelming number of profiles in the study got search suggestions
    // first, followed by history/bookmarks.)
    //
    // (2) This also ensures that (a) new users see search suggestions above
    // history/bookmarks, thereby effectively making the study permanent, and
    // (b) old users (including those in the study) continue to see whatever
    // they were seeing before.  This works together with UnifiedComplete.js.
    // By default, the browser.urlbar.matchBuckets pref does not exist, and
    // UnifiedComplete.js internally hardcodes a default value for it.  Before
    // Firefox 60, the hardcoded default was to show history/bookmarks first.
    // After 60, it's to show search suggestions first.

    // At this point, normally the pref should not exist.  If it does, then it
    // either has a user value, or something unexpectedly set its value on the
    // default branch.  Either way, preserve that value.
    if (Services.prefs.getCharPref(prefName, "")) {
      return;
    }

    // The new default is "suggestion:4,general:5" (show search suggestions
    // before history/bookmarks), but we implement that by leaving the pref
    // undefined, and UnifiedComplete.js hardcodes that value internally.  So if
    // the pref was "suggestion:4,general:5" (modulo whitespace), we're done.
    if (prefValue) {
      let buckets = PlacesUtils.convertMatchBucketsStringToArray(prefValue);
      if (
        ObjectUtils.deepEqual(buckets, [
          ["suggestion", 4],
          ["general", 5],
        ])
      ) {
        return;
      }
    }

    // Set the pref on the user branch.  If the pref had a value, then preserve
    // it.  Otherwise, set the previous default value, which was to show history
    // and bookmarks before search suggestions.
    prefValue = prefValue || "general:5,suggestion:Infinity";
    Services.prefs.setCharPref(prefName, prefValue);
  },

  /**
   * Open preferences even if there are no open windows.
   */
  _openPreferences(...args) {
    let chromeWindow = BrowserWindowTracker.getTopWindow();
    if (chromeWindow) {
      chromeWindow.openPreferences(...args);
      return;
    }

    if (Services.appShell.hiddenDOMWindow.openPreferences) {
      Services.appShell.hiddenDOMWindow.openPreferences(...args);
    }
  },

  _openURLInNewWindow(url) {
    let urlString = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    urlString.data = url;
    return new Promise(resolve => {
      let win = Services.ww.openWindow(
        null,
        AppConstants.BROWSER_CHROME_URL,
        "_blank",
        "chrome,all,dialog=no",
        urlString
      );
      win.addEventListener(
        "load",
        () => {
          resolve(win);
        },
        { once: true }
      );
    });
  },

  /**
   * Called as an observer when Sync's "display URIs" notification is fired.
   *
   * We open the received URIs in background tabs.
   */
  async _onDisplaySyncURIs(data) {
    try {
      // The payload is wrapped weirdly because of how Sync does notifications.
      const URIs = data.wrappedJSObject.object;

      // win can be null, but it's ok, we'll assign it later in openTab()
      let win = BrowserWindowTracker.getTopWindow({ private: false });

      const openTab = async URI => {
        let tab;
        if (!win) {
          win = await this._openURLInNewWindow(URI.uri);
          let tabs = win.gBrowser.tabs;
          tab = tabs[tabs.length - 1];
        } else {
          tab = win.gBrowser.addWebTab(URI.uri);
        }
        tab.setAttribute("attention", true);
        return tab;
      };

      const firstTab = await openTab(URIs[0]);
      await Promise.all(URIs.slice(1).map(URI => openTab(URI)));

      const deviceName = URIs[0].sender && URIs[0].sender.name;
      let title, body;
      const bundle = Services.strings.createBundle(
        "chrome://browser/locale/accounts.properties"
      );
      if (URIs.length == 1) {
        // Due to bug 1305895, tabs from iOS may not have device information, so
        // we have separate strings to handle those cases. (See Also
        // unnamedTabsArrivingNotificationNoDevice.body below)
        if (deviceName) {
          title = bundle.formatStringFromName(
            "tabArrivingNotificationWithDevice.title",
            [deviceName]
          );
        } else {
          title = bundle.GetStringFromName("tabArrivingNotification.title");
        }
        // Use the page URL as the body. We strip the fragment and query (after
        // the `?` and `#` respectively) to reduce size, and also format it the
        // same way that the url bar would.
        body = URIs[0].uri.replace(/([?#]).*$/, "$1");
        let wasTruncated = body.length < URIs[0].uri.length;
        if (win.gURLBar) {
          body = win.gURLBar.trimValue(body);
        }
        if (wasTruncated) {
          body = bundle.formatStringFromName(
            "singleTabArrivingWithTruncatedURL.body",
            [body]
          );
        }
      } else {
        title = bundle.GetStringFromName(
          "multipleTabsArrivingNotification.title"
        );
        const allKnownSender = URIs.every(URI => URI.sender != null);
        const allSameDevice =
          allKnownSender &&
          URIs.every(URI => URI.sender.id == URIs[0].sender.id);
        let tabArrivingBody;
        if (allSameDevice) {
          if (deviceName) {
            tabArrivingBody = "unnamedTabsArrivingNotification2.body";
          } else {
            tabArrivingBody = "unnamedTabsArrivingNotificationNoDevice.body";
          }
        } else {
          tabArrivingBody = "unnamedTabsArrivingNotificationMultiple2.body";
        }

        body = bundle.GetStringFromName(tabArrivingBody);
        body = PluralForm.get(URIs.length, body);
        body = body.replace("#1", URIs.length);
        body = body.replace("#2", deviceName);
      }

      const clickCallback = (obsSubject, obsTopic, obsData) => {
        if (obsTopic == "alertclickcallback") {
          win.gBrowser.selectedTab = firstTab;
        }
      };

      // Specify an icon because on Windows no icon is shown at the moment
      let imageURL;
      if (AppConstants.platform == "win") {
        imageURL = "chrome://branding/content/icon64.png";
      }
      this.AlertsService.showAlertNotification(
        imageURL,
        title,
        body,
        true,
        null,
        clickCallback
      );
    } catch (ex) {
      Cu.reportError("Error displaying tab(s) received by Sync: " + ex);
    }
  },

  async _onVerifyLoginNotification({ body, title, url }) {
    let tab;
    let imageURL;
    if (AppConstants.platform == "win") {
      imageURL = "chrome://branding/content/icon64.png";
    }
    let win = BrowserWindowTracker.getTopWindow({ private: false });
    if (!win) {
      win = await this._openURLInNewWindow(url);
      let tabs = win.gBrowser.tabs;
      tab = tabs[tabs.length - 1];
    } else {
      tab = win.gBrowser.addWebTab(url);
    }
    tab.setAttribute("attention", true);
    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      win.gBrowser.selectedTab = tab;
    };

    try {
      this.AlertsService.showAlertNotification(
        imageURL,
        title,
        body,
        true,
        null,
        clickCallback
      );
    } catch (ex) {
      Cu.reportError("Error notifying of a verify login event: " + ex);
    }
  },

  _onDeviceConnected(deviceName) {
    let accountsBundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let title = accountsBundle.GetStringFromName("deviceConnectedTitle");
    let body = accountsBundle.formatStringFromName(
      "deviceConnectedBody" + (deviceName ? "" : ".noDeviceName"),
      [deviceName]
    );

    let clickCallback = async (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      let url = await FxAccounts.config.promiseManageDevicesURI(
        "device-connected-notification"
      );
      let win = BrowserWindowTracker.getTopWindow({ private: false });
      if (!win) {
        this._openURLInNewWindow(url);
      } else {
        win.gBrowser.addWebTab(url);
      }
    };

    try {
      this.AlertsService.showAlertNotification(
        null,
        title,
        body,
        true,
        null,
        clickCallback
      );
    } catch (ex) {
      Cu.reportError("Error notifying of a new Sync device: " + ex);
    }
  },

  _onDeviceDisconnected() {
    let bundle = Services.strings.createBundle(
      "chrome://browser/locale/accounts.properties"
    );
    let title = bundle.GetStringFromName(
      "deviceDisconnectedNotification.title"
    );
    let body = bundle.GetStringFromName("deviceDisconnectedNotification.body");

    let clickCallback = (subject, topic, data) => {
      if (topic != "alertclickcallback") {
        return;
      }
      this._openPreferences("sync");
    };
    this.AlertsService.showAlertNotification(
      null,
      title,
      body,
      true,
      null,
      clickCallback
    );
  },

  _updateFxaBadges() {
    let state = UIState.get();
    if (
      state.status == UIState.STATUS_LOGIN_FAILED ||
      state.status == UIState.STATUS_NOT_VERIFIED
    ) {
      AppMenuNotifications.showBadgeOnlyNotification(
        "fxa-needs-authentication"
      );
    } else {
      AppMenuNotifications.removeNotification("fxa-needs-authentication");
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),
};

/**
 * ContentPermissionIntegration is responsible for showing the user
 * simple permission prompts when content requests additional
 * capabilities.
 *
 * While there are some built-in permission prompts, createPermissionPrompt
 * can also be overridden by system add-ons or tests to provide new ones.
 *
 * This override ability is provided by Integration.jsm. See
 * PermissionUI.jsm for an example of how to provide a new prompt
 * from an add-on.
 */
const ContentPermissionIntegration = {
  /**
   * Creates a PermissionPrompt for a given permission type and
   * nsIContentPermissionRequest.
   *
   * @param {string} type
   *        The type of the permission request from content. This normally
   *        matches the "type" field of an nsIContentPermissionType, but it
   *        can be something else if the permission does not use the
   *        nsIContentPermissionRequest model. Note that this type might also
   *        be different from the permission key used in the permissions
   *        database.
   *        Example: "geolocation"
   * @param {nsIContentPermissionRequest} request
   *        The request for a permission from content.
   * @return {PermissionPrompt} (see PermissionUI.jsm),
   *         or undefined if the type cannot be handled.
   */
  createPermissionPrompt(type, request) {
    switch (type) {
      case "geolocation": {
        return new PermissionUI.GeolocationPermissionPrompt(request);
      }
      case "desktop-notification": {
        return new PermissionUI.DesktopNotificationPermissionPrompt(request);
      }
      case "persistent-storage": {
        return new PermissionUI.PersistentStoragePermissionPrompt(request);
      }
      case "midi": {
        return new PermissionUI.MIDIPermissionPrompt(request);
      }
      case "storage-access": {
        return new PermissionUI.StorageAccessPermissionPrompt(request);
      }
    }
    return undefined;
  },
};

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{d8903bf6-68d5-4e97-bcd1-e4d3012f721a}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  /**
   * This implementation of nsIContentPermissionPrompt.prompt ensures
   * that there's only one nsIContentPermissionType in the request,
   * and that it's of type nsIContentPermissionType. Failing to
   * satisfy either of these conditions will result in this method
   * throwing NS_ERRORs. If the combined ContentPermissionIntegration
   * cannot construct a prompt for this particular request, an
   * NS_ERROR_FAILURE will be thrown.
   *
   * Any time an error is thrown, the nsIContentPermissionRequest is
   * cancelled automatically.
   *
   * @param {nsIContentPermissionRequest} request
   *        The request that we're to show a prompt for.
   */
  prompt(request) {
    let type;
    try {
      // Only allow exactly one permission request here.
      let types = request.types.QueryInterface(Ci.nsIArray);
      if (types.length != 1) {
        throw Components.Exception(
          "Expected an nsIContentPermissionRequest with only 1 type.",
          Cr.NS_ERROR_UNEXPECTED
        );
      }

      type = types.queryElementAt(0, Ci.nsIContentPermissionType).type;
      let combinedIntegration = Integration.contentPermission.getCombined(
        ContentPermissionIntegration
      );

      let permissionPrompt = combinedIntegration.createPermissionPrompt(
        type,
        request
      );
      if (!permissionPrompt) {
        throw Components.Exception(
          `Failed to handle permission of type ${type}`,
          Cr.NS_ERROR_FAILURE
        );
      }

      permissionPrompt.prompt();
    } catch (ex) {
      Cu.reportError(ex);
      request.cancel();
      throw ex;
    }

    let scheme = 0;
    try {
      if (request.principal.schemeIs("http")) {
        scheme = 1;
      } else if (request.principal.schemeIs("https")) {
        scheme = 2;
      }
    } catch (ex) {
      // If the request principal is not available at this point,
      // the request has likely been cancelled before being shown to the
      // user. We shouldn't record this request.
      if (ex.result != Cr.NS_ERROR_FAILURE) {
        Cu.reportError(ex);
      }
      return;
    }
  },
};

var DefaultBrowserCheck = {
  get OPTIONPOPUP() {
    return "defaultBrowserNotificationPopup";
  },

  closePrompt(aNode) {
    if (this._notification) {
      this._notification.close();
    }
  },

  setAsDefault() {
    let claimAllTypes = true;
    let setAsDefaultError = false;
    if (AppConstants.platform == "win") {
      try {
        // In Windows 8+, the UI for selecting default protocol is much
        // nicer than the UI for setting file type associations. So we
        // only show the protocol association screen on Windows 8+.
        // Windows 8 is version 6.2.
        let version = Services.sysinfo.getProperty("version");
        claimAllTypes = parseFloat(version) < 6.2;
      } catch (ex) {}
    }
    try {
      ShellService.setDefaultBrowser(claimAllTypes, false);
    } catch (ex) {
      setAsDefaultError = true;
      Cu.reportError(ex);
    }
  },

  _createPopup(win, notNowStrings, neverStrings) {
    let doc = win.document;
    let popup = doc.createXULElement("menupopup");
    popup.id = this.OPTIONPOPUP;

    let notNowItem = doc.createXULElement("menuitem");
    notNowItem.id = "defaultBrowserNotNow";
    notNowItem.setAttribute("label", notNowStrings.label);
    notNowItem.setAttribute("accesskey", notNowStrings.accesskey);
    popup.appendChild(notNowItem);

    let neverItem = doc.createXULElement("menuitem");
    neverItem.id = "defaultBrowserNever";
    neverItem.setAttribute("label", neverStrings.label);
    neverItem.setAttribute("accesskey", neverStrings.accesskey);
    popup.appendChild(neverItem);

    popup.addEventListener("command", this);

    let popupset = doc.getElementById("mainPopupSet");
    popupset.appendChild(popup);
  },

  handleEvent(event) {
    if (event.type == "command") {
      if (event.target.id == "defaultBrowserNever") {
        ShellService.shouldCheckDefaultBrowser = false;
      }
      this.closePrompt();
    }
  },

  prompt(win) {
    let useNotificationBar = Services.prefs.getBoolPref(
      "browser.defaultbrowser.notificationbar"
    );

    let brandBundle = win.document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");

    let shellBundle = win.document.getElementById("bundle_shell");
    let buttonPrefix =
      "setDefaultBrowser" + (useNotificationBar ? "" : "Alert");
    let yesButton = shellBundle.getFormattedString(
      buttonPrefix + "Confirm.label",
      [brandShortName]
    );
    let notNowButton = shellBundle.getString(buttonPrefix + "NotNow.label");

    if (useNotificationBar) {
      let promptMessage = shellBundle.getFormattedString(
        "setDefaultBrowserMessage2",
        [brandShortName]
      );
      let optionsMessage = shellBundle.getString(
        "setDefaultBrowserOptions.label"
      );
      let optionsKey = shellBundle.getString(
        "setDefaultBrowserOptions.accesskey"
      );

      let neverLabel = shellBundle.getString("setDefaultBrowserNever.label");
      let neverKey = shellBundle.getString("setDefaultBrowserNever.accesskey");

      let yesButtonKey = shellBundle.getString(
        "setDefaultBrowserConfirm.accesskey"
      );
      let notNowButtonKey = shellBundle.getString(
        "setDefaultBrowserNotNow.accesskey"
      );

      this._createPopup(
        win,
        {
          label: notNowButton,
          accesskey: notNowButtonKey,
        },
        {
          label: neverLabel,
          accesskey: neverKey,
        }
      );

      let buttons = [
        {
          label: yesButton,
          accessKey: yesButtonKey,
          callback: () => {
            this.setAsDefault();
            this.closePrompt();
          },
        },
        {
          label: optionsMessage,
          accessKey: optionsKey,
          popup: this.OPTIONPOPUP,
        },
      ];

      let iconPixels = win.devicePixelRatio > 1 ? "32" : "16";
      let iconURL = "chrome://branding/content/icon" + iconPixels + ".png";
      const priority = win.gHighPriorityNotificationBox.PRIORITY_WARNING_HIGH;
      let callback = this._onNotificationEvent.bind(this);
      this._notification = win.gHighPriorityNotificationBox.appendNotification(
        promptMessage,
        "default-browser",
        iconURL,
        priority,
        buttons,
        callback
      );
    } else {
      // Modal prompt
      let promptTitle = shellBundle.getString("setDefaultBrowserTitle");
      let promptMessage = shellBundle.getFormattedString(
        "setDefaultBrowserMessage",
        [brandShortName]
      );
      let askLabel = shellBundle.getFormattedString(
        "setDefaultBrowserDontAsk",
        [brandShortName]
      );

      let ps = Services.prompt;
      let shouldAsk = { value: true };
      let buttonFlags =
        ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
        ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_1 +
        ps.BUTTON_POS_0_DEFAULT;
      let rv = ps.confirmEx(
        win,
        promptTitle,
        promptMessage,
        buttonFlags,
        yesButton,
        notNowButton,
        null,
        askLabel,
        shouldAsk
      );
      if (rv == 0) {
        this.setAsDefault();
      } else if (!shouldAsk.value) {
        ShellService.shouldCheckDefaultBrowser = false;
      }
    }
  },

  _onNotificationEvent(eventType) {
    if (eventType == "removed") {
      let doc = this._notification.ownerDocument;
      let popup = doc.getElementById(this.OPTIONPOPUP);
      popup.removeEventListener("command", this);
      popup.remove();
      delete this._notification;
    }
  },
};

/*
 * Prompts users who have an outdated JAWS screen reader informing
 * them they need to update JAWS or switch to esr. Can be removed
 * 12/31/2018.
 */
var JawsScreenReaderVersionCheck = {
  _prompted: false,

  init() {
    Services.obs.addObserver(this, "a11y-init-or-shutdown", true);
  },

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsISupportsWeakReference,
  ]),

  observe(subject, topic, data) {
    if (topic == "a11y-init-or-shutdown" && data == "1") {
      Services.tm.dispatchToMainThread(() => this._checkVersionAndPrompt());
    }
  },

  onWindowsRestored() {
    Services.tm.dispatchToMainThread(() => this._checkVersionAndPrompt());
  },

  _checkVersionAndPrompt() {
    // Make sure we only prompt for versions of JAWS we do not
    // support and never prompt if e10s is disabled or if we're on
    // nightly.
    if (
      !Services.appinfo.shouldBlockIncompatJaws ||
      !Services.appinfo.browserTabsRemoteAutostart ||
      AppConstants.NIGHTLY_BUILD
    ) {
      return;
    }

    let win = BrowserWindowTracker.getTopWindow();
    if (!win || !win.gBrowser || !win.gBrowser.selectedBrowser) {
      Services.console.logStringMessage(
        "Content access support for older versions of JAWS is disabled " +
          "due to compatibility issues with this version of Firefox."
      );
      this._prompted = false;
      return;
    }

    // Only prompt once per session
    if (this._prompted) {
      return;
    }
    this._prompted = true;

    let browser = win.gBrowser.selectedBrowser;

    // Prompt JAWS users to let them know they need to update
    let promptMessage = win.gNavigatorBundle.getFormattedString(
      "e10s.accessibilityNotice.jawsMessage",
      [gBrandBundle.GetStringFromName("brandShortName")]
    );
    let notification;
    // main option: an Ok button, keeps running with content accessibility disabled
    let mainAction = {
      label: win.gNavigatorBundle.getString(
        "e10s.accessibilityNotice.acceptButton.label"
      ),
      accessKey: win.gNavigatorBundle.getString(
        "e10s.accessibilityNotice.acceptButton.accesskey"
      ),
      callback() {
        // If the user invoked the button option remove the notification,
        // otherwise keep the alert icon around in the address bar.
        notification.remove();
      },
    };
    let options = {
      popupIconURL: "chrome://browser/skin/e10s-64@2x.png",
      persistWhileVisible: true,
      persistent: true,
      persistence: 100,
    };

    notification = win.PopupNotifications.show(
      browser,
      "e10s_enabled_with_incompat_jaws",
      promptMessage,
      null,
      mainAction,
      null,
      options
    );
  },
};
