/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionContent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionProcessScript: "resource://gre/modules/ExtensionProcessScript.jsm",
  LanguageDetector: "resource:///modules/translation/LanguageDetector.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "styleSheetService",
  "@mozilla.org/content/style-sheet-service;1",
  "nsIStyleSheetService"
);

const Timer = Components.Constructor(
  "@mozilla.org/timer;1",
  "nsITimer",
  "initWithCallback"
);

const { ExtensionChild } = ChromeUtils.import(
  "resource://gre/modules/ExtensionChild.jsm"
);
const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["crypto", "TextEncoder"]);

const {
  DefaultMap,
  DefaultWeakMap,
  getInnerWindowID,
  getWinUtils,
  promiseDocumentIdle,
  promiseDocumentLoaded,
  promiseDocumentReady,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
  defineLazyGetter,
  runSafeSyncWithoutClone,
} = ExtensionCommon;

const { BrowserExtensionContent, ChildAPIManager, Messenger } = ExtensionChild;

XPCOMUtils.defineLazyGetter(this, "console", ExtensionCommon.getConsole);

XPCOMUtils.defineLazyGetter(this, "isContentScriptProcess", () => {
  return (
    Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT ||
    !WebExtensionPolicy.useRemoteWebExtensions
  );
});

var DocumentManager;

const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";

var apiManager = new (class extends SchemaAPIManager {
  constructor() {
    super("content", Schemas);
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let { value } of Services.catMan.enumerateCategory(
        CATEGORY_EXTENSION_SCRIPTS_CONTENT
      )) {
        this.loadScript(value);
      }
    }
  }
})();

const SCRIPT_EXPIRY_TIMEOUT_MS = 5 * 60 * 1000;
const SCRIPT_CLEAR_TIMEOUT_MS = 5 * 1000;

const CSS_EXPIRY_TIMEOUT_MS = 30 * 60 * 1000;
const CSSCODE_EXPIRY_TIMEOUT_MS = 10 * 60 * 1000;

const scriptCaches = new WeakSet();
const sheetCacheDocuments = new DefaultWeakMap(() => new WeakSet());

class CacheMap extends DefaultMap {
  constructor(timeout, getter, extension) {
    super(getter);

    this.expiryTimeout = timeout;

    scriptCaches.add(this);

    // This ensures that all the cached scripts and stylesheets are deleted
    // from the cache and the xpi is no longer actively used.
    // See Bug 1435100 for rationale.
    extension.once("shutdown", () => {
      this.clear(-1);
    });
  }

  get(url) {
    let promise = super.get(url);

    promise.lastUsed = Date.now();
    if (promise.timer) {
      promise.timer.cancel();
    }
    promise.timer = Timer(
      this.delete.bind(this, url),
      this.expiryTimeout,
      Ci.nsITimer.TYPE_ONE_SHOT
    );

    return promise;
  }

  delete(url) {
    if (this.has(url)) {
      super.get(url).timer.cancel();
    }

    super.delete(url);
  }

  clear(timeout = SCRIPT_CLEAR_TIMEOUT_MS) {
    let now = Date.now();
    for (let [url, promise] of this.entries()) {
      // Delete the entry if expired or if clear has been called with timeout -1
      // (which is used to force the cache to clear all the entries, e.g. when the
      // extension is shutting down).
      if (timeout === -1 || now - promise.lastUsed >= timeout) {
        this.delete(url);
      }
    }
  }
}

class ScriptCache extends CacheMap {
  constructor(options, extension) {
    super(SCRIPT_EXPIRY_TIMEOUT_MS, null, extension);
    this.options = options;
  }

  defaultConstructor(url) {
    let promise = ChromeUtils.compileScript(url, this.options);
    promise.then(script => {
      promise.script = script;
    });
    return promise;
  }
}

/**
 * Shared base class for the two specialized CSS caches:
 * CSSCache (for the "url"-based stylesheets) and CSSCodeCache
 * (for the stylesheet defined by plain CSS content as a string).
 */
class BaseCSSCache extends CacheMap {
  constructor(expiryTimeout, defaultConstructor, extension) {
    super(expiryTimeout, defaultConstructor, extension);
  }

  addDocument(key, document) {
    sheetCacheDocuments.get(this.get(key)).add(document);
  }

  deleteDocument(key, document) {
    sheetCacheDocuments.get(this.get(key)).delete(document);
  }

