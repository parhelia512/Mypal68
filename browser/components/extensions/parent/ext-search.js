/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "searchLoadInBackground",
  "browser.search.context.loadInBackground"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "btoa"]);

var { ExtensionError } = ExtensionUtils;

async function getDataURI(resourceURI) {
  let response = await fetch(resourceURI);
  let buffer = await response.arrayBuffer();
  let contentType = response.headers.get("content-type");
  let bytes = new Uint8Array(buffer);
  let str = String.fromCharCode.apply(null, bytes);
  return `data:${contentType};base64,${btoa(str)}`;
}

this.search = class extends ExtensionAPI {
  getAPI(context) {
    return {
      search: {
        async get() {
          await searchInitialized;
          let visibleEngines = await Services.search.getVisibleEngines();
          let defaultEngine = await Services.search.getDefault();
          return Promise.all(
            visibleEngines.map(async engine => {
              let favIconUrl;
              if (engine.iconURI) {
                if (
                  engine.iconURI.schemeIs("resource") ||
                  engine.iconURI.schemeIs("chrome")
                ) {
                  // Convert internal URLs to data URLs
                  favIconUrl = await getDataURI(engine.iconURI.spec);
                } else {
                  favIconUrl = engine.iconURI.spec;
                }
              }

              return {
                name: engine.name,
                isDefault: engine.name === defaultEngine.name,
                alias: engine.alias || undefined,
                favIconUrl,
              };
            })
          );
        },

        async search(searchProperties) {
          await searchInitialized;
          let engine;
          if (searchProperties.engine) {
            engine = Services.search.getEngineByName(searchProperties.engine);
            if (!engine) {
              throw new ExtensionError(
                `${searchProperties.engine} was not found`
              );
            }
          } else {
            engine = await Services.search.getDefault();
          }
          let submission = engine.getSubmission(
            searchProperties.query,
            null,
            "webextension"
          );
          let options = {
            postData: submission.postData,
            triggeringPrincipal: context.principal,
          };
          let tabbrowser;
          if (searchProperties.tabId === null) {
            let { gBrowser } = windowTracker.topWindow;
            let nativeTab = gBrowser.addTab(submission.uri.spec, options);
            if (!searchLoadInBackground) {
              gBrowser.selectedTab = nativeTab;
            }
            tabbrowser = gBrowser;
          } else {
            let tab = tabTracker.getTab(searchProperties.tabId);
            tab.linkedBrowser.loadURI(submission.uri.spec, options);
            tabbrowser = tab.linkedBrowser.getTabBrowser();
          }
        },
      },
    };
  }
};
