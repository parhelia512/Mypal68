/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Please indent all prefs defined within #ifdef/#ifndef conditions. This
// improves readability, particular for conditional blocks that exceed a single
// screen.

#filter substitution

#include mobile.js

pref("privacy.trackingprotection.pbmode.enabled", false);

pref("dom.ipc.keepProcessesAlive.web", 1);
pref("dom.ipc.processCount", 1);
pref("dom.ipc.processPrelaunch.enabled", false);

// Tell Telemetry that we're in GeckoView mode.
pref("toolkit.telemetry.isGeckoViewMode", true);
// Disable the Telemetry Event Ping
pref("toolkit.telemetry.eventping.enabled", false);

pref("geckoview.console.enabled", false);

#ifdef RELEASE_OR_BETA
  pref("geckoview.logging", "Warn");
#else
  pref("geckoview.logging", "Debug");
#endif

// Disable ServiceWorkers on ESR68 for non-Fennec
pref("dom.serviceWorkers.enabled", false);

// Disable Web Push until we get it working
pref("dom.push.enabled", false);

// enable external storage API
pref("dom.storageManager.enabled", true);

// Inherit locale from the OS, used for multi-locale builds
pref("intl.locale.requested", "");

// Enable Safe Browsing blocklist updates
pref("browser.safebrowsing.features.phishing.update", true);
pref("browser.safebrowsing.features.malware.update", true);

// Enable Tracking Protection blocklist updates
pref("browser.safebrowsing.features.trackingAnnotation.update", true);
pref("browser.safebrowsing.features.trackingProtection.update", true);

// Enable cryptomining protection blocklist updates
pref("browser.safebrowsing.features.cryptomining.update", true);
// Enable fingerprinting protection blocklist updates
pref("browser.safebrowsing.features.fingerprinting.update", true);

// Treat mouse as touch only on TV-ish devices
pref("ui.android.mouse_as_touch", 2);

// Fenix is currently not whitelisted for Web Authentication
pref("security.webauth.webauthn_enable_android_fido2", false);