  delete(key) {
    if (this.has(key)) {
      let promise = this.get(key);

      // Never remove a sheet from the cache if it's still being used by a
      // document. Rule processors can be shared between documents with the
      // same preloaded sheet, so we only lose by removing them while they're
      // still in use.
      let docs = ChromeUtils.nondeterministicGetWeakSetKeys(
        sheetCacheDocuments.get(promise)
      );
      if (docs.length) {
        return;
      }
    }

    super.delete(key);
  }
}

/**
 * Cache of the preloaded stylesheet defined by url.
 */
class CSSCache extends BaseCSSCache {
  constructor(sheetType, extension) {
    super(
      CSS_EXPIRY_TIMEOUT_MS,
      url => {
        let uri = Services.io.newURI(url);
        return styleSheetService
          .preloadSheetAsync(uri, sheetType)
          .then(sheet => {
            return { url, sheet };
          });
      },
      extension
    );
  }
}

/**
 * Cache of the preloaded stylesheet defined by plain CSS content as a string,
 * the key of the cached stylesheet is the hash of its "CSSCode" string.
 */
class CSSCodeCache extends BaseCSSCache {
  constructor(sheetType, extension) {
    super(
      CSSCODE_EXPIRY_TIMEOUT_MS,
      hash => {
        if (!this.has(hash)) {
          // Do not allow the getter to be used to lazily create the cached stylesheet,
          // the cached CSSCode stylesheet has to be explicitly set.
          throw new Error(
            "Unexistent cached cssCode stylesheet: " + Error().stack
          );
        }

        return super.get(hash);
      },
      extension
    );

    // Store the preferred sheetType (used to preload the expected stylesheet type in
    // the addCSSCode method).
    this.sheetType = sheetType;
  }

  addCSSCode(hash, cssCode) {
    if (this.has(hash)) {
      // This cssCode have been already cached, no need to create it again.
      return;
    }
    const uri = Services.io.newURI(
      "data:text/css;charset=utf-8," + encodeURIComponent(cssCode)
    );
    const value = styleSheetService
      .preloadSheetAsync(uri, this.sheetType)
      .then(sheet => {
        return { sheet, uri };
      });

    super.set(hash, value);
  }
}

defineLazyGetter(
  BrowserExtensionContent.prototype,
  "staticScripts",
  function() {
    return new ScriptCache({ hasReturnValue: false }, this);
  }
);

defineLazyGetter(
  BrowserExtensionContent.prototype,
  "dynamicScripts",
  function() {
    return new ScriptCache({ hasReturnValue: true }, this);
  }
);

defineLazyGetter(BrowserExtensionContent.prototype, "userCSS", function() {
  return new CSSCache(Ci.nsIStyleSheetService.USER_SHEET, this);
});

defineLazyGetter(BrowserExtensionContent.prototype, "authorCSS", function() {
  return new CSSCache(Ci.nsIStyleSheetService.AUTHOR_SHEET, this);
});

// These two caches are similar to the above but specialized to cache the cssCode
// using an hash computed from the cssCode string as the key (instead of the generated data
// URI which can be pretty long for bigger injected cssCode).
defineLazyGetter(BrowserExtensionContent.prototype, "userCSSCode", function() {
  return new CSSCodeCache(Ci.nsIStyleSheetService.USER_SHEET, this);
});

defineLazyGetter(
  BrowserExtensionContent.prototype,
  "authorCSSCode",
  function() {
    return new CSSCodeCache(Ci.nsIStyleSheetService.AUTHOR_SHEET, this);
  }
);

// Represents a content script.
class Script {
  /**
   * @param {BrowserExtensionContent} extension
   * @param {WebExtensionContentScript|object} matcher
   *        An object with a "matchesWindow" method and content script execution
   *        details. This is usually a plain WebExtensionContentScript object,
   *        except when the script is run via `tabs.executeScript`. In this
   *        case, the object may have some extra properties:
   *        wantReturnValue, removeCSS, cssOrigin, jsCode
   */
  constructor(extension, matcher) {
    this.extension = extension;
    this.matcher = matcher;

    this.runAt = this.matcher.runAt;
    this.js = this.matcher.jsPaths;
    this.css = this.matcher.cssPaths.slice();
    this.cssCodeHash = null;

    this.removeCSS = this.matcher.removeCSS;
    this.cssOrigin = this.matcher.cssOrigin;

    this.cssCache =
      extension[this.cssOrigin === "user" ? "userCSS" : "authorCSS"];
    this.cssCodeCache =
      extension[this.cssOrigin === "user" ? "userCSSCode" : "authorCSSCode"];
    this.scriptCache =
      extension[matcher.wantReturnValue ? "dynamicScripts" : "staticScripts"];

    if (matcher.wantReturnValue) {
      this.compileScripts();
      this.loadCSS();
    }
  }

