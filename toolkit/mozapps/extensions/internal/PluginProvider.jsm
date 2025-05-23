/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported logger */

var EXPORTED_SYMBOLS = [];

const { AddonManager, AddonManagerPrivate } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "Blocklist",
  "resource://gre/modules/Blocklist.jsm"
);

const URI_EXTENSION_STRINGS =
  "chrome://mozapps/locale/extensions/extensions.properties";
const LIST_UPDATED_TOPIC = "plugins-list-updated";

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const LOGGER_ID = "addons.plugins";

// Create a new logger for use by the Addons Plugin Provider
// (Requires AddonManager.jsm)
var logger = Log.repository.getLogger(LOGGER_ID);

var PluginProvider = {
  get name() {
    return "PluginProvider";
  },

  // A dictionary mapping IDs to names and descriptions
  plugins: null,

  startup() {
    Services.obs.addObserver(this, LIST_UPDATED_TOPIC);
  },

  /**
   * Called when the application is shutting down. Only necessary for tests
   * to be able to simulate a shutdown.
   */
  shutdown() {
    this.plugins = null;
    Services.obs.removeObserver(this, LIST_UPDATED_TOPIC);
  },

  async observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case LIST_UPDATED_TOPIC:
        if (this.plugins) {
          this.updatePluginList();
        }
        break;
    }
  },

  /**
   * Creates a PluginWrapper for a plugin object.
   */
  buildWrapper(aPlugin) {
    return new PluginWrapper(
      aPlugin.id,
      aPlugin.name,
      aPlugin.description,
      aPlugin.tags
    );
  },

  /**
   * Called to get an Addon with a particular ID.
   *
   * @param  aId
   *         The ID of the add-on to retrieve
   */
  async getAddonByID(aId) {
    if (!this.plugins) {
      this.buildPluginList();
    }

    if (aId in this.plugins) {
      return this.buildWrapper(this.plugins[aId]);
    }
    return null;
  },

  /**
   * Called to get Addons of a particular type.
   *
   * @param  aTypes
   *         An array of types to fetch. Can be null to get all types.
   */
  async getAddonsByTypes(aTypes) {
    if (aTypes && !aTypes.includes("plugin")) {
      return [];
    }

    if (!this.plugins) {
      this.buildPluginList();
    }

    return Promise.all(
      Object.keys(this.plugins).map(id => this.getAddonByID(id))
    );
  },

  /**
   * Called to get the current AddonInstalls, optionally restricting by type.
   *
   * @param  aTypes
   *         An array of types or null to get all types
   */
  getInstallsByTypes(aTypes) {
    return [];
  },

  /**
   * Builds a list of the current plugins reported by the plugin host
   *
   * @return a dictionary of plugins indexed by our generated ID
   */
  getPluginList() {
    let tags = Cc["@mozilla.org/plugin/host;1"]
      .getService(Ci.nsIPluginHost)
      .getPluginTags();

    let list = {};
    let seenPlugins = {};
    for (let tag of tags) {
      if (!(tag.name in seenPlugins)) {
        seenPlugins[tag.name] = {};
      }
      if (!(tag.description in seenPlugins[tag.name])) {
        let plugin = {
          id: tag.name + tag.description,
          name: tag.name,
          description: tag.description,
          tags: [tag],
        };

        seenPlugins[tag.name][tag.description] = plugin;
        list[plugin.id] = plugin;
      } else {
        seenPlugins[tag.name][tag.description].tags.push(tag);
      }
    }

    return list;
  },

  /**
   * Builds the list of known plugins from the plugin host
   */
  buildPluginList() {
    this.plugins = this.getPluginList();
  },

  /**
   * Updates the plugins from the plugin host by comparing the current plugins
   * to the last known list sending out any necessary API notifications for
   * changes.
   */
  updatePluginList() {
    let newList = this.getPluginList();

    let lostPlugins = Object.keys(this.plugins)
      .filter(id => !(id in newList))
      .map(id => this.buildWrapper(this.plugins[id]));
    let newPlugins = Object.keys(newList)
      .filter(id => !(id in this.plugins))
      .map(id => this.buildWrapper(newList[id]));
    let matchedIDs = Object.keys(newList).filter(id => id in this.plugins);

    // The plugin host generates new tags for every plugin after a scan and
    // if the plugin's filename has changed then the disabled state won't have
    // been carried across, send out notifications for anything that has
    // changed (see bug 830267).
    let changedWrappers = [];
    for (let id of matchedIDs) {
      let oldWrapper = this.buildWrapper(this.plugins[id]);
      let newWrapper = this.buildWrapper(newList[id]);

      if (newWrapper.isActive != oldWrapper.isActive) {
        AddonManagerPrivate.callAddonListeners(
          newWrapper.isActive ? "onEnabling" : "onDisabling",
          newWrapper,
          false
        );
        changedWrappers.push(newWrapper);
      }
    }

    // Notify about new installs
    for (let plugin of newPlugins) {
      AddonManagerPrivate.callInstallListeners(
        "onExternalInstall",
        null,
        plugin,
        null,
        false
      );
      AddonManagerPrivate.callAddonListeners("onInstalling", plugin, false);
    }

    // Notify for any plugins that have vanished.
    for (let plugin of lostPlugins) {
      AddonManagerPrivate.callAddonListeners("onUninstalling", plugin, false);
    }

    this.plugins = newList;

    // Signal that new installs are complete
    for (let plugin of newPlugins) {
      AddonManagerPrivate.callAddonListeners("onInstalled", plugin);
    }

    // Signal that enables/disables are complete
    for (let wrapper of changedWrappers) {
      AddonManagerPrivate.callAddonListeners(
        wrapper.isActive ? "onEnabled" : "onDisabled",
        wrapper
      );
    }

    // Signal that uninstalls are complete
    for (let plugin of lostPlugins) {
      AddonManagerPrivate.callAddonListeners("onUninstalled", plugin);
    }
  },
};

