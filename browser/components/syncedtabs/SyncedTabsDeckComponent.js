/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { SyncedTabsDeckStore } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsDeckStore.js"
);
const { SyncedTabsDeckView } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsDeckView.js"
);
const { SyncedTabsListStore } = ChromeUtils.import(
  "resource:///modules/syncedtabs/SyncedTabsListStore.js"
);
const { TabListComponent } = ChromeUtils.import(
  "resource:///modules/syncedtabs/TabListComponent.js"
);
const { TabListView } = ChromeUtils.import(
  "resource:///modules/syncedtabs/TabListView.js"
);
let { getChromeWindow } = ChromeUtils.import(
  "resource:///modules/syncedtabs/util.js"
);

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function() {
  return ChromeUtils.import("resource://gre/modules/FxAccountsCommon.js", {});
});

let log = ChromeUtils.import(
  "resource://gre/modules/Log.jsm",
  {}
).Log.repository.getLogger("Sync.RemoteTabs");

var EXPORTED_SYMBOLS = ["SyncedTabsDeckComponent"];

/* SyncedTabsDeckComponent
 * This component instantiates views and storage objects as well as defines
 * behaviors that will be passed down to the views. This helps keep the views
 * isolated and easier to test.
 */

function SyncedTabsDeckComponent({
  window,
  SyncedTabs,
  fxAccounts,
  deckStore,
  listStore,
  listComponent,
  DeckView,
  getChromeWindowMock,
}) {
  this._window = window;
  this._SyncedTabs = SyncedTabs;
  this._fxAccounts = fxAccounts;
  this._DeckView = DeckView || SyncedTabsDeckView;
  // used to stub during tests
  this._getChromeWindow = getChromeWindowMock || getChromeWindow;

  this._deckStore = deckStore || new SyncedTabsDeckStore();
  this._syncedTabsListStore = listStore || new SyncedTabsListStore(SyncedTabs);
  this.tabListComponent =
    listComponent ||
    new TabListComponent({
      window: this._window,
      store: this._syncedTabsListStore,
      View: TabListView,
      SyncedTabs,
      clipboardHelper: Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
        Ci.nsIClipboardHelper
      ),
      getChromeWindow: this._getChromeWindow,
    });
}

SyncedTabsDeckComponent.prototype = {
  PANELS: {
    TABS_CONTAINER: "tabs-container",
    TABS_FETCHING: "tabs-fetching",
    LOGIN_FAILED: "reauth",
    NOT_AUTHED_INFO: "notAuthedInfo",
    SINGLE_DEVICE_INFO: "singleDeviceInfo",
    TABS_DISABLED: "tabs-disabled",
    UNVERIFIED: "unverified",
  },

  get container() {
    return this._deckView ? this._deckView.container : null;
  },

  init() {
    Services.obs.addObserver(this, this._SyncedTabs.TOPIC_TABS_CHANGED);
    Services.obs.addObserver(this, FxAccountsCommon.ONLOGIN_NOTIFICATION);
    Services.obs.addObserver(this, "weave:service:login:change");
    // If the Sync service is not ready, in init() > updatePanel() we will
    // show a blank screen. If tab syncing is disabled, we will not get any other
    // ui-refreshing notifications! We listen to :ready in order to check again
    // if this engine is disabled and refresh the UI one last time.
    Services.obs.addObserver(this, "weave:service:ready");

    // Add app locale change support for HTML sidebar
    Services.obs.addObserver(this, "intl:app-locales-changed");
    Services.prefs.addObserver("intl.l10n.pseudo", this);
    this.updateDir();

    // Go ahead and trigger sync
    this._SyncedTabs.syncTabs().catch(Cu.reportError);

    this._deckView = new this._DeckView(this._window, this.tabListComponent, {
      onConnectDeviceClick: event => this.openConnectDevice(event),
      onSyncPrefClick: event => this.openSyncPrefs(event),
    });

    this._deckStore.on("change", state => this._deckView.render(state));
    // Trigger the initial rendering of the deck view
    // Object.values only in nightly
    this._deckStore.setPanels(
      Object.keys(this.PANELS).map(k => this.PANELS[k])
    );
    // Set the initial panel to display
    this.updatePanel();
  },

  uninit() {
    Services.obs.removeObserver(this, this._SyncedTabs.TOPIC_TABS_CHANGED);
    Services.obs.removeObserver(this, FxAccountsCommon.ONLOGIN_NOTIFICATION);
    Services.obs.removeObserver(this, "weave:service:login:change");
    Services.obs.removeObserver(this, "weave:service:ready");
    Services.obs.removeObserver(this, "intl:app-locales-changed");
    Services.prefs.removeObserver("intl.l10n.pseudo", this);
    this._deckView.destroy();
  },

  observe(subject, topic, data) {
    switch (topic) {
      case this._SyncedTabs.TOPIC_TABS_CHANGED:
        this._syncedTabsListStore.getData();
        this.updatePanel();
        break;
      case "weave:service:ready":
        Services.obs.removeObserver(this, "weave:service:ready");
      // Intended fallthrough.
      case FxAccountsCommon.ONLOGIN_NOTIFICATION:
      case "weave:service:login:change":
        this.updatePanel();
        break;
      case "intl:app-locales-changed":
        this.updateDir();
        break;
      case "nsPref:changed":
        if (data == "intl.l10n.pseudo") {
          this.updateDir();
        }
        break;
      default:
        break;
    }
  },

  // There's no good way to mock fxAccounts in browser tests where it's already
  // been instantiated, so we have this method for stubbing.
  _getSignedInUser() {
    return this._fxAccounts.getSignedInUser();
  },

  getPanelStatus() {
    return this._getSignedInUser()
      .then(user => {
        if (!user) {
          return this.PANELS.NOT_AUTHED_INFO;
        }
        if (this._SyncedTabs.loginFailed) {
          return this.PANELS.LOGIN_FAILED;
        }
        if (!user.verified) {
          return this.PANELS.UNVERIFIED;
        }
        if (!this._SyncedTabs.isConfiguredToSyncTabs) {
          return this.PANELS.TABS_DISABLED;
        }
        if (!this._SyncedTabs.hasSyncedThisSession) {
          return this.PANELS.TABS_FETCHING;
        }
        return this._SyncedTabs.getTabClients().then(clients => {
          if (clients.length) {
            return this.PANELS.TABS_CONTAINER;
          }
          return this.PANELS.SINGLE_DEVICE_INFO;
        });
      })
      .catch(err => {
        Cu.reportError(err);
        return this.PANELS.NOT_AUTHED_INFO;
      });
  },

  updateDir() {
    // If the HTML document doesn't exist, we can't update the window
    if (!this._window.document) {
      return;
    }

    if (Services.locale.isAppLocaleRTL) {
      this._window.document.body.dir = "rtl";
    } else {
      this._window.document.body.dir = "ltr";
    }
  },

  updatePanel() {
    // return promise for tests
    return this.getPanelStatus()
      .then(panelId => this._deckStore.selectPanel(panelId))
      .catch(Cu.reportError);
  },

  openSyncPrefs() {
    this._getChromeWindow(this._window).gSync.openPrefs("tabs-sidebar");
  },

  openConnectDevice() {
    this._getChromeWindow(this._window).gSync.openConnectAnotherDevice(
      "tabs-sidebar"
    );
  },
};