  get requiresCleanup() {
    return !this.removeCSS && (!!this.css.length || this.cssCodeHash);
  }

  async addCSSCode(cssCode) {
    if (!cssCode) {
      return;
    }

    // Store the hash of the cssCode.
    const buffer = await crypto.subtle.digest(
      "SHA-1",
      new TextEncoder().encode(cssCode)
    );
    this.cssCodeHash = String.fromCharCode(...new Uint16Array(buffer));

    // Cache and preload the cssCode stylesheet.
    this.cssCodeCache.addCSSCode(this.cssCodeHash, cssCode);
  }

  compileScripts() {
    return this.js.map(url => this.scriptCache.get(url));
  }

  loadCSS() {
    return this.css.map(url => this.cssCache.get(url));
  }

  preload() {
    this.loadCSS();
    this.compileScripts();
  }

  cleanup(window) {
    if (this.requiresCleanup) {
      if (window) {
        let winUtils = getWinUtils(window);

        let type =
          this.cssOrigin === "user"
            ? winUtils.USER_SHEET
            : winUtils.AUTHOR_SHEET;

        for (let url of this.css) {
          this.cssCache.deleteDocument(url, window.document);

          if (!window.closed) {
            runSafeSyncWithoutClone(
              winUtils.removeSheetUsingURIString,
              url,
              type
            );
          }
        }

        const { cssCodeHash } = this;

        if (cssCodeHash && this.cssCodeCache.has(cssCodeHash)) {
          if (!window.closed) {
            this.cssCodeCache.get(cssCodeHash).then(({ uri }) => {
              runSafeSyncWithoutClone(winUtils.removeSheet, uri, type);
            });
          }
          this.cssCodeCache.deleteDocument(cssCodeHash, window.document);
        }
      }

      // Clear any sheets that were kept alive past their timeout as
      // a result of living in this document.
      this.cssCodeCache.clear(CSSCODE_EXPIRY_TIMEOUT_MS);
      this.cssCache.clear(CSS_EXPIRY_TIMEOUT_MS);
    }
  }

  matchesWindow(window) {
    return this.matcher.matchesWindow(window);
  }

  async injectInto(window) {
    if (!isContentScriptProcess) {
      return;
    }

    let context = this.extension.getContext(window);
    try {
      if (this.runAt === "document_end") {
        await promiseDocumentReady(window.document);
      } else if (this.runAt === "document_idle") {
        await Promise.race([
          promiseDocumentIdle(window),
          promiseDocumentLoaded(window.document),
        ]);
      }

      return this.inject(context);
    } catch (e) {
      return Promise.reject(context.normalizeError(e));
    }
  }