const wrapperMap = new WeakMap();
let pluginFor = wrapper => wrapperMap.get(wrapper);

/**
 * The PluginWrapper wraps a set of nsIPluginTags to provide the data visible to
 * public callers through the API.
 */
function PluginWrapper(id, name, description, tags) {
  wrapperMap.set(this, { id, name, description, tags });
}

PluginWrapper.prototype = {
  get id() {
    return pluginFor(this).id;
  },

  get type() {
    return "plugin";
  },

  get name() {
    return pluginFor(this).name;
  },

  get creator() {
    return null;
  },

  get description() {
    return pluginFor(this).description.replace(/<\/?[a-z][^>]*>/gi, " ");
  },

  get version() {
    let {
      tags: [tag],
    } = pluginFor(this);
    return tag.version;
  },

  get homepageURL() {
    let { description } = pluginFor(this);
    if (/<A\s+HREF=[^>]*>/i.test(description)) {
      return /<A\s+HREF=["']?([^>"'\s]*)/i.exec(description)[1];
    }
    return null;
  },

  get isActive() {
    let {
      tags: [tag],
    } = pluginFor(this);
    return !tag.blocklisted && !tag.disabled;
  },

  get appDisabled() {
    let {
      tags: [tag],
    } = pluginFor(this);
    return tag.blocklisted;
  },

  get userDisabled() {
    let {
      tags: [tag],
    } = pluginFor(this);
    if (tag.disabled) {
      return true;
    }

    if (
      (Services.prefs.getBoolPref("plugins.click_to_play") &&
        tag.clicktoplay) ||
      this.blocklistState ==
        Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE ||
      this.blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE
    ) {
      return AddonManager.STATE_ASK_TO_ACTIVATE;
    }

    return false;
  },

  set userDisabled(val) {
    let previousVal = this.userDisabled;
    if (val === previousVal) {
      return val;
    }

    let { tags } = pluginFor(this);

    for (let tag of tags) {
      if (val === true) {
        tag.enabledState = Ci.nsIPluginTag.STATE_DISABLED;
      } else if (val === false) {
        tag.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
      } else if (val == AddonManager.STATE_ASK_TO_ACTIVATE) {
        tag.enabledState = Ci.nsIPluginTag.STATE_CLICKTOPLAY;
      }
    }

    // If 'userDisabled' was 'true' and we're going to a state that's not
    // that, we're enabling, so call those listeners.
    if (previousVal === true && val !== true) {
      AddonManagerPrivate.callAddonListeners("onEnabling", this, false);
      AddonManagerPrivate.callAddonListeners("onEnabled", this);
    }

    // If 'userDisabled' was not 'true' and we're going to a state where
    // it is, we're disabling, so call those listeners.
    if (previousVal !== true && val === true) {
      AddonManagerPrivate.callAddonListeners("onDisabling", this, false);
      AddonManagerPrivate.callAddonListeners("onDisabled", this);
    }

    // If the 'userDisabled' value involved AddonManager.STATE_ASK_TO_ACTIVATE,
    // call the onPropertyChanged listeners.
    if (
      previousVal == AddonManager.STATE_ASK_TO_ACTIVATE ||
      val == AddonManager.STATE_ASK_TO_ACTIVATE
    ) {
      AddonManagerPrivate.callAddonListeners("onPropertyChanged", this, [
        "userDisabled",
      ]);
    }

    return val;
  },

  async enable() {
    this.userDisabled = false;
  },
  async disable() {
    this.userDisabled = true;
  },

  get blocklistState() {
    let {
      tags: [tag],
    } = pluginFor(this);
    return tag.blocklistState;
  },

  async getBlocklistURL() {
    let {
      tags: [tag],
    } = pluginFor(this);
    return Blocklist.getPluginBlockURL(tag);
  },

  get pluginLibraries() {
    let libs = [];
    for (let tag of pluginFor(this).tags) {
      libs.push(tag.filename);
    }
    return libs;
  },

  get pluginFullpath() {
    let paths = [];
    for (let tag of pluginFor(this).tags) {
      paths.push(tag.fullpath);
    }
    return paths;
  },

  get pluginMimeTypes() {
    let types = [];
    for (let tag of pluginFor(this).tags) {
      let mimeTypes = tag.getMimeTypes();
      let mimeDescriptions = tag.getMimeDescriptions();
      let extensions = tag.getExtensions();
      for (let i = 0; i < mimeTypes.length; i++) {
        let type = {};
        type.type = mimeTypes[i];
        type.description = mimeDescriptions[i];
        type.suffixes = extensions[i];

        types.push(type);
      }
    }
    return types;
  },

  get installDate() {
    let date = 0;
    for (let tag of pluginFor(this).tags) {
      date = Math.max(date, tag.lastModifiedTime);
    }
    return new Date(date);
  },

  get scope() {
    let {
      tags: [tag],
    } = pluginFor(this);
    let path = tag.fullpath;
    // Plugins inside the application directory are in the application scope
    let dir = Services.dirsvc.get("APlugns", Ci.nsIFile);
    if (path.startsWith(dir.path)) {
      return AddonManager.SCOPE_APPLICATION;
    }

    // Plugins inside the profile directory are in the profile scope
    dir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    if (path.startsWith(dir.path)) {
      return AddonManager.SCOPE_PROFILE;
    }

    // Plugins anywhere else in the user's home are in the user scope,
    // but not all platforms have a home directory.
    try {
      dir = Services.dirsvc.get("Home", Ci.nsIFile);
      if (path.startsWith(dir.path)) {
        return AddonManager.SCOPE_USER;
      }
    } catch (e) {
      if (!e.result || e.result != Cr.NS_ERROR_FAILURE) {
        throw e;
      }
      // Do nothing: missing "Home".
    }

    // Any other locations are system scope
    return AddonManager.SCOPE_SYSTEM;
  },

  get pendingOperations() {
    return AddonManager.PENDING_NONE;
  },

  get operationsRequiringRestart() {
    return AddonManager.OP_NEEDS_RESTART_NONE;
  },

  get permissions() {
    let {
      tags: [tag],
    } = pluginFor(this);
    let permissions = 0;
    if (tag.isEnabledStateLocked) {
      return permissions;
    }
    if (!this.appDisabled) {
      if (this.userDisabled !== true) {
        permissions |= AddonManager.PERM_CAN_DISABLE;
      }

      let blocklistState = this.blocklistState;
      let isCTPBlocklisted =
        blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE ||
        blocklistState ==
          Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE;

      if (
        this.userDisabled !== AddonManager.STATE_ASK_TO_ACTIVATE &&
        (Services.prefs.getBoolPref("plugins.click_to_play") ||
          isCTPBlocklisted)
      ) {
        permissions |= AddonManager.PERM_CAN_ASK_TO_ACTIVATE;
      }

      if (this.userDisabled !== false && !isCTPBlocklisted) {
        permissions |= AddonManager.PERM_CAN_ENABLE;
      }
    }
    return permissions;
  },

  get optionsType() {
    return AddonManager.OPTIONS_TYPE_INLINE_BROWSER;
  },

  get optionsURL() {
    return (
      "chrome://mozapps/content/extensions/pluginPrefs.xhtml#id=" +
      encodeURIComponent(this.id)
    );
  },

  get updateDate() {
    return this.installDate;
  },

  get isCompatible() {
    return true;
  },

  get isPlatformCompatible() {
    return true;
  },

  get providesUpdatesSecurely() {
    return true;
  },

  get foreignInstall() {
    return true;
  },

  isCompatibleWith(aAppVersion, aPlatformVersion) {
    return true;
  },

  findUpdates(aListener, aReason, aAppVersion, aPlatformVersion) {
    if ("onNoCompatibilityUpdateAvailable" in aListener) {
      aListener.onNoCompatibilityUpdateAvailable(this);
    }
    if ("onNoUpdateAvailable" in aListener) {
      aListener.onNoUpdateAvailable(this);
    }
    if ("onUpdateFinished" in aListener) {
      aListener.onUpdateFinished(this);
    }
  },
};

AddonManagerPrivate.registerProvider(PluginProvider, [
  new AddonManagerPrivate.AddonType(
    "plugin",
    URI_EXTENSION_STRINGS,
    "type.plugin.name",
    AddonManager.VIEW_TYPE_LIST,
    6000,
    AddonManager.TYPE_SUPPORTS_ASK_TO_ACTIVATE
  ),
]);
