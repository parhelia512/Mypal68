/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Contains functions shared by different Login Manager components.
 *
 * This JavaScript module exists in order to share code between the different
 * XPCOM components that constitute the Login Manager, including implementations
 * of nsILoginManager and nsILoginManagerStorage.
 */

"use strict";

const EXPORTED_SYMBOLS = ["LoginHelper"];

// Globals

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

/**
 * Contains functions shared by different Login Manager components.
 */
this.LoginHelper = {
  debug: null,
  enabled: null,
  formlessCaptureEnabled: null,
  generationAvailable: null,
  generationEnabled: null,
  insecureAutofill: null,
  managementURI: null,
  privateBrowsingCaptureEnabled: null,
  schemeUpgrades: null,
  showAutoCompleteFooter: null,

  init() {
    // Watch for pref changes to update cached pref values.
    Services.prefs.addObserver("signon.", () => this.updateSignonPrefs());
    this.updateSignonPrefs();
    Services.telemetry.setEventRecordingEnabled("pwmgr", true);
  },

  updateSignonPrefs() {
    this.autofillForms = Services.prefs.getBoolPref("signon.autofillForms");
    this.autofillAutocompleteOff = Services.prefs.getBoolPref(
      "signon.autofillForms.autocompleteOff"
    );
    this.debug = Services.prefs.getBoolPref("signon.debug");
    this.enabled = Services.prefs.getBoolPref("signon.rememberSignons");
    this.formlessCaptureEnabled = Services.prefs.getBoolPref(
      "signon.formlessCapture.enabled"
    );
    this.generationAvailable = Services.prefs.getBoolPref(
      "signon.generation.available"
    );
    this.generationEnabled = Services.prefs.getBoolPref(
      "signon.generation.enabled"
    );
    this.insecureAutofill = Services.prefs.getBoolPref(
      "signon.autofillForms.http"
    );
    this.managementURI = Services.prefs.getStringPref(
      "signon.management.overrideURI",
      null
    );
    this.privateBrowsingCaptureEnabled = Services.prefs.getBoolPref(
      "signon.privateBrowsingCapture.enabled"
    );
    this.schemeUpgrades = Services.prefs.getBoolPref("signon.schemeUpgrades");
    this.showAutoCompleteFooter = Services.prefs.getBoolPref(
      "signon.showAutoCompleteFooter"
    );
    this.storeWhenAutocompleteOff = Services.prefs.getBoolPref(
      "signon.storeWhenAutocompleteOff"
    );
  },

  createLogger(aLogPrefix) {
    let getMaxLogLevel = () => {
      return this.debug ? "Debug" : "Warn";
    };

    // Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
    let consoleOptions = {
      maxLogLevel: getMaxLogLevel(),
      prefix: aLogPrefix,
    };
    let logger = console.createInstance(consoleOptions);

    // Watch for pref changes and update this.debug and the maxLogLevel for created loggers
    Services.prefs.addObserver("signon.debug", () => {
      this.debug = Services.prefs.getBoolPref("signon.debug");
      if (logger) {
        logger.maxLogLevel = getMaxLogLevel();
      }
    });

    return logger;
  },

  /**
   * Due to the way the signons2.txt file is formatted, we need to make
   * sure certain field values or characters do not cause the file to
   * be parsed incorrectly.  Reject origins that we can't store correctly.
   *
   * @throws String with English message in case validation failed.
   */
  checkOriginValue(aOrigin) {
    // Nulls are invalid, as they don't round-trip well.  Newlines are also
    // invalid for any field stored as plaintext, and an origin made of a
    // single dot cannot be stored in the legacy format.
    if (
      aOrigin == "." ||
      aOrigin.includes("\r") ||
      aOrigin.includes("\n") ||
      aOrigin.includes("\0")
    ) {
      throw new Error("Invalid origin");
    }
  },

  /**
   * Due to the way the signons2.txt file is formatted, we need to make
   * sure certain field values or characters do not cause the file to
   * be parsed incorrectly.  Reject logins that we can't store correctly.
   *
   * @throws String with English message in case validation failed.
   */
  checkLoginValues(aLogin) {
    function badCharacterPresent(l, c) {
      return (
        (l.formActionOrigin && l.formActionOrigin.includes(c)) ||
        (l.httpRealm && l.httpRealm.includes(c)) ||
        l.origin.includes(c) ||
        l.usernameField.includes(c) ||
        l.passwordField.includes(c)
      );
    }

    // Nulls are invalid, as they don't round-trip well.
    // Mostly not a formatting problem, although ".\0" can be quirky.
    if (badCharacterPresent(aLogin, "\0")) {
      throw new Error("login values can't contain nulls");
    }

    // In theory these nulls should just be rolled up into the encrypted
    // values, but nsISecretDecoderRing doesn't use nsStrings, so the
    // nulls cause truncation. Check for them here just to avoid
    // unexpected round-trip surprises.
    if (aLogin.username.includes("\0") || aLogin.password.includes("\0")) {
      throw new Error("login values can't contain nulls");
    }

    // Newlines are invalid for any field stored as plaintext.
    if (
      badCharacterPresent(aLogin, "\r") ||
      badCharacterPresent(aLogin, "\n")
    ) {
      throw new Error("login values can't contain newlines");
    }

    // A line with just a "." can have special meaning.
    if (aLogin.usernameField == "." || aLogin.formActionOrigin == ".") {
      throw new Error("login values can't be periods");
    }

    // An origin with "\ \(" won't roundtrip.
    // eg host="foo (", realm="bar" --> "foo ( (bar)"
    // vs host="foo", realm=" (bar" --> "foo ( (bar)"
    if (aLogin.origin.includes(" (")) {
      throw new Error("bad parens in origin");
    }
  },

  /**
   * Returns a new XPCOM property bag with the provided properties.
   *
   * @param {Object} aProperties
   *        Each property of this object is copied to the property bag.  This
   *        parameter can be omitted to return an empty property bag.
   *
   * @return A new property bag, that is an instance of nsIWritablePropertyBag,
   *         nsIWritablePropertyBag2, nsIPropertyBag, and nsIPropertyBag2.
   */
  newPropertyBag(aProperties) {
    let propertyBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
      Ci.nsIWritablePropertyBag
    );
    if (aProperties) {
      for (let [name, value] of Object.entries(aProperties)) {
        propertyBag.setProperty(name, value);
      }
    }
    return propertyBag
      .QueryInterface(Ci.nsIPropertyBag)
      .QueryInterface(Ci.nsIPropertyBag2)
      .QueryInterface(Ci.nsIWritablePropertyBag2);
  },

  /**
   * Helper to avoid the property bags when calling
   * Services.logins.searchLogins from JS.
   *
   * @param {Object} aSearchOptions - A regular JS object to copy to a property bag before searching
   * @return {nsILoginInfo[]} - The result of calling searchLogins.
   */
  searchLoginsWithObject(aSearchOptions) {
    return Services.logins.searchLogins(this.newPropertyBag(aSearchOptions));
  },

  /**
   * @param {string} aURL
   * @returns {string} which is the hostPort of aURL if supported by the scheme
   *                   otherwise, returns the original aURL.
   */
  maybeGetHostPortForURL(aURL) {
    try {
      let uri = Services.io.newURI(aURL);
      return uri.hostPort;
    } catch (ex) {
      // No need to warn for javascript:/data:/about:/chrome:/etc.
    }
    return aURL;
  },

  /**
   * Get the parts of the URL we want for identification.
   * Strip out things like the userPass portion and handle javascript:.
   */
  getLoginOrigin(uriString, allowJS) {
    let realm = "";
    try {
      let uri = Services.io.newURI(uriString);

      if (allowJS && uri.scheme == "javascript") {
        return "javascript:";
      }
      // TODO: Bug 1559205 - Add support for moz-proxy

      // Build this manually instead of using prePath to avoid including the userPass portion.
      realm = uri.scheme + "://" + uri.displayHostPort;
    } catch (e) {
      // bug 159484 - disallow url types that don't support a hostPort.
      // (although we handle "javascript:..." as a special case above.)
      log.warn("Couldn't parse origin for", uriString, e);
      realm = null;
    }

    return realm;
  },

  getFormActionOrigin(form) {
    let uriString = form.action;

    // A blank or missing action submits to where it came from.
    if (uriString == "") {
      // ala bug 297761
      uriString = form.baseURI;
    }

    return this.getLoginOrigin(uriString, true);
  },

  /**
   * @param {String} aLoginOrigin - An origin value from a stored login's
   *                                origin or formActionOrigin properties.
   * @param {String} aSearchOrigin - The origin that was are looking to match
   *                                 with aLoginOrigin. This would normally come
   *                                 from a form or page that we are considering.
   * @param {nsILoginFindOptions} aOptions - Options to affect whether the origin
   *                                         from the login (aLoginOrigin) is a
   *                                         match for the origin we're looking
   *                                         for (aSearchOrigin).
   */
  isOriginMatching(
    aLoginOrigin,
    aSearchOrigin,
    aOptions = {
      schemeUpgrades: false,
      acceptWildcardMatch: false,
    }
  ) {
    if (aLoginOrigin == aSearchOrigin) {
      return true;
    }

    if (!aOptions) {
      return false;
    }

    if (aOptions.acceptWildcardMatch && aLoginOrigin == "") {
      return true;
    }

    if (aOptions.schemeUpgrades) {
      try {
        let loginURI = Services.io.newURI(aLoginOrigin);
        let searchURI = Services.io.newURI(aSearchOrigin);
        if (loginURI.scheme == "http" && searchURI.scheme == "https" &&
            loginURI.hostPort == searchURI.hostPort) {
          return true;
        }
      } catch (ex) {
        // newURI will throw for some values e.g. chrome://FirefoxAccounts
        return false;
      }
    }

    return false;
  },

  doLoginsMatch(
    aLogin1,
    aLogin2,
    { ignorePassword = false, ignoreSchemes = false }
  ) {
    if (
      aLogin1.httpRealm != aLogin2.httpRealm ||
      aLogin1.username != aLogin2.username
    ) {
      return false;
    }

    if (!ignorePassword && aLogin1.password != aLogin2.password) {
      return false;
    }

    if (ignoreSchemes) {
      let login1HostPort = this.maybeGetHostPortForURL(aLogin1.origin);
      let login2HostPort = this.maybeGetHostPortForURL(aLogin2.origin);
      if (login1HostPort != login2HostPort) {
        return false;
      }

      if (
        aLogin1.formActionOrigin != "" &&
        aLogin2.formActionOrigin != "" &&
        this.maybeGetHostPortForURL(aLogin1.formActionOrigin) !=
          this.maybeGetHostPortForURL(aLogin2.formActionOrigin)
      ) {
        return false;
      }
    } else {
      if (aLogin1.origin != aLogin2.origin) {
        return false;
      }

      // If either formActionOrigin is blank (but not null), then match.
      if (
        aLogin1.formActionOrigin != "" &&
        aLogin2.formActionOrigin != "" &&
        aLogin1.formActionOrigin != aLogin2.formActionOrigin
      ) {
        return false;
      }
    }

    // The .usernameField and .passwordField values are ignored.

    return true;
  },

  /**
   * Creates a new login object that results by modifying the given object with
   * the provided data.
   *
   * @param aOldStoredLogin
   *        Existing nsILoginInfo object to modify.
   * @param aNewLoginData
   *        The new login values, either as nsILoginInfo or nsIProperyBag.
   *
   * @return The newly created nsILoginInfo object.
   *
   * @throws String with English message in case validation failed.
   */
  buildModifiedLogin(aOldStoredLogin, aNewLoginData) {
    function bagHasProperty(aPropName) {
      try {
        aNewLoginData.getProperty(aPropName);
        return true;
      } catch (ex) {}
      return false;
    }

    aOldStoredLogin.QueryInterface(Ci.nsILoginMetaInfo);

    let newLogin;
    if (aNewLoginData instanceof Ci.nsILoginInfo) {
      // Clone the existing login to get its nsILoginMetaInfo, then init it
      // with the replacement nsILoginInfo data from the new login.
      newLogin = aOldStoredLogin.clone();
      newLogin.init(
        aNewLoginData.origin,
        aNewLoginData.formActionOrigin,
        aNewLoginData.httpRealm,
        aNewLoginData.username,
        aNewLoginData.password,
        aNewLoginData.usernameField,
        aNewLoginData.passwordField
      );
      newLogin.QueryInterface(Ci.nsILoginMetaInfo);

      // Automatically update metainfo when password is changed.
      if (newLogin.password != aOldStoredLogin.password) {
        newLogin.timePasswordChanged = Date.now();
      }
    } else if (aNewLoginData instanceof Ci.nsIPropertyBag) {
      // Clone the existing login, along with all its properties.
      newLogin = aOldStoredLogin.clone();
      newLogin.QueryInterface(Ci.nsILoginMetaInfo);

      // Automatically update metainfo when password is changed.
      // (Done before the main property updates, lest the caller be
      // explicitly updating both .password and .timePasswordChanged)
      if (bagHasProperty("password")) {
        let newPassword = aNewLoginData.getProperty("password");
        if (newPassword != aOldStoredLogin.password) {
          newLogin.timePasswordChanged = Date.now();
        }
      }

      for (let prop of aNewLoginData.enumerator) {
        switch (prop.name) {
          // nsILoginInfo (fall through)
          case "origin":
          case "httpRealm":
          case "formActionOrigin":
          case "username":
          case "password":
          case "usernameField":
          case "passwordField":
          // nsILoginMetaInfo (fall through)
          case "guid":
          case "timeCreated":
          case "timeLastUsed":
          case "timePasswordChanged":
          case "timesUsed":
            newLogin[prop.name] = prop.value;
            break;

          // Fake property, allows easy incrementing.
          case "timesUsedIncrement":
            newLogin.timesUsed += prop.value;
            break;

          // Fail if caller requests setting an unknown property.
          default:
            throw new Error("Unexpected propertybag item: " + prop.name);
        }
      }
    } else {
      throw new Error("newLoginData needs an expected interface!");
    }

    // Sanity check the login
    if (newLogin.origin == null || newLogin.origin.length == 0) {
      throw new Error("Can't add a login with a null or empty origin.");
    }

    // For logins w/o a username, set to "", not null.
    if (newLogin.username == null) {
      throw new Error("Can't add a login with a null username.");
    }

    if (newLogin.password == null || newLogin.password.length == 0) {
      throw new Error("Can't add a login with a null or empty password.");
    }

    if (newLogin.formActionOrigin || newLogin.formActionOrigin == "") {
      // We have a form submit URL. Can't have a HTTP realm.
      if (newLogin.httpRealm != null) {
        throw new Error(
          "Can't add a login with both a httpRealm and formActionOrigin."
        );
      }
    } else if (newLogin.httpRealm) {
      // We have a HTTP realm. Can't have a form submit URL.
      if (newLogin.formActionOrigin != null) {
        throw new Error(
          "Can't add a login with both a httpRealm and formActionOrigin."
        );
      }
    } else {
      // Need one or the other!
      throw new Error(
        "Can't add a login without a httpRealm or formActionOrigin."
      );
    }

    // Throws if there are bogus values.
    this.checkLoginValues(newLogin);

    return newLogin;
  },

  /**
   * Removes duplicates from a list of logins while preserving the sort order.
   *
   * @param {nsILoginInfo[]} logins
   *        A list of logins we want to deduplicate.
   * @param {string[]} [uniqueKeys = ["username", "password"]]
   *        A list of login attributes to use as unique keys for the deduplication.
   * @param {string[]} [resolveBy = ["timeLastUsed"]]
   *        Ordered array of keyword strings used to decide which of the
   *        duplicates should be used. "scheme" would prefer the login that has
   *        a scheme matching `preferredOrigin`'s if there are two logins with
   *        the same `uniqueKeys`. The default preference to distinguish two
   *        logins is `timeLastUsed`. If there is no preference between two
   *        logins, the first one found wins.
   * @param {string} [preferredOrigin = undefined]
   *        String representing the origin to use for preferring one login over
   *        another when they are dupes. This is used with "scheme" for
   *        `resolveBy` so the scheme from this origin will be preferred.
   * @param {string} [preferredFormActionOrigin = undefined]
   *        String representing the action origin to use for preferring one login over
   *        another when they are dupes. This is used with "actionOrigin" for
   *        `resolveBy` so the scheme from this action origin will be preferred.
   *
   * @returns {nsILoginInfo[]} list of unique logins.
   */
  dedupeLogins(
    logins,
    uniqueKeys = ["username", "password"],
    resolveBy = ["timeLastUsed"],
    preferredOrigin = undefined,
    preferredFormActionOrigin = undefined
  ) {
    const KEY_DELIMITER = ":";

    if (!preferredOrigin && resolveBy.includes("scheme")) {
      throw new Error("dedupeLogins: `preferredOrigin` is required in order to " +
                      "prefer schemes which match it.");
    }

    let preferredOriginScheme;
    if (preferredOrigin) {
      try {
        preferredOriginScheme = Services.io.newURI(preferredOrigin).scheme;
      } catch (ex) {
        // Handle strings that aren't valid URIs e.g. chrome://FirefoxAccounts
      }
    }

    if (!preferredOriginScheme && resolveBy.includes("scheme")) {
      log.warn(
        "dedupeLogins: Deduping with a scheme preference but couldn't " +
          "get the preferred origin scheme."
      );
    }

    // We use a Map to easily lookup logins by their unique keys.
    let loginsByKeys = new Map();

    // Generate a unique key string from a login.
    function getKey(login, uniqueKeys) {
      return uniqueKeys.reduce((prev, key) => prev + KEY_DELIMITER + login[key], "");
    }

    /**
     * @return {bool} whether `login` is preferred over its duplicate (considering `uniqueKeys`)
     *                `existingLogin`.
     *
     * `resolveBy` is a sorted array so we can return true the first time `login` is preferred
     * over the existingLogin.
     */
    function isLoginPreferred(existingLogin, login) {
      if (!resolveBy || resolveBy.length == 0) {
        // If there is no preference, prefer the existing login.
        return false;
      }

      for (let preference of resolveBy) {
        switch (preference) {
          case "actionOrigin": {
            if (!preferredFormActionOrigin) {
              break;
            }
            if (
              LoginHelper.isOriginMatching(
                existingLogin.formActionOrigin,
                preferredFormActionOrigin,
                { schemeUpgrades: LoginHelper.schemeUpgrades }
              ) &&
              !LoginHelper.isOriginMatching(
                login.formActionOrigin,
                preferredFormActionOrigin,
                { schemeUpgrades: LoginHelper.schemeUpgrades }
              )
            ) {
              return false;
            }
            break;
          }
          case "scheme": {
            if (!preferredOriginScheme) {
              break;
            }

            try {
              // Only `origin` is currently considered
              let existingLoginURI = Services.io.newURI(existingLogin.origin);
              let loginURI = Services.io.newURI(login.origin);
              // If the schemes of the two logins are the same or neither match the
              // preferredOriginScheme then we have no preference and look at the next resolveBy.
              if (
                loginURI.scheme == existingLoginURI.scheme ||
                (loginURI.scheme != preferredOriginScheme &&
                  existingLoginURI.scheme != preferredOriginScheme)
              ) {
                break;
              }

              return loginURI.scheme == preferredOriginScheme;
            } catch (ex) {
              // Some URLs aren't valid nsIURI (e.g. chrome://FirefoxAccounts)
              log.debug(
                "dedupeLogins/shouldReplaceExisting: Error comparing schemes:",
                existingLogin.origin,
                login.origin,
                "preferredOrigin:",
                preferredOrigin,
                ex
              );
            }
            break;
          }
          case "timeLastUsed":
          case "timePasswordChanged": {
            // If we find a more recent login for the same key, replace the existing one.
            let loginDate = login.QueryInterface(Ci.nsILoginMetaInfo)[
              preference
            ];
            let storedLoginDate = existingLogin.QueryInterface(
              Ci.nsILoginMetaInfo
            )[preference];
            if (loginDate == storedLoginDate) {
              break;
            }

            return loginDate > storedLoginDate;
          }
          default: {
            throw new Error(
              "dedupeLogins: Invalid resolveBy preference: " + preference
            );
          }
        }
      }

      return false;
    }

    for (let login of logins) {
      let key = getKey(login, uniqueKeys);

      if (loginsByKeys.has(key)) {
        if (!isLoginPreferred(loginsByKeys.get(key), login)) {
          // If there is no preference for the new login, use the existing one.
          continue;
        }
      }
      loginsByKeys.set(key, login);
    }

    // Return the map values in the form of an array.
    return [...loginsByKeys.values()];
  },

  /**
   * Open the password manager window.
   *
   * @param {Window} window
   *                 the window from where we want to open the dialog
   *
   * @param {object?} args
   *                  params for opening the password manager
   * @param {string} [args.filterString=""]
   *                 the domain (not origin) to pass to the login manager dialog
   *                 to pre-filter the results
   * @param {string} args.entryPoint
   *                 The name of the entry point, used for telemetry
   */
  openPasswordManager(window, { filterString = "", entryPoint = "" } = {}) {
    Services.telemetry.recordEvent("pwmgr", "open_management", entryPoint);
    if (this.managementURI && window.openTrustedLinkIn) {
      let managementURL = this.managementURI.replace(
        "%DOMAIN%",
        window.encodeURIComponent(filterString)
      );
      window.openTrustedLinkIn(managementURL, "tab");
      return;
    }
    let win = Services.wm.getMostRecentWindow("Toolkit:PasswordManager");
    if (win) {
      win.setFilter(filterString);
      win.focus();
    } else {
      window.openDialog(
        "chrome://passwordmgr/content/passwordManager.xhtml",
        "Toolkit:PasswordManager",
        "",
        { filterString }
      );
    }
  },

  /**
   * Checks if a field type is username compatible.
   *
   * @param {Element} element
   *                  the field we want to check.
   *
   * @returns {Boolean} true if the field type is one
   *                    of the username types.
   */
  isUsernameFieldType(element) {
    if (ChromeUtils.getClassName(element) !== "HTMLInputElement") {
      return false;
    }

    if (!element.isConnected) {
      // If the element isn't connected then it isn't visible to the user so
      // shouldn't be considered. It must have been connected in the past.
      return false;
    }

    let fieldType = element.hasAttribute("type")
      ? element.getAttribute("type").toLowerCase()
      : element.type;
    if (
      !(
        fieldType == "text" ||
        fieldType == "email" ||
        fieldType == "url" ||
        fieldType == "tel" ||
        fieldType == "number"
      )
    ) {
      return false;
    }

    let acFieldName = element.getAutocompleteInfo().fieldName;
    if (
      !(
        acFieldName == "username" ||
        // Bug 1540154: Some sites use tel/email on their username fields.
        acFieldName == "email" ||
        acFieldName == "tel" ||
        acFieldName == "tel-national" ||
        acFieldName == "off" ||
        acFieldName == "on" ||
        acFieldName == ""
      )
    ) {
      return false;
    }
    return true;
  },

  /**
   * For each login, add the login to the password manager if a similar one
   * doesn't already exist. Merge it otherwise with the similar existing ones.
   *
   * @param {Object[]} loginDatas - For each login, the data that needs to be added.
   * @returns {nsILoginInfo[]} the newly added logins, filtered if no login was added.
   */
  async maybeImportLogins(loginDatas) {
    let loginsToAdd = [];
    let loginMap = new Map();
    for (let loginData of loginDatas) {
      // create a new login
      let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
        Ci.nsILoginInfo
      );
      login.init(
        loginData.origin,
        loginData.formActionOrigin ||
          (typeof loginData.httpRealm == "string" ? null : ""),
        typeof loginData.httpRealm == "string" ? loginData.httpRealm : null,
        loginData.username,
        loginData.password,
        loginData.usernameElement || "",
        loginData.passwordElement || ""
      );

      login.QueryInterface(Ci.nsILoginMetaInfo);
      login.timeCreated = loginData.timeCreated;
      login.timeLastUsed = loginData.timeLastUsed || loginData.timeCreated;
      login.timePasswordChanged =
        loginData.timePasswordChanged || loginData.timeCreated;
      login.timesUsed = loginData.timesUsed || 1;

      try {
        // Ensure we only send checked logins through, since the validation is optimized
        // out from the bulk APIs below us.
        this.checkLoginValues(login);
      } catch (e) {
        Cu.reportError(e);
        continue;
      }

      // First, we need to check the logins that we've already decided to add, to
      // see if this is a duplicate. This should mirror the logic below for
      // existingLogins, but only for the array of logins we're adding.
      let newLogins = loginMap.get(login.origin) || [];
      if (!newLogins) {
        loginMap.set(login.origin, newLogins);
      } else {
        if (newLogins.some(l => login.matches(l, false /* ignorePassword */))) {
          continue;
        }
        let foundMatchingNewLogin = false;
        for (let newLogin of newLogins) {
          if (login.username == newLogin.username) {
            foundMatchingNewLogin = true;
            newLogin.QueryInterface(Ci.nsILoginMetaInfo);
            if (
              (login.password != newLogin.password) &
              (login.timePasswordChanged > newLogin.timePasswordChanged)
            ) {
              // if a login with the same username and different password already exists and it's older
              // than the current one, update its password and timestamp.
              newLogin.password = login.password;
              newLogin.timePasswordChanged = login.timePasswordChanged;
            }
          }
        }

        if (foundMatchingNewLogin) {
          continue;
        }
      }

      // While here we're passing formActionOrigin and httpRealm, they could be empty/null and get
      // ignored in that case, leading to multiple logins for the same username.
      let existingLogins = Services.logins.findLogins(
        login.origin,
        login.formActionOrigin,
        login.httpRealm
      );
      // Check for an existing login that matches *including* the password.
      // If such a login exists, we do not need to add a new login.
      if (
        existingLogins.some(l => login.matches(l, false /* ignorePassword */))
      ) {
        continue;
      }
      // Now check for a login with the same username, where it may be that we have an
      // updated password.
      let foundMatchingLogin = false;
      for (let existingLogin of existingLogins) {
        if (login.username == existingLogin.username) {
          foundMatchingLogin = true;
          existingLogin.QueryInterface(Ci.nsILoginMetaInfo);
          if (
            (login.password != existingLogin.password) &
            (login.timePasswordChanged > existingLogin.timePasswordChanged)
          ) {
            // if a login with the same username and different password already exists and it's older
            // than the current one, update its password and timestamp.
            let propBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
              Ci.nsIWritablePropertyBag
            );
            propBag.setProperty("password", login.password);
            propBag.setProperty(
              "timePasswordChanged",
              login.timePasswordChanged
            );
            Services.logins.modifyLogin(existingLogin, propBag);
          }
        }
      }
      // if the new login is an update or is older than an exiting login, don't add it.
      if (foundMatchingLogin) {
        continue;
      }

      newLogins.push(login);
      loginsToAdd.push(login);
    }
    if (!loginsToAdd.length) {
      return [];
    }
    return Services.logins.addLogins(loginsToAdd);
  },

  /**
   * Convert an array of nsILoginInfo to vanilla JS objects suitable for
   * sending over IPC.
   *
   * NB: All members of nsILoginInfo and nsILoginMetaInfo are strings.
   */
  loginsToVanillaObjects(logins) {
    return logins.map(this.loginToVanillaObject);
  },

  /**
   * Same as above, but for a single login.
   */
  loginToVanillaObject(login) {
    let obj = {};
    for (let i in login.QueryInterface(Ci.nsILoginMetaInfo)) {
      if (typeof login[i] !== "function") {
        obj[i] = login[i];
      }
    }

    return obj;
  },

  /**
   * Convert an object received from IPC into an nsILoginInfo (with guid).
   */
  vanillaObjectToLogin(login) {
    let formLogin = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(
      Ci.nsILoginInfo
    );
    // For compatibility for the Lockwise extension we fallback to hostname/formSubmitURL
    formLogin.init(
      login.origin || login.hostname,
      "formSubmitURL" in login ? login.formSubmitURL : login.formActionOrigin,
      login.httpRealm,
      login.username,
      login.password,
      login.usernameField,
      login.passwordField
    );

    formLogin.QueryInterface(Ci.nsILoginMetaInfo);
    for (let prop of [
      "guid",
      "timeCreated",
      "timeLastUsed",
      "timePasswordChanged",
      "timesUsed",
    ]) {
      formLogin[prop] = login[prop];
    }
    return formLogin;
  },

  /**
   * As above, but for an array of objects.
   */
  vanillaObjectsToLogins(logins) {
    return logins.map(this.vanillaObjectToLogin);
  },

  /**
   * Returns true if the user has a master password set and false otherwise.
   */
  isMasterPasswordSet() {
    let tokenDB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
      Ci.nsIPK11TokenDB
    );
    let token = tokenDB.getInternalKeyToken();
    return token.hasPassword;
  },

  /**
   * Send a notification when stored data is changed.
   */
  notifyStorageChanged(changeType, data) {
    let dataObject = data;
    // Can't pass a raw JS string or array though notifyObservers(). :-(
    if (Array.isArray(data)) {
      dataObject = Cc["@mozilla.org/array;1"].createInstance(
        Ci.nsIMutableArray
      );
      for (let i = 0; i < data.length; i++) {
        dataObject.appendElement(data[i]);
      }
    } else if (typeof data == "string") {
      dataObject = Cc["@mozilla.org/supports-string;1"].createInstance(
        Ci.nsISupportsString
      );
      dataObject.data = data;
    }
    Services.obs.notifyObservers(
      dataObject,
      "passwordmgr-storage-changed",
      changeType
    );
  },
};

LoginHelper.init();

XPCOMUtils.defineLazyPreferenceGetter(
  LoginHelper,
  "showInsecureFieldWarning",
  "security.insecure_field_warning.contextual.enabled"
);

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let logger = LoginHelper.createLogger("LoginHelper");
  return logger;
});