  /**
   * Tries to inject this script into the given window and sandbox, if
   * there are pending operations for the window's current load state.
   *
   * @param {BaseContext} context
   *        The content script context into which to inject the scripts.
   * @returns {Promise<any>}
   *        Resolves to the last value in the evaluated script, when
   *        execution is complete.
   */
  async inject(context) {
    DocumentManager.lazyInit();
    if (this.requiresCleanup) {
      context.addScript(this);
    }

    const { cssCodeHash } = this;

    let cssPromise;
    if (this.css.length || cssCodeHash) {
      let window = context.contentWindow;
      let winUtils = getWinUtils(window);

      let type =
        this.cssOrigin === "user" ? winUtils.USER_SHEET : winUtils.AUTHOR_SHEET;

      if (this.removeCSS) {
        for (let url of this.css) {
          this.cssCache.deleteDocument(url, window.document);

          runSafeSyncWithoutClone(
            winUtils.removeSheetUsingURIString,
            url,
            type
          );
        }

        if (cssCodeHash && this.cssCodeCache.has(cssCodeHash)) {
          const { uri } = await this.cssCodeCache.get(cssCodeHash);
          this.cssCodeCache.deleteDocument(cssCodeHash, window.document);

          runSafeSyncWithoutClone(winUtils.removeSheet, uri, type);
        }
      } else {
        cssPromise = Promise.all(this.loadCSS()).then(sheets => {
          let window = context.contentWindow;
          if (!window) {
            return;
          }

          for (let { url, sheet } of sheets) {
            this.cssCache.addDocument(url, window.document);

            runSafeSyncWithoutClone(winUtils.addSheet, sheet, type);
          }
        });

        if (cssCodeHash) {
          cssPromise = cssPromise.then(async () => {
            const { sheet } = await this.cssCodeCache.get(cssCodeHash);
            this.cssCodeCache.addDocument(cssCodeHash, window.document);

            runSafeSyncWithoutClone(winUtils.addSheet, sheet, type);
          });
        }

        // We're loading stylesheets via the stylesheet service, which means
        // that the normal mechanism for blocking layout and onload for pending
        // stylesheets aren't in effect (since there's no document to block). So
        // we need to do something custom here, similar to what we do for
        // scripts. Blocking parsing is overkill, since we really just want to
        // block layout and onload. But we have an API to do the former and not
        // the latter, so we do it that way. This hopefully isn't a performance
        // problem since there are no network loads involved, and since we cache
        // the stylesheets on first load. We should fix this up if it does becomes
        // a problem.
        if (this.css.length) {
          context.contentWindow.document.blockParsing(cssPromise, {
            blockScriptCreated: false,
          });
        }
      }
    }

    let scripts = this.getCompiledScripts(context);
    if (scripts instanceof Promise) {
      scripts = await scripts;
    }

    let result;

    const { extension } = context;

    // The evaluations below may throw, in which case the promise will be
    // automatically rejected.
    try {
      for (let script of scripts) {
        result = script.executeInGlobal(context.cloneScope);
      }

      if (this.matcher.jsCode) {
        result = Cu.evalInSandbox(
          this.matcher.jsCode,
          context.cloneScope,
          "latest",
          "sandbox eval code",
          1
        );
      }
    } finally {
    }

    await cssPromise;
    return result;
  }

  /**
   *  Get the compiled scripts (if they are already precompiled and cached) or a promise which resolves
   *  to the precompiled scripts (once they have been compiled and cached).
   *
   * @param {BaseContext} context
   *        The document to block the parsing on, if the scripts are not yet precompiled and cached.
   *
   * @returns {Array<PreloadedScript> | Promise<Array<PreloadedScript>>}
   *          Returns an array of preloaded scripts if they are already available, or a promise which
   *          resolves to the array of the preloaded scripts once they are precompiled and cached.
   */
  getCompiledScripts(context) {
    let scriptPromises = this.compileScripts();
    let scripts = scriptPromises.map(promise => promise.script);

    // If not all scripts are already available in the cache, block
    // parsing and wait all promises to resolve.
    if (!scripts.every(script => script)) {
      let promise = Promise.all(scriptPromises);

      // If we're supposed to inject at the start of the document load,
      // and we haven't already missed that point, block further parsing
      // until the scripts have been loaded.
      const { document } = context.contentWindow;
      if (
        this.runAt === "document_start" &&
        document.readyState !== "complete"
      ) {
        document.blockParsing(promise, { blockScriptCreated: false });
      }

      return promise;
    }

    return scripts;
  }
}

// Represents a user script.
class UserScript extends Script {
  /**
   * @param {BrowserExtensionContent} extension
   * @param {WebExtensionContentScript|object} matcher
   *        An object with a "matchesWindow" method and content script execution
   *        details.
   */
  constructor(extension, matcher) {
    super(extension, matcher);

    // This is an opaque object that the extension provides, it is associated to
    // the particular userScript and it is passed as a parameter to the custom
    // userScripts APIs defined by the extension.
    this.scriptMetadata = matcher.userScriptOptions.scriptMetadata;
    this.apiScriptURL =
      extension.manifest.user_scripts &&
      extension.manifest.user_scripts.api_script;

    // Add the apiScript to the js scripts to compile.
    if (this.apiScriptURL) {
      this.js = [this.apiScriptURL].concat(this.js);
    }

    // WeakMap<ContentScriptContextChild, Sandbox>
    this.sandboxes = new DefaultWeakMap(context => {
      return this.createSandbox(context);
    });
  }

