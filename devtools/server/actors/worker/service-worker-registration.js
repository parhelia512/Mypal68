/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");
const ChromeUtils = require("ChromeUtils");
const Services = require("Services");
const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");
const protocol = require("devtools/shared/protocol");
const {
  serviceWorkerRegistrationSpec,
} = require("devtools/shared/specs/worker/service-worker-registration");
const {
  PushSubscriptionActor,
} = require("devtools/server/actors/worker/push-subscription");
const {
  ServiceWorkerActor,
} = require("devtools/server/actors/worker/service-worker");

XPCOMUtils.defineLazyServiceGetter(
  this,
  "swm",
  "@mozilla.org/serviceworkers/manager;1",
  "nsIServiceWorkerManager"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PushService",
  "@mozilla.org/push/Service;1",
  "nsIPushService"
);

// Lazily load the service-worker-process.js process script only once.
let _serviceWorkerProcessScriptLoaded = false;

const ServiceWorkerRegistrationActor = protocol.ActorClassWithSpec(
  serviceWorkerRegistrationSpec,
  {
    /**
     * Create the ServiceWorkerRegistrationActor
     * @param DevToolsServerConnection conn
     *   The server connection.
     * @param ServiceWorkerRegistrationInfo registration
     *   The registration's information.
     */
    initialize(conn, registration) {
      protocol.Actor.prototype.initialize.call(this, conn);
      this._conn = conn;
      this._registration = registration;
      this._pushSubscriptionActor = null;

      // A flag to know if preventShutdown has been called and we should
      // try to allow the shutdown of the SW when the actor is destroyed
      this._preventedShutdown = false;

      this._registration.addListener(this);

      this._createServiceWorkerActors();

      Services.obs.addObserver(this, PushService.subscriptionModifiedTopic);
    },

    onChange() {
      this._destroyServiceWorkerActors();
      this._createServiceWorkerActors();
      this.emit("registration-changed");
    },

    form() {
      const registration = this._registration;
      const installingWorker = this._installingWorker.form();
      const waitingWorker = this._waitingWorker.form();
      const activeWorker = this._activeWorker.form();

      const newestWorker = activeWorker || waitingWorker || installingWorker;

      const isNewE10sImplementation = swm.isParentInterceptEnabled();
      const isMultiE10sWithOldImplementation =
        Services.appinfo.browserTabsRemoteAutostart && !isNewE10sImplementation;
      return {
        actor: this.actorID,
        scope: registration.scope,
        url: registration.scriptSpec,
        installingWorker,
        waitingWorker,
        activeWorker,
        fetch: newestWorker && newestWorker.fetch,
        // - In old multi e10s: only active registrations are available.
        // - In non-e10s or new implementaion: check if we have an active worker
        active: isMultiE10sWithOldImplementation ? true : !!activeWorker,
        lastUpdateTime: registration.lastUpdateTime,
      };
    },

    destroy() {
      protocol.Actor.prototype.destroy.call(this);

      // Ensure resuming the service worker in case the connection drops
      if (
        swm.isParentInterceptEnabled() &&
        this._registration.activeWorker &&
        this._preventedShutdown
      ) {
        this.allowShutdown();
      }

      Services.obs.removeObserver(this, PushService.subscriptionModifiedTopic);
      this._registration.removeListener(this);
      this._registration = null;
      if (this._pushSubscriptionActor) {
        this._pushSubscriptionActor.destroy();
      }
      this._pushSubscriptionActor = null;

      this._destroyServiceWorkerActors();

      this._installingWorker = null;
      this._waitingWorker = null;
      this._activeWorker = null;
    },

    /**
     * Standard observer interface to listen to push messages and changes.
     */
    observe(subject, topic, data) {
      const scope = this._registration.scope;
      if (data !== scope) {
        // This event doesn't concern us, pretend nothing happened.
        return;
      }
      switch (topic) {
        case PushService.subscriptionModifiedTopic:
          if (this._pushSubscriptionActor) {
            this._pushSubscriptionActor.destroy();
            this._pushSubscriptionActor = null;
          }
          this.emit("push-subscription-modified");
          break;
      }
    },

    start() {
      if (!_serviceWorkerProcessScriptLoaded) {
        Services.ppmm.loadProcessScript(
          "resource://devtools/server/actors/worker/service-worker-process.js",
          true
        );
        _serviceWorkerProcessScriptLoaded = true;
      }

      // XXX: Send the permissions down to the content process before starting
      // the service worker within the content process. As we don't know what
      // content process we're starting the service worker in (as we're using a
      // broadcast channel to talk to it), we just broadcast the permissions to
      // everyone as well.
      //
      // This call should be replaced with a proper implementation when
      // ServiceWorker debugging is improved to support multiple content processes
      // correctly.
      Services.perms.broadcastPermissionsForPrincipalToAllContentProcesses(
        this._registration.principal
      );

      Services.ppmm.broadcastAsyncMessage("serviceWorkerRegistration:start", {
        scope: this._registration.scope,
      });
      return { type: "started" };
    },

    unregister() {
      const { principal, scope } = this._registration;
      const unregisterCallback = {
        unregisterSucceeded: function() {},
        unregisterFailed: function() {
          console.error("Failed to unregister the service worker for " + scope);
        },
        QueryInterface: ChromeUtils.generateQI([
          Ci.nsIServiceWorkerUnregisterCallback,
        ]),
      };
      swm.propagateUnregister(principal, unregisterCallback, scope);

      return { type: "unregistered" };
    },

    getPushSubscription() {
      const registration = this._registration;
      let pushSubscriptionActor = this._pushSubscriptionActor;
      if (pushSubscriptionActor) {
        return Promise.resolve(pushSubscriptionActor);
      }
      return new Promise((resolve, reject) => {
        PushService.getSubscription(
          registration.scope,
          registration.principal,
          (result, subscription) => {
            if (!subscription) {
              resolve(null);
              return;
            }
            pushSubscriptionActor = new PushSubscriptionActor(
              this._conn,
              subscription
            );
            this._pushSubscriptionActor = pushSubscriptionActor;
            resolve(pushSubscriptionActor);
          }
        );
      });
    },

    _destroyServiceWorkerActors() {
      this._installingWorker.destroy();
      this._waitingWorker.destroy();
      this._activeWorker.destroy();
    },

    _createServiceWorkerActors() {
      const {
        installingWorker,
        waitingWorker,
        activeWorker,
      } = this._registration;

      this._installingWorker = new ServiceWorkerActor(
        this._conn,
        installingWorker
      );
      this._waitingWorker = new ServiceWorkerActor(this._conn, waitingWorker);
      this._activeWorker = new ServiceWorkerActor(this._conn, activeWorker);

      // Add the ServiceWorker actors as children of this ServiceWorkerRegistration actor,
      // assigning them valid actorIDs.
      this.manage(this._installingWorker);
      this.manage(this._waitingWorker);
      this.manage(this._activeWorker);
    },
  }
);

exports.ServiceWorkerRegistrationActor = ServiceWorkerRegistrationActor;