  async inject(context) {
    const { extension } = context;

    DocumentManager.lazyInit();

    let scripts = this.getCompiledScripts(context);
    if (scripts instanceof Promise) {
      scripts = await scripts;
    }

    let apiScript, sandboxScripts;

    if (this.apiScriptURL) {
      [apiScript, ...sandboxScripts] = scripts;
    } else {
      sandboxScripts = scripts;
    }

    // Load and execute the API script once per context.
    if (apiScript) {
      context.executeAPIScript(apiScript);
    }

    // The evaluations below may throw, in which case the promise will be
    // automatically rejected.
    try {
      let userScriptSandbox = this.sandboxes.get(context);

      context.callOnClose({
        close: () => {
          // Destroy the userScript sandbox when the related ContentScriptContextChild instance
          // is being closed.
          this.sandboxes.delete(context);
          Cu.nukeSandbox(userScriptSandbox);
        },
      });

      // Notify listeners subscribed to the userScripts.onBeforeScript API event,
      // to allow extension API script to provide its custom APIs to the userScript.
      if (apiScript) {
        context.userScriptsEvents.emit(
          "on-before-script",
          this.scriptMetadata,
          userScriptSandbox
        );
      }

      for (let script of sandboxScripts) {
        script.executeInGlobal(userScriptSandbox);
      }
    } finally {
    }
  }

  createSandbox(context) {
    const { contentWindow } = context;
    const contentPrincipal = contentWindow.document.nodePrincipal;
    const ssm = Services.scriptSecurityManager;

    let principal;
    if (contentPrincipal.isSystemPrincipal) {
      principal = ssm.createNullPrincipal(contentPrincipal.originAttributes);
    } else {
      principal = [contentPrincipal];
    }

    const sandbox = Cu.Sandbox(principal, {
      sandboxName: `User Script registered by ${
        this.extension.policy.debugName
      }`,
      sandboxPrototype: contentWindow,
      sameZoneAs: contentWindow,
      wantXrays: true,
      wantGlobalProperties: ["XMLHttpRequest", "fetch"],
      originAttributes: contentPrincipal.originAttributes,
      metadata: {
        "inner-window-id": context.innerWindowID,
        addonId: this.extension.policy.id,
      },
    });

    return sandbox;
  }
}

var contentScripts = new DefaultWeakMap(matcher => {
  const extension = ExtensionProcessScript.extensions.get(matcher.extension);

  if ("userScriptOptions" in matcher) {
    return new UserScript(extension, matcher);
  }

  return new Script(extension, matcher);
});

/**
 * An execution context for semi-privileged extension content scripts.
 *
 * This is the child side of the ContentScriptContextParent class
 * defined in ExtensionParent.jsm.
 */
class ContentScriptContextChild extends BaseContext {
  constructor(extension, contentWindow) {
    super("content_child", extension);

    this.setContentWindow(contentWindow);

    let frameId = WebNavigationFrames.getFrameId(contentWindow);
    this.frameId = frameId;

    this.scripts = [];

    let contentPrincipal = contentWindow.document.nodePrincipal;
    let ssm = Services.scriptSecurityManager;

    // Copy origin attributes from the content window origin attributes to
    // preserve the user context id.
    let attrs = contentPrincipal.originAttributes;
    let extensionPrincipal = ssm.createCodebasePrincipal(
      this.extension.baseURI,
      attrs
    );

    this.isExtensionPage = contentPrincipal.equals(extensionPrincipal);

    if (this.isExtensionPage) {
      // This is an iframe with content script API enabled and its principal
      // should be the contentWindow itself. We create a sandbox with the
      // contentWindow as principal and with X-rays disabled because it
      // enables us to create the APIs object in this sandbox object and then
      // copying it into the iframe's window.  See bug 1214658.
      this.sandbox = Cu.Sandbox(contentWindow, {
        sandboxName: `Web-Accessible Extension Page ${
          extension.policy.debugName
        }`,
        sandboxPrototype: contentWindow,
        sameZoneAs: contentWindow,
        wantXrays: false,
        isWebExtensionContentScript: true,
      });
    } else {
      let principal;
      if (contentPrincipal.isSystemPrincipal) {
        // Make sure we don't hand out the system principal by accident.
        // Also make sure that the null principal has the right origin attributes.
        principal = ssm.createNullPrincipal(attrs);
      } else {
        principal = [contentPrincipal, extensionPrincipal];
      }
      // This metadata is required by the Developer Tools, in order for
      // the content script to be associated with both the extension and
      // the tab holding the content page.
      let metadata = {
        "inner-window-id": this.innerWindowID,
        addonId: extensionPrincipal.addonId,
      };

      this.sandbox = Cu.Sandbox(principal, {
        metadata,
        sandboxName: `Content Script ${extension.policy.debugName}`,
        sandboxPrototype: contentWindow,
        sameZoneAs: contentWindow,
        wantXrays: true,
        isWebExtensionContentScript: true,
        wantExportHelpers: true,
        wantGlobalProperties: ["XMLHttpRequest", "fetch"],
        originAttributes: attrs,
      });

      // Preserve a copy of the original Error and Promise globals from the sandbox object,
      // which are used in the WebExtensions internals (before any content script code had
      // any chance to redefine them).
      this.cloneScopePromise = this.sandbox.Promise;
      this.cloneScopeError = this.sandbox.Error;

      // Preserve a copy of the original window's XMLHttpRequest and fetch
      // in a content object (fetch is manually binded to the window
      // to prevent it from raising a TypeError because content object is not
      // a real window).
      Cu.evalInSandbox(
        `
        this.content = {
          XMLHttpRequest: window.XMLHttpRequest,
          fetch: window.fetch.bind(window),
        };

        window.JSON = JSON;
        window.XMLHttpRequest = XMLHttpRequest;
        window.fetch = fetch;
      `,
        this.sandbox
      );
    }

    Object.defineProperty(this, "principal", {
      value: Cu.getObjectPrincipal(this.sandbox),
      enumerable: true,
      configurable: true,
    });

    this.url = contentWindow.location.href;

    defineLazyGetter(this, "chromeObj", () => {
      let chromeObj = Cu.createObjectIn(this.sandbox);

      this.childManager.inject(chromeObj);
      return chromeObj;
    });

    Schemas.exportLazyGetter(this.sandbox, "browser", () => this.chromeObj);
    Schemas.exportLazyGetter(this.sandbox, "chrome", () => this.chromeObj);

    // Keep track if the userScript API script has been already executed in this context
    // (e.g. because there are more then one UserScripts that match the related webpage
    // and so the UserScript apiScript has already been executed).
    this.hasUserScriptAPIs = false;

    // A lazy created EventEmitter related to userScripts-specific events.
    defineLazyGetter(this, "userScriptsEvents", () => {
      return new ExtensionCommon.EventEmitter();
    });
  }

  injectAPI() {
    if (!this.isExtensionPage) {
      throw new Error("Cannot inject extension API into non-extension window");
    }

    // This is an iframe with content script API enabled (See Bug 1214658)
    Schemas.exportLazyGetter(
      this.contentWindow,
      "browser",
      () => this.chromeObj
    );
    Schemas.exportLazyGetter(
      this.contentWindow,
      "chrome",
      () => this.chromeObj
    );
  }

  get cloneScope() {
    return this.sandbox;
  }

  async executeAPIScript(apiScript) {
    // Execute the UserScript apiScript only once per context (e.g. more then one UserScripts
    // match the same webpage and the apiScript has already been executed).
    if (apiScript && !this.hasUserScriptAPIs) {
      this.hasUserScriptAPIs = true;
      apiScript.executeInGlobal(this.cloneScope);
    }
  }

  addScript(script) {
    if (script.requiresCleanup) {
      this.scripts.push(script);
    }
  }

  close() {
    super.unload();

    // Cleanup the scripts even if the contentWindow have been destroyed.
    for (let script of this.scripts) {
      script.cleanup(this.contentWindow);
    }

    if (this.contentWindow) {
      // Overwrite the content script APIs with an empty object if the APIs objects are still
      // defined in the content window (See Bug 1214658).
      if (this.isExtensionPage) {
        Cu.createObjectIn(this.contentWindow, { defineAs: "browser" });
        Cu.createObjectIn(this.contentWindow, { defineAs: "chrome" });
      }
    }
    Cu.nukeSandbox(this.sandbox);

    this.sandbox = null;
  }
}

defineLazyGetter(ContentScriptContextChild.prototype, "messenger", function() {
  // The |sender| parameter is passed directly to the extension.
  let sender = { id: this.extension.id, frameId: this.frameId, url: this.url };
  let filter = { extensionId: this.extension.id };
  let optionalFilter = { frameId: this.frameId };

  return new Messenger(
    this,
    [this.messageManager],
    sender,
    filter,
    optionalFilter
  );
});

defineLazyGetter(
  ContentScriptContextChild.prototype,
  "childManager",
  function() {
    apiManager.lazyInit();

    let localApis = {};
    let can = new CanOfAPIs(this, apiManager, localApis);

    let childManager = new ChildAPIManager(this, this.messageManager, can, {
      envType: "content_parent",
      url: this.url,
    });

    this.callOnClose(childManager);

    return childManager;
  }
);

// Responsible for creating ExtensionContexts and injecting content
// scripts into them when new documents are created.
DocumentManager = {
  // Map[windowId -> Map[ExtensionChild -> ContentScriptContextChild]]
  contexts: new Map(),

  initialized: false,

  lazyInit() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    Services.obs.addObserver(this, "inner-window-destroyed");
    Services.obs.addObserver(this, "memory-pressure");
  },

  uninit() {
    Services.obs.removeObserver(this, "inner-window-destroyed");
    Services.obs.removeObserver(this, "memory-pressure");
  },

  observers: {
    "inner-window-destroyed"(subject, topic, data) {
      let windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;

      MessageChannel.abortResponses({ innerWindowID: windowId });

      // Close any existent content-script context for the destroyed window.
      if (this.contexts.has(windowId)) {
        let extensions = this.contexts.get(windowId);
        for (let context of extensions.values()) {
          context.close();
        }

        this.contexts.delete(windowId);
      }
    },
    "memory-pressure"(subject, topic, data) {
      let timeout = data === "heap-minimize" ? 0 : undefined;

      for (let cache of ChromeUtils.nondeterministicGetWeakSetKeys(
        scriptCaches
      )) {
        cache.clear(timeout);
      }
    },
  },

  observe(subject, topic, data) {
    this.observers[topic].call(this, subject, topic, data);
  },

  shutdownExtension(extension) {
    for (let extensions of this.contexts.values()) {
      let context = extensions.get(extension);
      if (context) {
        context.close();
        extensions.delete(extension);
      }
    }
  },

  getContexts(window) {
    let winId = getInnerWindowID(window);

    let extensions = this.contexts.get(winId);
    if (!extensions) {
      extensions = new Map();
      this.contexts.set(winId, extensions);
    }

    return extensions;
  },

  // For test use only.
  getContext(extensionId, window) {
    for (let [extension, context] of this.getContexts(window)) {
      if (extension.id === extensionId) {
        return context;
      }
    }
  },

  getContentScriptGlobals(window) {
    let extensions = this.contexts.get(getInnerWindowID(window));

    if (extensions) {
      return Array.from(extensions.values(), ctx => ctx.sandbox);
    }

    return [];
  },

  initExtensionContext(extension, window) {
    extension.getContext(window).injectAPI();
  },
};

var ExtensionContent = {
  BrowserExtensionContent,

  contentScripts,

  shutdownExtension(extension) {
    DocumentManager.shutdownExtension(extension);
  },

  // This helper is exported to be integrated in the devtools RDP actors,
  // that can use it to retrieve the existent WebExtensions ContentScripts
  // of a target window and be able to show the ContentScripts source in the
  // DevTools Debugger panel.
  getContentScriptGlobals(window) {
    return DocumentManager.getContentScriptGlobals(window);
  },

  initExtensionContext(extension, window) {
    DocumentManager.initExtensionContext(extension, window);
  },

  getContext(extension, window) {
    let extensions = DocumentManager.getContexts(window);

    let context = extensions.get(extension);
    if (!context) {
      context = new ContentScriptContextChild(extension, window);
      extensions.set(extension, context);
    }
    return context;
  },

  handleExtensionCapture(global, width, height, options) {
    let win = global.content;

    const XHTML_NS = "http://www.w3.org/1999/xhtml";
    let canvas = win.document.createElementNS(XHTML_NS, "canvas");
    canvas.width = width;
    canvas.height = height;
    canvas.mozOpaque = true;

    let ctx = canvas.getContext("2d");

    // We need to scale the image to the visible size of the browser,
    // in order for the result to appear as the user sees it when
    // settings like full zoom come into play.
    ctx.scale(canvas.width / win.innerWidth, canvas.height / win.innerHeight);

    ctx.drawWindow(
      win,
      win.scrollX,
      win.scrollY,
      win.innerWidth,
      win.innerHeight,
      "#fff"
    );

    return canvas.toDataURL(`image/${options.format}`, options.quality / 100);
  },

  handleDetectLanguage(global, target) {
    let doc = target.content.document;

    return promiseDocumentReady(doc).then(() => {
      let elem = doc.documentElement;

      let language =
        elem.getAttribute("xml:lang") ||
        elem.getAttribute("lang") ||
        doc.contentLanguage ||
        null;

      // We only want the last element of the TLD here.
      // Only country codes have any effect on the results, but other
      // values cause no harm.
      let tld = doc.location.hostname.match(/[a-z]*$/)[0];

      // The CLD2 library used by the language detector is capable of
      // analyzing raw HTML. Unfortunately, that takes much more memory,
      // and since it's hosted by emscripten, and therefore can't shrink
      // its heap after it's grown, it has a performance cost.
      // So we send plain text instead.
      let encoder = Cu.createDocumentEncoder("text/plain");
      encoder.init(
        doc,
        "text/plain",
        Ci.nsIDocumentEncoder.SkipInvisibleContent
      );
      let text = encoder.encodeToStringWithMaxLength(60 * 1024);

      let encoding = doc.characterSet;

      return LanguageDetector.detectLanguage({
        language,
        tld,
        text,
        encoding,
      }).then(result => (result.language === "un" ? "und" : result.language));
    });
  },

  // Used to executeScript, insertCSS and removeCSS.
  async handleExtensionExecute(global, target, options, script) {
    let executeInWin = window => {
      if (script.matchesWindow(window)) {
        return script.injectInto(window);
      }
      return null;
    };

    let promises;
    try {
      promises = Array.from(
        this.enumerateWindows(global.docShell),
        executeInWin
      ).filter(promise => promise);
    } catch (e) {
      Cu.reportError(e);
      return Promise.reject({ message: "An unexpected error occurred" });
    }

    if (!promises.length) {
      if (options.frameID) {
        return Promise.reject({
          message: `Frame not found, or missing host permission`,
        });
      }

      let frames = options.allFrames ? ", and any iframes" : "";
      return Promise.reject({
        message: `Missing host permission for the tab${frames}`,
      });
    }
    if (!options.allFrames && promises.length > 1) {
      return Promise.reject({
        message: `Internal error: Script matched multiple windows`,
      });
    }

    let result = await Promise.all(promises);

    try {
      // Make sure we can structured-clone the result value before
      // we try to send it back over the message manager.
      Cu.cloneInto(result, target);
    } catch (e) {
      const { jsPaths } = options;
      const fileName = jsPaths.length
        ? jsPaths[jsPaths.length - 1]
        : "<anonymous code>";
      const message = `Script '${fileName}' result is non-structured-clonable data`;
      return Promise.reject({ message, fileName });
    }

    return result;
  },

  handleWebNavigationGetFrame(global, { frameId }) {
    return WebNavigationFrames.getFrame(global.docShell, frameId);
  },

  handleWebNavigationGetAllFrames(global) {
    return WebNavigationFrames.getAllFrames(global.docShell);
  },

  async receiveMessage(global, name, target, data, recipient) {
    switch (name) {
      case "Extension:Capture":
        return this.handleExtensionCapture(
          global,
          data.width,
          data.height,
          data.options
        );
      case "Extension:DetectLanguage":
        return this.handleDetectLanguage(global, target);
      case "Extension:Execute":
        let policy = WebExtensionPolicy.getByID(recipient.extensionId);

        let matcher = new WebExtensionContentScript(policy, data.options);

        Object.assign(matcher, {
          wantReturnValue: data.options.wantReturnValue,
          removeCSS: data.options.removeCSS,
          cssOrigin: data.options.cssOrigin,
          jsCode: data.options.jsCode,
        });

        let script = contentScripts.get(matcher);

        // Add the cssCode to the script, so that it can be converted into a cached URL.
        await script.addCSSCode(data.options.cssCode);
        delete data.options.cssCode;

        return this.handleExtensionExecute(
          global,
          target,
          data.options,
          script
        );
      case "WebNavigation:GetFrame":
        return this.handleWebNavigationGetFrame(global, data.options);
      case "WebNavigation:GetAllFrames":
        return this.handleWebNavigationGetAllFrames(global);
    }
    return null;
  },

  // Helpers

  *enumerateWindows(docShell) {
    let docShells = docShell.getAllDocShellsInSubtree(
      docShell.typeContent,
      docShell.ENUMERATE_FORWARDS
    );

    for (let docShell of docShells) {
      try {
        yield docShell.domWindow;
      } catch (e) {
        // This can fail if the docShell is being destroyed, so just
        // ignore the error.
      }
    }
  },
};
