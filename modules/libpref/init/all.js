/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The prefs in this file are shipped with the GRE and should apply to all
 * embedding situations. Application-specific preferences belong somewhere else,
 * such as browser/app/profile/firefox.js or mobile/android/app/mobile.js.
 *
 * NOTE: Not all prefs should be defined in this (or any other) data file.
 * Static prefs, especially VarCache prefs, are defined in StaticPrefList.yaml.
 * Those prefs should *not* appear in this file.
 *
 * For the syntax used by this file, consult the comments at the top of
 * modules/libpref/parser/src/lib.rs.
 *
 * Please indent all prefs defined within #ifdef/#ifndef conditions. This
 * improves readability, particular for conditional blocks that exceed a single
 * screen.
 */

pref("security.tls.version.min", 1);
pref("security.tls.version.max", 4);
pref("security.tls.version.fallback-limit", 4);
pref("security.tls.insecure_fallback_hosts", "");
// Turn off post-handshake authentication for TLS 1.3 by default,
// until the incompatibility with HTTP/2 is resolved:
// https://tools.ietf.org/html/draft-davidben-http2-tls13-00
pref("security.tls.enable_post_handshake_auth", false);
#ifdef RELEASE_OR_BETA
  pref("security.tls.hello_downgrade_check", false);
#else
  pref("security.tls.hello_downgrade_check", true);
#endif
pref("security.tls.enable_delegated_credentials", false);

pref("security.ssl.treat_unsafe_negotiation_as_broken", false);
pref("security.ssl.require_safe_negotiation",  false);
pref("security.ssl.enable_ocsp_stapling", true);
pref("security.ssl.enable_false_start", true);
pref("security.ssl.enable_alpn", true);

pref("security.ssl3.ecdhe_rsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_gcm_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_chacha20_poly1305_sha256", true);
pref("security.ssl3.ecdhe_rsa_chacha20_poly1305_sha256", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_gcm_sha384", true);
pref("security.ssl3.ecdhe_rsa_aes_256_gcm_sha384", true);
pref("security.ssl3.ecdhe_rsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_128_sha", true);
pref("security.ssl3.ecdhe_rsa_aes_256_sha", true);
pref("security.ssl3.ecdhe_ecdsa_aes_256_sha", true);
pref("security.ssl3.dhe_rsa_aes_128_sha", false);
pref("security.ssl3.dhe_rsa_aes_256_sha", false);
pref("security.ssl3.rsa_aes_128_sha", true);
pref("security.ssl3.rsa_aes_256_sha", true);
pref("security.ssl3.rsa_aes_128_gcm_sha256", true);
pref("security.ssl3.rsa_aes_256_gcm_sha384", true);
pref("security.ssl3.rsa_des_ede3_sha", true);

pref("security.content.signature.root_hash",
     "97:E8:BA:9C:F1:2F:B3:DE:53:CC:42:A4:E6:57:7E:D6:4D:F4:93:C2:47:B4:14:FE:A0:36:81:8D:38:23:56:0E");

pref("security.default_personal_cert",   "Ask Every Time");
pref("security.remember_cert_checkbox_default_setting", true);
pref("security.ask_for_password",        0);
pref("security.password_lifetime",       30);

// On Windows 8.1, if the following preference is 2, we will attempt to detect
// if the Family Safety TLS interception feature has been enabled. If so, we
// will behave as if the enterprise roots feature has been enabled (i.e. import
// and trust third party root certificates from the OS).
// With any other value of the pref or on any other platform, this does nothing.
// This preference takes precedence over "security.enterprise_roots.enabled".
pref("security.family_safety.mode", 2);

pref("security.enterprise_roots.enabled", true);

// The supported values of this pref are:
// 0: do not fetch OCSP
// 1: fetch OCSP for DV and EV certificates
// 2: fetch OCSP only for EV certificates
pref("security.OCSP.enabled", 0);
pref("security.OCSP.require", false);
#ifdef RELEASE_OR_BETA
  pref("security.OCSP.timeoutMilliseconds.soft", 2000);
#else
  pref("security.OCSP.timeoutMilliseconds.soft", 1000);
#endif
pref("security.OCSP.timeoutMilliseconds.hard", 10000);

pref("security.pki.cert_short_lifetime_in_days", 10);
// NB: Changes to this pref affect CERT_CHAIN_SHA1_POLICY_STATUS telemetry.
// See the comment in CertVerifier.cpp.
// 3 = only allow SHA-1 for certificates issued by an imported root.
pref("security.pki.sha1_enforcement_level", 3);

// This preference controls what signature algorithms are accepted for signed
// apps (i.e. add-ons). The number is interpreted as a bit mask with the
// following semantic:
// The lowest order bit determines which PKCS#7 algorithms are accepted.
// xxx_0_: SHA-1 and/or SHA-256 PKCS#7 allowed
// xxx_1_: SHA-256 PKCS#7 allowed
// The next two bits determine whether COSE is required and PKCS#7 is allowed
// x_00_x: COSE disabled, ignore files, PKCS#7 must verify
// x_01_x: COSE is verified if present, PKCS#7 must verify
// x_10_x: COSE is required, PKCS#7 must verify if present
// x_11_x: COSE is required, PKCS#7 disabled (fail when present)
pref("security.signed_app_signatures.policy", 2);

// security.pki.name_matching_mode controls how the platform matches hostnames
// to name information in TLS certificates. The possible values are:
// 0: always fall back to the subject common name if necessary (as in, if the
//    subject alternative name extension is either not present or does not
//    contain any DNS names or IP addresses)
// 1: fall back to the subject common name for certificates valid before 23
//    August 2016 if necessary
// 2: fall back to the subject common name for certificates valid before 23
//    August 2015 if necessary
// 3: only use name information from the subject alternative name extension
pref("security.pki.name_matching_mode", 3);

// security.pki.netscape_step_up_policy controls how the platform handles the
// id-Netscape-stepUp OID in extended key usage extensions of CA certificates.
// 0: id-Netscape-stepUp is always considered equivalent to id-kp-serverAuth
// 1: it is considered equivalent when the notBefore is before 23 August 2016
// 2: similarly, but for 23 August 2015
// 3: it is never considered equivalent
#ifdef RELEASE_OR_BETA
  pref("security.pki.netscape_step_up_policy", 1);
#else
  pref("security.pki.netscape_step_up_policy", 2);
#endif

// Configures Certificate Transparency support mode:
// 0: Fully disabled.
// 1: Only collect telemetry. CT qualification checks are not performed.
pref("security.pki.certificate_transparency.mode", 0);

// Only one of ["enable_softtoken", "enable_usbtoken",
// "webauthn_enable_android_fido2"] should be true at a time, as the
// softtoken will override the other two. Note android's pref is set in
// mobile.js / geckoview-prefs.js
pref("security.webauth.webauthn_enable_softtoken", false);

#ifdef MOZ_WIDGET_ANDROID
  // the Rust usbtoken support does not function on Android
  pref("security.webauth.webauthn_enable_usbtoken", false);
#else
  pref("security.webauth.webauthn_enable_usbtoken", true);
#endif

// Impose a maximum age on HPKP headers, to avoid sites getting permanently
// blacking themselves out by setting a bad pin.  (60 days by default)
// https://tools.ietf.org/html/rfc7469#section-4.1
pref("security.cert_pinning.max_max_age_seconds", 5184000);

// security.pki.distrust_ca_policy controls what root program distrust policies
// are enforced at this time:
// 0: No distrust policies enforced
// 1: Symantec roots distrusted for certificates issued after cutoff
// 2: Symantec roots distrusted regardless of date
// See https://wiki.mozilla.org/CA/Upcoming_Distrust_Actions for more details.
pref("security.pki.distrust_ca_policy", 2);

// Issuer we use to detect MitM proxies. Set to the issuer of the cert of the
// Firefox update service. The string format is whatever NSS uses to print a DN.
// This value is set and cleared automatically.
pref("security.pki.mitm_canary_issuer", "");
// Pref to disable the MitM proxy checks.
pref("security.pki.mitm_canary_issuer.enabled", true);

// It is set to true when a non-built-in root certificate is detected on a
// Firefox update service's connection.
// This value is set automatically.
// The difference between security.pki.mitm_canary_issuer and this pref is that
// here the root is trusted but not a built-in, whereas for
// security.pki.mitm_canary_issuer.enabled, the root is not trusted.
pref("security.pki.mitm_detected", false);

// Intermediate CA Preloading settings
#if defined(RELEASE_OR_BETA) || defined(MOZ_WIDGET_ANDROID)
  pref("security.remote_settings.intermediates.enabled", false);
#else
  pref("security.remote_settings.intermediates.enabled", true);
#endif
pref("security.remote_settings.intermediates.bucket", "security-state");
pref("security.remote_settings.intermediates.collection", "intermediates");
pref("security.remote_settings.intermediates.checked", 0);
pref("security.remote_settings.intermediates.downloads_per_poll", 100);
pref("security.remote_settings.intermediates.signer", "onecrl.content-signature.mozilla.org");

pref("general.useragent.compatMode.firefox", true);

// This pref exists only for testing purposes. In order to disable all
// overrides by default, don't initialize UserAgentOverrides.jsm.
pref("general.useragent.site_specific_overrides", true);

pref("general.config.obscure_value", 13); // for MCD .cfg files

pref("general.warnOnAboutConfig", true);

// maximum number of dated backups to keep at any time
pref("browser.bookmarks.max_backups",       5);

// Size (in KB) explicitly set by the user. Used when smart_size.enabled == false
pref("browser.cache.disk.capacity",         256000);

pref("browser.cache.disk_cache_ssl",        true);
// 0 = once-per-session, 1 = each-time, 2 = never, 3 = when-appropriate/automatically
pref("browser.cache.check_doc_frequency",   3);
// The half life used to re-compute cache entries frecency in hours.
pref("browser.cache.frecency_half_life_hours", 6);

// offline cache capacity in kilobytes
pref("browser.cache.offline.capacity",         512000);

// Don't show "Open with" option on download dialog if true.
pref("browser.download.forbid_open_with", false);

// Insecure registerProtocolHandler is disabled by default
pref("dom.registerProtocolHandler.insecure.enabled", false);

// Whether or not indexedDB experimental features are enabled.
pref("dom.indexedDB.experimental", false);
// Enable indexedDB logging.
pref("dom.indexedDB.logging.enabled", true);
// Detailed output in log messages.
pref("dom.indexedDB.logging.details", true);
// Enable profiler marks for indexedDB events.
pref("dom.indexedDB.logging.profiler-marks", false);

// Whether or not File Handle is enabled.
pref("dom.fileHandle.enabled", true);

// The number of workers per domain allowed to run concurrently.
// We're going for effectively infinite, while preventing abuse.
pref("dom.workers.maxPerDomain", 512);

// The amount of time (milliseconds) service workers keep running after each event.
pref("dom.serviceWorkers.idle_timeout", 30000);

// The amount of time (milliseconds) service workers can be kept running using waitUntil promises.
pref("dom.serviceWorkers.idle_extended_timeout", 300000);

// The amount of time (milliseconds) an update request is delayed when triggered
// by a service worker that doesn't control any clients.
pref("dom.serviceWorkers.update_delay", 1000);

// Enable test for 24 hours update, service workers will always treat last update check time is over 24 hours
pref("dom.serviceWorkers.testUpdateOverOneDay", false);

// Blacklist of domains of web apps which are not aware of strict keypress
// dispatching behavior.  This is comma separated list.  If you need to match
// all sub-domains, you can specify it as "*.example.com".  Additionally, you
// can limit the path.  E.g., "example.com/foo" means "example.com/foo*".  So,
// if you need to limit under a directory, the path should end with "/" like
// "example.com/foo/".  Note that this cannot limit port number for now.
pref("dom.keyboardevent.keypress.hack.dispatch_non_printable_keys", "www.icloud.com");
// Pref for end-users and policy to add additional values.
pref("dom.keyboardevent.keypress.hack.dispatch_non_printable_keys.addl", "");

// Blacklist of domains of web apps which handle keyCode and charCode of
// keypress events with a path only for Firefox (i.e., broken if we set
// non-zero keyCode or charCode value to the other).  The format is exactly
// same as "dom.keyboardevent.keypress.hack.dispatch_non_printable_keys". So,
// check its explanation for the detail.
pref("dom.keyboardevent.keypress.hack.use_legacy_keycode_and_charcode", "*.collabserv.com,*.gov.online.office365.us,*.officeapps-df.live.com,*.officeapps.live.com,*.online.office.de,*.partner.officewebapps.cn,*.scniris.com");
// Pref for end-users and policy to add additional values.
pref("dom.keyboardevent.keypress.hack.use_legacy_keycode_and_charcode.addl", "");

// Blacklist of domains of web apps which listen for non-primary click events
// on window global or document. The format is exactly same as
// "dom.keyboardevent.keypress.hack.dispatch_non_printable_keys". So, check its
// explanation for the detail.
pref("dom.mouseevent.click.hack.use_legacy_non-primary_dispatch", "");

// Fastback caching - if this pref is negative, then we calculate the number
// of content viewers to cache based on the amount of available memory.
pref("browser.sessionhistory.max_total_viewers", -1);
// 0 = false, 1 = true, 2 = autodetect.
pref("ui.android.mouse_as_touch", 1);

// Duration of timeout of incremental search in menus (ms).  0 means infinite.
pref("ui.menu.incremental_search.timeout", 1000);
// If true, all popups won't hide automatically on blur
pref("ui.popup.disable_autohide", false);

#ifdef XP_MACOSX
  pref("ui.touchbar.layout", "Back,Forward,Reload,OpenLocation,NewTab,Share");
#endif

pref("browser.display.force_inline_alttext", false); // true = force ALT text for missing images to be layed out inline
// 0 = no external leading,
// 1 = use external leading only when font provides,
// 2 = add extra leading both internal leading and external leading are zero
pref("browser.display.normal_lineheight_calc_control", 2);
// enable showing image placeholders while image is loading or when image is broken
pref("browser.display.show_image_placeholders", true);
// if browser.display.show_image_placeholders is true then this controls whether the loading image placeholder and border is shown or not
pref("browser.display.show_loading_image_placeholder", false);
// min font device pixel size at which to turn on high quality
pref("browser.display.auto_quality_min_font_size", 20);
pref("browser.enable_automatic_image_resizing", false);
pref("browser.enable_click_image_resizing", true);

// See http://whatwg.org/specs/web-apps/current-work/#ping
pref("browser.send_pings", false);
pref("browser.send_pings.max_per_link", 1);           // limit the number of pings that are sent per link click
pref("browser.send_pings.require_same_host", false);  // only send pings to the same host if this is true

pref("browser.helperApps.neverAsk.saveToDisk", "");
pref("browser.helperApps.neverAsk.openFile", "");
pref("browser.helperApps.deleteTempFileOnExit", false);

// xxxbsmedberg: where should prefs for the toolkit go?
pref("browser.chrome.toolbar_tips",         true);
// max image size for which it is placed in the tab icon for tabbrowser.
// if 0, no images are used for tab icons for image documents.
pref("browser.chrome.image_icons.max_size", 1024);

pref("browser.triple_click_selects_paragraph", true);

// Print/Preview Shrink-To-Fit won't shrink below 20% for text-ish documents.
pref("print.shrink-to-fit.scale-limit-percent", 20);

// Whether we should display simplify page checkbox on print preview UI
pref("print.use_simplify_page", false);

// Disable support for MathML
pref("mathml.disabled",    false);

// Enable scale transform for stretchy MathML operators. See bug 414277.
pref("mathml.scale_stretchy_operators.enabled", true);

// Used by ChannelMediaResource to run data callbacks from HTTP channel
// off the main thread.
pref("media.omt_data_delivery.enabled", true);

// We'll throttle the download if the download rate is throttle-factor times
// the estimated playback rate, AND we satisfy the cache readahead_limit
// above. The estimated playback rate is time_duration/length_in_bytes.
// This means we'll only throttle the download if there's no concern that
// throttling would cause us to stop and buffer.
pref("media.throttle-factor", 2);
// By default, we'll throttle media download once we've reached the the
// readahead_limit if the download is fast. This pref toggles the "and the
// download is fast" check off, so that we can always throttle the download
// once the readaheadd limit is reached even on a slow network.
pref("media.throttle-regardless-of-download-rate", false);

// Master HTML5 media volume scale.
pref("media.volume_scale", "1.0");

// Whether we should play videos opened in a "video document", i.e. videos
// opened as top-level documents, as opposed to inside a media element.
pref("media.play-stand-alone", true);

pref("media.hardware-video-decoding.enabled", true);

#ifdef MOZ_WMF
  pref("media.wmf.dxva.enabled", true);
  pref("media.wmf.dxva.d3d11.enabled", true);
  pref("media.wmf.dxva.max-videos", 8);
  pref("media.wmf.low-latency.enabled", false);
  pref("media.wmf.low-latency.force-disabled", false);
  pref("media.wmf.amd.highres.enabled", true);
  pref("media.wmf.allow-unsupported-resolutions", false);
  pref("media.wmf.use-nv12-format", true);
  pref("media.wmf.force.allow-p010-format", false);
  pref("media.wmf.disable-d3d11-for-dlls", "igd11dxva64.dll: 20.19.15.4463, 20.19.15.4454, 20.19.15.4444, 20.19.15.4416, 20.19.15.4404, 20.19.15.4390, 20.19.15.4380, 20.19.15.4377, 20.19.15.4364, 20.19.15.4360, 20.19.15.4352, 20.19.15.4331, 20.19.15.4326, 20.19.15.4300; igd10iumd32.dll: 20.19.15.4444, 20.19.15.4424, 20.19.15.4409, 20.19.15.4390, 20.19.15.4380, 20.19.15.4360, 10.18.10.4358, 20.19.15.4331, 20.19.15.4312, 20.19.15.4300, 10.18.15.4281, 10.18.15.4279, 10.18.10.4276, 10.18.15.4268, 10.18.15.4256, 10.18.10.4252, 10.18.15.4248, 10.18.14.4112, 10.18.10.3958, 10.18.10.3496, 10.18.10.3431, 10.18.10.3412, 10.18.10.3355, 9.18.10.3234, 9.18.10.3071, 9.18.10.3055, 9.18.10.3006; igd10umd32.dll: 9.17.10.4229, 9.17.10.3040, 9.17.10.2884, 9.17.10.2857, 8.15.10.2274, 8.15.10.2272, 8.15.10.2246, 8.15.10.1840, 8.15.10.1808; igd10umd64.dll: 9.17.10.4229, 9.17.10.2884, 9.17.10.2857, 10.18.10.3496; isonyvideoprocessor.dll: 4.1.2247.8090, 4.1.2153.6200; tosqep.dll: 1.2.15.526, 1.1.12.201, 1.0.11.318, 1.0.11.215, 1.0.10.1224; tosqep64.dll: 1.1.12.201, 1.0.11.215; nvwgf2um.dll: 22.21.13.8253, 22.21.13.8233, 22.21.13.8205, 22.21.13.8189, 22.21.13.8178, 22.21.13.8165, 21.21.13.7892, 21.21.13.7878, 21.21.13.7866, 21.21.13.7849, 21.21.13.7654, 21.21.13.7653, 21.21.13.7633, 21.21.13.7619, 21.21.13.7563, 21.21.13.7306, 21.21.13.7290, 21.21.13.7270, 21.21.13.7254, 21.21.13.6939, 21.21.13.6926, 21.21.13.6909, 21.21.13.4201, 21.21.13.4200, 10.18.13.6881, 10.18.13.6839, 10.18.13.6510, 10.18.13.6472, 10.18.13.6143, 10.18.13.5946, 10.18.13.5923, 10.18.13.5921, 10.18.13.5891, 10.18.13.5887, 10.18.13.5582, 10.18.13.5445, 10.18.13.5382, 10.18.13.5362, 9.18.13.4788, 9.18.13.4752, 9.18.13.4725, 9.18.13.4709, 9.18.13.4195, 9.18.13.4192, 9.18.13.4144, 9.18.13.4052, 9.18.13.3788, 9.18.13.3523, 9.18.13.3235, 9.18.13.3165, 9.18.13.2723, 9.18.13.2702, 9.18.13.1422, 9.18.13.1407, 9.18.13.1106, 9.18.13.546; atidxx32.dll: 21.19.151.3, 21.19.142.257, 21.19.137.514, 21.19.137.1, 21.19.134.1, 21.19.128.7, 21.19.128.4, 20.19.0.32837, 20.19.0.32832, 8.17.10.682, 8.17.10.671, 8.17.10.661, 8.17.10.648, 8.17.10.644, 8.17.10.625, 8.17.10.605, 8.17.10.581, 8.17.10.569, 8.17.10.560, 8.17.10.545, 8.17.10.539, 8.17.10.531, 8.17.10.525, 8.17.10.520, 8.17.10.519, 8.17.10.514, 8.17.10.511, 8.17.10.494, 8.17.10.489, 8.17.10.483, 8.17.10.453, 8.17.10.451, 8.17.10.441, 8.17.10.436, 8.17.10.432, 8.17.10.425, 8.17.10.418, 8.17.10.414, 8.17.10.401, 8.17.10.395, 8.17.10.385, 8.17.10.378, 8.17.10.362, 8.17.10.355, 8.17.10.342, 8.17.10.331, 8.17.10.318, 8.17.10.310, 8.17.10.286, 8.17.10.269, 8.17.10.261, 8.17.10.247, 8.17.10.240, 8.15.10.212; atidxx64.dll: 21.19.151.3, 21.19.142.257, 21.19.137.514, 21.19.137.1, 21.19.134.1, 21.19.128.7, 21.19.128.4, 20.19.0.32832, 8.17.10.682, 8.17.10.661, 8.17.10.644, 8.17.10.625; nvumdshim.dll: 10.18.13.6822");
  pref("media.wmf.disable-d3d9-for-dlls", "igdumd64.dll: 8.15.10.2189, 8.15.10.2119, 8.15.10.2104, 8.15.10.2102, 8.771.1.0; atiumd64.dll: 7.14.10.833, 7.14.10.867, 7.14.10.885, 7.14.10.903, 7.14.10.911, 8.14.10.768, 9.14.10.1001, 9.14.10.1017, 9.14.10.1080, 9.14.10.1128, 9.14.10.1162, 9.14.10.1171, 9.14.10.1183, 9.14.10.1197, 9.14.10.945, 9.14.10.972, 9.14.10.984, 9.14.10.996");
  pref("media.wmf.deblacklisting-for-telemetry-in-gpu-process", false);
  pref("media.wmf.play-stand-alone", true);
#endif
pref("media.gmp.decoder.aac", 0);
pref("media.gmp.decoder.h264", 0);

// GMP storage version number. At startup we check the version against
// media.gmp.storage.version.observed, and if the versions don't match,
// we clear storage and set media.gmp.storage.version.observed=expected.
// This provides a mechanism to clear GMP storage when non-compatible
// changes are made.
pref("media.gmp.storage.version.expected", 1);

// Filter what triggers user notifications.
// See DecoderDoctorDocumentWatcher::ReportAnalysis for details.
#ifdef NIGHTLY_BUILD
  pref("media.decoder-doctor.notifications-allowed", "MediaWMFNeeded,MediaCannotInitializePulseAudio,MediaCannotPlayNoDecoders,MediaUnsupportedLibavcodec,MediaDecodeError");
#else
  pref("media.decoder-doctor.notifications-allowed", "MediaWMFNeeded,MediaCannotInitializePulseAudio,MediaCannotPlayNoDecoders,MediaUnsupportedLibavcodec");
#endif
pref("media.decoder-doctor.decode-errors-allowed", "");
pref("media.decoder-doctor.decode-warnings-allowed", "");
// Whether we report partial failures.
pref("media.decoder-doctor.verbose", false);
// URL to report decode issues
pref("media.decoder-doctor.new-issue-endpoint", "data:text/plain,");

pref("media.videocontrols.picture-in-picture.enabled", false);
pref("media.videocontrols.picture-in-picture.video-toggle.enabled", false);
pref("media.videocontrols.picture-in-picture.video-toggle.flyout-enabled", false);
pref("media.videocontrols.picture-in-picture.video-toggle.flyout-wait-ms", 5000);
pref("media.videocontrols.picture-in-picture.video-toggle.always-show", false);

#ifdef MOZ_WEBRTC
  pref("media.navigator.video.enabled", true);
  pref("media.navigator.video.default_fps",30);
  pref("media.navigator.video.use_remb", true);
  pref("media.navigator.video.use_tmmbr", false);
  pref("media.navigator.audio.use_fec", true);
  pref("media.navigator.video.red_ulpfec_enabled", false);

  #ifdef NIGHTLY_BUILD
    pref("media.peerconnection.sdp.parser", "sipcc");
    pref("media.peerconnection.sdp.alternate_parse_mode", "never");
    pref("media.peerconnection.sdp.strict_success", false);
  #else
    pref("media.peerconnection.sdp.parser", "sipcc");
    pref("media.peerconnection.sdp.alternate_parse_mode", "never");
    pref("media.peerconnection.sdp.strict_success", false);
  #endif

  pref("media.webrtc.debug.trace_mask", 0);
  pref("media.webrtc.debug.multi_log", false);
  pref("media.webrtc.debug.log_file", "");
  pref("media.webrtc.debug.aec_dump_max_size", 4194304); // 4MB

  pref("media.navigator.video.default_width",0);  // adaptive default
  pref("media.navigator.video.default_height",0); // adaptive default
  pref("media.peerconnection.video.enabled", true);
  pref("media.navigator.video.max_fs", 12288); // Enough for 2048x1536
  pref("media.navigator.video.max_fr", 60);
  pref("media.navigator.video.h264.level", 31); // 0x42E01f - level 3.1
  pref("media.navigator.video.h264.max_br", 0);
  pref("media.navigator.video.h264.max_mbps", 0);
  pref("media.peerconnection.video.vp9_enabled", true);
  pref("media.peerconnection.video.vp9_preferred", false);
  pref("media.getusermedia.browser.enabled", false);
  pref("media.getusermedia.channels", 0);
  #if defined(ANDROID)
    pref("media.getusermedia.camera.off_while_disabled.enabled", false);
    pref("media.getusermedia.microphone.off_while_disabled.enabled", false);
  #else
    pref("media.getusermedia.camera.off_while_disabled.enabled", true);
    pref("media.getusermedia.microphone.off_while_disabled.enabled", true);
  #endif
  pref("media.getusermedia.camera.off_while_disabled.delay_ms", 3000);
  pref("media.getusermedia.microphone.off_while_disabled.delay_ms", 3000);
  // Desktop is typically VGA capture or more; and qm_select will not drop resolution
  // below 1/2 in each dimension (or so), so QVGA (320x200) is the lowest here usually.
  pref("media.peerconnection.video.min_bitrate", 0);
  pref("media.peerconnection.video.start_bitrate", 0);
  pref("media.peerconnection.video.max_bitrate", 0);
  pref("media.peerconnection.video.min_bitrate_estimate", 0);
  pref("media.peerconnection.video.denoising", false);
  pref("media.navigator.audio.fake_frequency", 1000);
  pref("media.navigator.permission.disabled", false);
  pref("media.navigator.streams.fake", false);
  pref("media.peerconnection.simulcast", true);
  pref("media.peerconnection.default_iceservers", "[]");
  pref("media.peerconnection.ice.loopback", false); // Set only for testing in offline environments.
  pref("media.peerconnection.ice.tcp", true);
  pref("media.peerconnection.ice.tcp_so_sock_count", 0); // Disable SO gathering
  pref("media.peerconnection.ice.link_local", false); // Set only for testing IPV6 in networks that don't assign IPV6 addresses
  pref("media.peerconnection.ice.force_interface", ""); // Limit to only a single interface
  pref("media.peerconnection.ice.relay_only", false); // Limit candidates to TURN
  pref("media.peerconnection.use_document_iceservers", true);

  pref("media.peerconnection.identity.timeout", 10000);
  pref("media.peerconnection.ice.stun_client_maximum_transmits", 7);
  pref("media.peerconnection.ice.trickle_grace_period", 5000);
  pref("media.peerconnection.ice.no_host", false);
  pref("media.peerconnection.ice.default_address_only", false);
  pref("media.peerconnection.ice.obfuscate_host_addresses", false);
  pref("media.peerconnection.ice.proxy_only_if_behind_proxy", false);
  pref("media.peerconnection.ice.proxy_only", false);
  pref("media.peerconnection.turn.disable", false);
  pref("media.peerconnection.mute_on_bye_or_timeout", false);

  // 770 = DTLS 1.0, 771 = DTLS 1.2
  pref("media.peerconnection.dtls.version.min", 770);
  pref("media.peerconnection.dtls.version.max", 771);

  // These values (aec, agc, and noise) are from:
  // media/webrtc/trunk/webrtc/modules/audio_processing/include/audio_processing.h
  #if defined(MOZ_WEBRTC_HARDWARE_AEC_NS)
    pref("media.getusermedia.aec_enabled", false);
    pref("media.getusermedia.noise_enabled", false);
  #else
    pref("media.getusermedia.aec_enabled", true);
    pref("media.getusermedia.noise_enabled", true);
  #endif
  pref("media.getusermedia.use_aec_mobile", false);
  pref("media.getusermedia.aec", 1); // kModerateSuppression
  pref("media.getusermedia.aec_extended_filter", true);
  pref("media.getusermedia.noise", 1); // kModerate
  pref("media.getusermedia.agc_enabled", true);
  pref("media.getusermedia.agc", 1); // kAdaptiveDigital
  // full_duplex: enable cubeb full-duplex capture/playback
  pref("media.navigator.audio.full_duplex", true);
#endif // MOZ_WEBRTC

#if !defined(ANDROID)
  pref("media.getusermedia.screensharing.enabled", true);
#endif

pref("media.getusermedia.audiocapture.enabled", false);

// WebVTT pseudo element and class support.
pref("media.webvtt.pseudo.enabled", true);

// Whether to allow recording of AudioNodes with MediaRecorder
pref("media.recorder.audio_node.enabled", false);

// Whether MediaRecorder's video encoder should allow dropping frames in order
// to keep up under load. Useful for tests but beware of memory consumption!
pref("media.recorder.video.frame_drops", true);

// Whether to autostart a media element with an |autoplay| attribute.
// ALLOWED=0, BLOCKED=1, defined in dom/media/Autoplay.idl
pref("media.autoplay.default", 0);

// By default, don't block WebAudio from playing automatically.
pref("media.autoplay.block-webaudio", false);

// By default, don't block the media from extension background script.
pref("media.autoplay.allow-extension-background-pages", true);

// The default number of decoded video frames that are enqueued in
// MediaDecoderReader's mVideoQueue.
pref("media.video-queue.default-size", 10);

// The maximum number of queued frames to send to the compositor.
// By default, send all of them.
pref("media.video-queue.send-to-compositor-size", 9999);

// Log level for cubeb, the audio input/output system. Valid values are
// "verbose", "normal" and "" (log disabled).
pref("media.cubeb.logging_level", "");

// Cubeb sandbox (remoting) control
#if defined(XP_LINUX) && !defined(MOZ_WIDGET_ANDROID)
  pref("media.audioipc.pool_size", 1);
  // 64 * 4 kB stack per pool thread.
  pref("media.audioipc.stack_size", 262144);
#endif

// GraphRunner (fixed MediaTrackGraph thread) control
pref("media.audiograph.single_thread.enabled", false);

// APZ preferences. For documentation/details on what these prefs do, check
// gfx/layers/apz/src/AsyncPanZoomController.cpp.
pref("apz.overscroll.stop_velocity_threshold", "0.01");
pref("apz.overscroll.stretch_factor", "0.35");

#ifdef XP_MACOSX
  // Whether to run in native HiDPI mode on machines with "Retina"/HiDPI
  // display.
  //   <= 0 : hidpi mode disabled, display will just use pixel-based upscaling.
  //   == 1 : hidpi supported if all screens share the same backingScaleFactor.
  //   >= 2 : hidpi supported even with mixed backingScaleFactors (somewhat
  //          broken).
  pref("gfx.hidpi.enabled", 2);
#endif

pref("gfx.color_management.display_profile", "");

pref("gfx.downloadable_fonts.enabled", true);
pref("gfx.downloadable_fonts.fallback_delay", 3000);
pref("gfx.downloadable_fonts.fallback_delay_short", 100);

// disable downloadable font cache so that behavior is consistently
// the uncached load behavior across pages (useful for testing reflow problems)
pref("gfx.downloadable_fonts.disable_cache", false);

#ifdef ANDROID
  pref("gfx.bundled_fonts.enabled", true);
  pref("gfx.bundled_fonts.force-enabled", false);
#endif

// Do we fire a notification about missing fonts, so the front-end can decide
// whether to try and do something about it (e.g. download additional fonts)?
pref("gfx.missing_fonts.notify", false);

// prefs controlling the font (name/cmap) loader that runs shortly after startup
#ifdef XP_WIN
  pref("gfx.font_loader.delay", 120000);       // 2 minutes after startup
  pref("gfx.font_loader.interval", 1000);      // every 1 second until complete
#else
  pref("gfx.font_loader.delay", 8000);         // 8 secs after startup
  pref("gfx.font_loader.interval", 50);        // run every 50 ms
#endif

// whether to always search all font cmaps during system font fallback
pref("gfx.font_rendering.fallback.always_use_cmaps", false);

// cache shaped word results
pref("gfx.font_rendering.wordcache.charlimit", 32);

// cache shaped word results
pref("gfx.font_rendering.wordcache.maxentries", 10000);

pref("gfx.font_rendering.graphite.enabled", true);

#ifdef XP_WIN
  pref("gfx.font_rendering.directwrite.use_gdi_table_loading", true);
#endif

#if defined(XP_WIN)
  // comma separated list of backends to use in order of preference
  // e.g., pref("gfx.canvas.azure.backends", "direct2d,skia,cairo");
  pref("gfx.canvas.azure.backends", "direct2d1.1,skia,cairo");
  pref("gfx.content.azure.backends", "direct2d1.1,skia,cairo");
#elif defined(XP_MACOSX)
  pref("gfx.content.azure.backends", "skia");
  pref("gfx.canvas.azure.backends", "skia");
#else
  pref("gfx.canvas.azure.backends", "skia");
  pref("gfx.content.azure.backends", "skia");
#endif

#ifdef MOZ_BUILD_WEBRENDER
#ifdef XP_WIN
  pref("gfx.webrender.force-angle", true);
  pref("gfx.webrender.flip-sequential", false);
  pref("gfx.webrender.dcomp-win.enabled", true);
  pref("gfx.webrender.triple-buffering.enabled", true);
#endif

#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID)
  pref("gfx.webrender.program-binary-disk", true);
#endif

// WebRender debugging utilities.
pref("gfx.webrender.debug.texture-cache", false);
pref("gfx.webrender.debug.texture-cache.clear-evicted", true);
pref("gfx.webrender.debug.render-targets", false);
pref("gfx.webrender.debug.gpu-cache", false);
pref("gfx.webrender.debug.alpha-primitives", false);
pref("gfx.webrender.debug.profiler", false);
pref("gfx.webrender.debug.gpu-time-queries", false);
pref("gfx.webrender.debug.gpu-sample-queries", false);
pref("gfx.webrender.debug.disable-batching", false);
pref("gfx.webrender.debug.epochs", false);
pref("gfx.webrender.debug.compact-profiler", false);
pref("gfx.webrender.debug.smart-profiler", false);
pref("gfx.webrender.debug.echo-driver-messages", false);
pref("gfx.webrender.debug.new-frame-indicator", false);
pref("gfx.webrender.debug.new-scene-indicator", false);
pref("gfx.webrender.debug.show-overdraw", false);
pref("gfx.webrender.debug.slow-frame-indicator", false);
pref("gfx.webrender.debug.picture-caching", false);
pref("gfx.webrender.debug.primitives", false);
pref("gfx.webrender.debug.small-screen", false);
pref("gfx.webrender.debug.obscure-images", false);
pref("gfx.webrender.debug.glyph-flashing", false);
#endif

pref("accessibility.warn_on_browsewithcaret", true);

pref("accessibility.browsewithcaret_shortcut.enabled", true);

#ifndef XP_MACOSX
  // Tab focus model bit field:
  // 1 focuses text controls, 2 focuses other form elements, 4 adds links.
  // Most users will want 1, 3, or 7.
  // On OS X, we use Full Keyboard Access system preference,
  // unless accessibility.tabfocus is set by the user.
  pref("accessibility.tabfocus", 7);
  pref("accessibility.tabfocus_applies_to_xul", false);
#else
  // Only on mac tabfocus is expected to handle UI widgets as well as web
  // content.
  pref("accessibility.tabfocus_applies_to_xul", true);
#endif

// We follow the "Click in the scrollbar to:" system preference on OS X and
// "gtk-primary-button-warps-slider" property with GTK (since 2.24 / 3.6),
// unless this preference is explicitly set.
#if !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GTK)
  pref("ui.scrollToClick", 0);
#endif

// We want the ability to forcibly disable platform a11y, because
// some non-a11y-related components attempt to bring it up.  See bug
// 538530 for details about Windows; we have a pref here that allows it
// to be disabled for performance and testing resons.
// See bug 761589 for the crossplatform aspect.
//
// This pref is checked only once, and the browser needs a restart to
// pick up any changes.
//
// Values are -1 always on. 1 always off, 0 is auto as some platform perform
// further checks.
pref("accessibility.force_disabled", 0);

#ifdef XP_WIN
  // Some accessibility tools poke at windows in the plugin process during
  // setup which can cause hangs.  To hack around this set
  // accessibility.delay_plugins to true, you can also try increasing
  // accessibility.delay_plugin_time if your machine is slow and you still
  // experience hangs. See bug 781791.
  pref("accessibility.delay_plugins", false);
  pref("accessibility.delay_plugin_time", 10000);

  // The COM handler used for Windows e10s performance and live regions.
  pref("accessibility.handler.enabled", true);
#endif

pref("focusmanager.testmode", false);

pref("accessibility.usetexttospeech", "");
pref("accessibility.mouse_focuses_formcontrol", false);

// Type Ahead Find
pref("accessibility.typeaheadfind", true);
// Enable FAYT by pressing / or "
pref("accessibility.typeaheadfind.manual", true);
pref("accessibility.typeaheadfind.autostart", true);
// casesensitive: controls the find bar's case-sensitivity
//     0 - "never"  (case-insensitive)
//     1 - "always" (case-sensitive)
// other - "auto"   (case-sensitive for mixed-case input, insensitive otherwise)
pref("accessibility.typeaheadfind.casesensitive", 0);
pref("accessibility.typeaheadfind.linksonly", true);
pref("accessibility.typeaheadfind.startlinksonly", false);
// timeout: controls the delay in milliseconds after which the quick-find dialog will close
//          if no further keystrokes are pressed
//              set to a zero or negative value to keep dialog open until it's manually closed
pref("accessibility.typeaheadfind.timeout", 4000);
pref("accessibility.typeaheadfind.soundURL", "beep");
pref("accessibility.typeaheadfind.enablesound", true);
#ifdef XP_MACOSX
  pref("accessibility.typeaheadfind.prefillwithselection", false);
#else
  pref("accessibility.typeaheadfind.prefillwithselection", true);
#endif
pref("accessibility.typeaheadfind.matchesCountLimit", 1000);
pref("findbar.highlightAll", false);
pref("findbar.entireword", false);
pref("findbar.iteratorTimeout", 100);

// use Mac OS X Appearance panel text smoothing setting when rendering text, disabled by default
pref("gfx.use_text_smoothing_setting", false);

// Number of characters to consider emphasizing for rich autocomplete results
pref("toolkit.autocomplete.richBoundaryCutoff", 200);

// Variable controlling logging for osfile.
pref("toolkit.osfile.log", false);

pref("toolkit.cosmeticAnimations.enabled", true);

pref("toolkit.scrollbox.smoothScroll", true);
pref("toolkit.scrollbox.scrollIncrement", 20);
pref("toolkit.scrollbox.clickToScroll.scrollDelay", 150);

pref("toolkit.tabbox.switchByScrolling", false);

// Telemetry settings.
pref("toolkit.telemetry.enabled", false);
// Server to submit telemetry pings to.
pref("toolkit.telemetry.server", "data:text/plain,");
// Telemetry server owner. Please change if you set toolkit.telemetry.server to a different server
pref("toolkit.telemetry.server_owner", "Mozilla");
// Determines whether full SQL strings are returned when they might contain sensitive info
// i.e. dynamically constructed SQL strings or SQL executed by addons against addon DBs
pref("toolkit.telemetry.debugSlowSql", false);
// Whether to use the unified telemetry behavior, requires a restart.
pref("toolkit.telemetry.unified", false);
// AsyncShutdown delay before crashing in case of shutdown freeze
#ifndef MOZ_ASAN
  pref("toolkit.asyncshutdown.crash_timeout", 60000); // 1 minute
#else
  // MOZ_ASAN builds can be considerably slower. Extending the grace period
  // of both asyncshutdown and the terminator.
  pref("toolkit.asyncshutdown.crash_timeout", 180000); // 3 minutes
#endif // MOZ_ASAN
// Extra logging for AsyncShutdown barriers and phases
pref("toolkit.asyncshutdown.log", false);

pref("toolkit.content-background-hang-monitor.disabled", false);

// Enable JS dump() function.
// IMPORTANT: These prefs must be here even though they're also defined in
// StaticPrefList.yaml. They are required because MOZILLA_OFFICIAL is false in
// local full builds but true in artifact builds. Without these definitions
// here, dumping is disabled in artifact builds (see Bug 1490412).
#ifdef MOZILLA_OFFICIAL
  pref("browser.dom.window.dump.enabled", false, sticky);
  pref("devtools.console.stdout.chrome", false, sticky);
#else
  pref("browser.dom.window.dump.enabled", true, sticky);
  pref("devtools.console.stdout.chrome", true, sticky);
#endif

pref("devtools.console.stdout.content", false, sticky);

// Controls whether EventEmitter module throws dump message on each emit
pref("toolkit.dump.emit", false);

// view source
pref("view_source.syntax_highlight", true);
pref("view_source.wrap_long_lines", false);
pref("view_source.editor.path", "");
// allows to add further arguments to the editor; use the %LINE% placeholder
// for jumping to a specific line (e.g. "/line:%LINE%" or "--goto %LINE%")
pref("view_source.editor.args", "");

// whether or not to draw images while dragging
pref("nglayout.enable_drag_images", true);

// enable/disable paint flashing --- useful for debugging
// the first one applies to everything, the second one only to chrome
pref("nglayout.debug.paint_flashing", false);
pref("nglayout.debug.paint_flashing_chrome", false);

// URI fixup prefs
pref("browser.fixup.alternate.enabled", true);
pref("browser.fixup.alternate.prefix", "www.");
pref("browser.fixup.alternate.suffix", ".com");

// Print header customization
// Use the following codes:
// &T - Title
// &U - Document URL
// &D - Date/Time
// &P - Page Number
// &PT - Page Number "of" Page total
// Set each header to a string containing zero or one of these codes
// and the code will be replaced in that string by the corresponding data
pref("print.print_headerleft", "&T");
pref("print.print_headercenter", "");
pref("print.print_headerright", "&U");
pref("print.print_footerleft", "&PT");
pref("print.print_footercenter", "");
pref("print.print_footerright", "&D");
pref("print.show_print_progress", true);

// xxxbsmedberg: more toolkit prefs

// When this is set to false each window has its own PrintSettings
// and a change in one window does not affect the others
pref("print.use_global_printsettings", true);

// Save the Printings after each print job
pref("print.save_print_settings", true);

// Enables you to specify the amount of the paper that is to be treated
// as unwriteable.  The print_edge_XXX and print_margin_XXX preferences
// are treated as offsets that are added to this pref.
// Default is "-1", which means "use the system default".  (If there is
// no system default, then the -1 is treated as if it were 0.)
// This is used by both Printing and Print Preview.
// Units are in 1/100ths of an inch.
pref("print.print_unwriteable_margin_top",    -1);
pref("print.print_unwriteable_margin_left",   -1);
pref("print.print_unwriteable_margin_right",  -1);
pref("print.print_unwriteable_margin_bottom", -1);

// Enables you to specify the gap from the edge of the paper's
// unwriteable area to the margin.
// This is used by both Printing and Print Preview
// Units are in 1/100ths of an inch.
pref("print.print_edge_top", 0);
pref("print.print_edge_left", 0);
pref("print.print_edge_right", 0);
pref("print.print_edge_bottom", 0);

// Print via the parent process. This is only used when e10s is enabled.
#if !defined(MOZ_WIDGET_ANDROID)
  pref("print.print_via_parent", true);
#else
  pref("print.print_via_parent", false);
#endif

// Pref used by the spellchecker extension to control the
// maximum number of misspelled words that will be underlined
// in a document.
pref("extensions.spellcheck.inline.max-misspellings", 500);

// Whether inserting <div> when typing Enter in a block element which can
// contain <div>.  If false, inserts <br> instead.
pref("editor.use_div_for_default_newlines",  true);

// Prefs specific to seamonkey composer belong in
// comm-central/editor/ui/composer.js
pref("editor.use_custom_colors", false);
pref("editor.use_css",                       false);
pref("editor.css.default_length_unit",       "px");
pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);

// Scripts & Windows prefs
pref("dom.disable_window_flip",             false);
pref("dom.disable_window_move_resize",      false);

pref("dom.disable_window_open_feature.titlebar",    false);
pref("dom.disable_window_open_feature.close",       false);
pref("dom.disable_window_open_feature.toolbar",     false);
pref("dom.disable_window_open_feature.location",    false);
pref("dom.disable_window_open_feature.personalbar", false);
pref("dom.disable_window_open_feature.menubar",     false);
pref("dom.disable_window_open_feature.resizable",   true);
pref("dom.disable_window_open_feature.minimizable", false);
pref("dom.disable_window_open_feature.status",      true);

pref("dom.allow_scripts_to_close_windows",          false);

pref("dom.popup_maximum",                           20);
pref("dom.popup_allowed_events", "change click dblclick auxclick mouseup pointerup notificationclick reset submit touchend contextmenu");

pref("dom.serviceWorkers.disable_open_click_delay", 1000);

pref("dom.storage.enabled", true);
// Whether or not LSNG (Next Generation Local Storage) is enabled.
// See bug 1517090 for enabling this on Nightly.
// See bug 1534736 for changing it to EARLY_BETA_OR_EARLIER.
// See bug 1539835 for enabling this unconditionally.
// See bug 1562942 for disabling this in late 68.
pref("dom.storage.next_gen", false);
pref("dom.storage.shadow_writes", true);
pref("dom.storage.snapshot_prefill", 16384);
pref("dom.storage.snapshot_gradual_prefill", 4096);
pref("dom.storage.snapshot_reusing", true);
pref("dom.storage.client_validation", true);

pref("dom.send_after_paint_to_content", false);

// Enable time picker UI. By default, disabled.
pref("dom.forms.datetime.timepicker", false);

// Enable search in <select> dropdowns (more than 40 options)
pref("dom.forms.selectSearch", false);
// Allow for webpages to provide custom styling for <select>
// popups. Disabled on GTK due to bug 1338283.
#ifdef MOZ_WIDGET_GTK
  pref("dom.forms.select.customstyling", false);
#else
  pref("dom.forms.select.customstyling", true);
#endif
pref("dom.select_popup_in_parent.enabled", false);

pref("dom.cycle_collector.incremental", true);

// Disable popups from plugins by default
//   0 = openAllowed
//   1 = openControlled
//   2 = openBlocked
//   3 = openAbused
pref("privacy.popups.disable_from_plugins", 3);

// Enable Paritioned LocalStorage for a list of hosts when detected as trackers
// (See nsICookieService::BEHAVIOR_REJECT_TRACKER cookie behavior)
pref("privacy.restrict3rdpartystorage.partitionedHosts", "accounts.google.com/o/oauth2/");

// If a host is contained in this pref list, user-interaction is required
// before granting the storage access permission.
pref("privacy.restrict3rdpartystorage.userInteractionRequiredForHosts", "");

// The url decoration tokens used to for stripping document referrers based on.
// A list separated by spaces.  This pref isn't meant to be changed by users.
pref("privacy.restrict3rdpartystorage.url_decorations", "");

// Excessive reporting of blocked popups can be a DOS vector,
// by overloading the main process as popups get blocked and when
// users try to restore all popups, which is the most visible
// option in our UI at the time of writing.
// We will invisibly drop any popups from a page that has already
// opened more than this number of popups.
pref("privacy.popups.maxReported", 100);


pref("dom.event.contextmenu.enabled",       true);
pref("dom.event.coalesce_mouse_move",       true);

pref("javascript.enabled",                  true);
pref("javascript.options.asmjs",                  true);
pref("javascript.options.wasm",                   true);
pref("javascript.options.wasm_trustedprincipals", true);
pref("javascript.options.wasm_verbose",           false);
pref("javascript.options.wasm_baselinejit",       true);

pref("javascript.options.parallel_parsing", true);
pref("javascript.options.source_pragmas",    true);

pref("javascript.options.asyncstack", true);
// Broadly capturing async stack data adds overhead that is only advisable for
// developers, so we only enable it when the devtools are open, by default.
pref("javascript.options.asyncstack_capture_debuggee_only", true);

pref("javascript.options.throw_on_asmjs_validation_failure", false);
// This preference instructs the JS engine to discard the
// source of any privileged JS after compilation. This saves
// memory, but makes things like Function.prototype.toSource()
// fail.
pref("javascript.options.discardSystemSource", false);

// Many of the the following preferences tune the SpiderMonkey GC, if you
// change the defaults here please also consider changing them in
// js/src/jsgc.cpp.  They're documented in js/src/jsapi.h.

// JSGC_MAX_BYTES
// SpiderMonkey defaults to 2^32-1 bytes, but this is measured in MB so that
// cannot be represented directly in order to show it in about:config.
pref("javascript.options.mem.max", -1);

// JSGC_MIN_NURSERY_BYTES / JSGC_MAX_NURSERY_BYTES
#if defined(ANDROID) || defined(XP_IOS)
  pref("javascript.options.mem.nursery.min_kb", 256);
  pref("javascript.options.mem.nursery.max_kb", 4096);
#else
  pref("javascript.options.mem.nursery.min_kb", 256);
  pref("javascript.options.mem.nursery.max_kb", 16384);
#endif

// JSGC_MODE
pref("javascript.options.mem.gc_per_zone", true);
pref("javascript.options.mem.gc_incremental", true);

// JSGC_SLICE_TIME_BUDGET_MS
// Override the shell's default of unlimited slice time.
pref("javascript.options.mem.gc_incremental_slice_ms", 5);

// JSGC_COMPACTING_ENABLED
pref("javascript.options.mem.gc_compacting", true);

// JSGC_HIGH_FREQUENCY_TIME_LIMIT
pref("javascript.options.mem.gc_high_frequency_time_limit_ms", 1000);

// JSGC_SMALL_HEAP_SIZE_MAX
pref("javascript.options.mem.gc_small_heap_size_max_mb", 100);

// JSGC_LARGE_HEAP_SIZE_MIN
pref("javascript.options.mem.gc_large_heap_size_min_mb", 500);

// JSGC_HIGH_FREQUENCY_SMALL_HEAP_GROWTH
pref("javascript.options.mem.gc_high_frequency_small_heap_growth", 300);

// JSGC_HIGH_FREQUENCY_LARGE_HEAP_GROWTH
pref("javascript.options.mem.gc_high_frequency_large_heap_growth", 150);

// JSGC_LOW_FREQUENCY_HEAP_GROWTH
pref("javascript.options.mem.gc_low_frequency_heap_growth", 150);

// JSGC_ALLOCATION_THRESHOLD
pref("javascript.options.mem.gc_allocation_threshold_mb", 27);

// JSGC_SMALL_HEAP_INCREMENTAL_LIMIT
pref("javascript.options.mem.gc_small_heap_incremental_limit", 140);

// JSGC_LARGE_HEAP_INCREMENTAL_LIMIT
pref("javascript.options.mem.gc_large_heap_incremental_limit", 110);

// JSGC_MIN_EMPTY_CHUNK_COUNT
pref("javascript.options.mem.gc_min_empty_chunk_count", 1);

// JSGC_MAX_EMPTY_CHUNK_COUNT
pref("javascript.options.mem.gc_max_empty_chunk_count", 30);

// JSGC_HELPER_THREAD_RATIO
pref("javascript.options.mem.gc_helper_thread_ratio", 50);

// JSGC_MAX_HELPER_THREADS
pref("javascript.options.mem.gc_max_helper_threads", 8);

pref("javascript.options.shared_memory", true);

pref("javascript.options.throw_on_debuggee_would_run", false);
pref("javascript.options.dump_stack_on_debuggee_would_run", false);

// Dynamic module import.
pref("javascript.options.dynamicImport", true);

// advanced prefs
pref("advanced.mailftp",                    false);
pref("image.animation_mode",                "normal");

// If this pref is true, prefs in the logging.config branch will be cleared on
// startup. This is done so that setting a log-file and log-modules at runtime
// doesn't persist across restarts leading to huge logfile and low disk space.
pref("logging.config.clear_on_startup", true);

// If there is ever a security firedrill that requires
// us to block certian ports global, this is the pref
// to use.  Is is a comma delimited list of port numbers
// for example:
//   pref("network.security.ports.banned", "1,2,3,4,5");
// prevents necko connecting to ports 1-5 unless the protocol
// overrides.

// Allow the network changed event to get sent when a network topology or
// setup change is noticed while running.
pref("network.notify.changed", true);

// Allow network detection of IPv6 related changes (bug 1245059)
#if defined(XP_WIN)
  pref("network.notify.IPv6", false);
#else
  pref("network.notify.IPv6", true);
#endif

// Transmit UDP busy-work to the LAN when anticipating low latency
// network reads and on wifi to mitigate 802.11 Power Save Polling delays
pref("network.tickle-wifi.enabled", false);
pref("network.tickle-wifi.duration", 400);
pref("network.tickle-wifi.delay", 16);

// Default action for unlisted external protocol handlers
pref("network.protocol-handler.external-default", true);      // OK to load
pref("network.protocol-handler.warn-external-default", true); // warn before load

// Prevent using external protocol handlers for these schemes
pref("network.protocol-handler.external.hcp", false);
pref("network.protocol-handler.external.vbscript", false);
pref("network.protocol-handler.external.javascript", false);
pref("network.protocol-handler.external.data", false);
pref("network.protocol-handler.external.ie.http", false);
pref("network.protocol-handler.external.iehistory", false);
pref("network.protocol-handler.external.ierss", false);
pref("network.protocol-handler.external.ms-help", false);
pref("network.protocol-handler.external.res", false);
pref("network.protocol-handler.external.shell", false);
pref("network.protocol-handler.external.vnd.ms.radio", false);
#ifdef XP_MACOSX
  pref("network.protocol-handler.external.help", false);
#endif
pref("network.protocol-handler.external.disk", false);
pref("network.protocol-handler.external.disks", false);
pref("network.protocol-handler.external.afp", false);
pref("network.protocol-handler.external.moz-icon", false);

// Don't allow  external protocol handlers for common typos
pref("network.protocol-handler.external.ttp", false);  // http
pref("network.protocol-handler.external.htp", false);  // http
pref("network.protocol-handler.external.ttps", false); // https
pref("network.protocol-handler.external.tps", false);  // https
pref("network.protocol-handler.external.ps", false);   // https
pref("network.protocol-handler.external.htps", false); // https
pref("network.protocol-handler.external.ile", false);  // file
pref("network.protocol-handler.external.le", false);   // file

// An exposed protocol handler is one that can be used in all contexts.  A
// non-exposed protocol handler is one that can only be used internally by the
// application.  For example, a non-exposed protocol would not be loaded by the
// application in response to a link click or a X-remote openURL command.
// Instead, it would be deferred to the system's external protocol handler.
// Only internal/built-in protocol handlers can be marked as exposed.

// This pref controls the default settings.  Per protocol settings can be used
// to override this value.
pref("network.protocol-handler.expose-all", true);

// Warning for about:networking page
pref("network.warnOnAboutNetworking", true);

// Example: make IMAP an exposed protocol
// pref("network.protocol-handler.expose.imap", true);

// Whether IOService.connectivity and NS_IsOffline depends on connectivity status
pref("network.manage-offline-status", true);
// If set to true, IOService.offline depends on IOService.connectivity
pref("network.offline-mirrors-connectivity", false);

// <http>
pref("network.http.version", "1.1");      // default
// pref("network.http.version", "1.0");   // uncomment this out in case of problems
// pref("network.http.version", "0.9");   // it'll work too if you're crazy
// keep-alive option is effectively obsolete. Nevertheless it'll work with
// some older 1.0 servers:

pref("network.http.proxy.version", "1.1");    // default
// pref("network.http.proxy.version", "1.0"); // uncomment this out in case of problems
                                              // (required if using junkbuster proxy)

// Whether we should respect the BE_CONSERVATIVE (aka nsIHttpChannelInternal.beConservative)
// flag when connecting to a proxy.  If the configured proxy accepts only TLS 1.3, system
// requests like updates will not pass through.  Setting this pref to false will fix that
// problem.
// Default at true to preserve the behavior we had before for backward compat.
pref("network.http.proxy.respect-be-conservative", true);

// this preference can be set to override the socket type used for normal
// HTTP traffic.  an empty value indicates the normal TCP/IP socket type.
pref("network.http.default-socket-type", "");

// There is a problem with some IIS7 servers that don't close the connection
// properly after it times out (bug #491541). Default timeout on IIS7 is
// 120 seconds. We need to reuse or drop the connection within this time.
// We set the timeout a little shorter to keep a reserve for cases when
// the packet is lost or delayed on the route.
pref("network.http.keep-alive.timeout", 115);

// Timeout connections if an initial response is not received after 5 mins.
pref("network.http.response.timeout", 300);

// Limit the absolute number of http connections.
// Note: the socket transport service will clamp the number below this if the OS
// cannot allocate that many FDs
#ifdef ANDROID
  pref("network.http.max-connections", 40);
#else
  pref("network.http.max-connections", 900);
#endif

// If NOT connecting via a proxy, then
// a new connection will only be attempted if the number of active persistent
// connections to the server is less then max-persistent-connections-per-server.
pref("network.http.max-persistent-connections-per-server", 6);

// Number of connections that we can open beyond the standard parallelism limit defined
// by max-persistent-connections-per-server/-proxy to handle urgent-start marked requests
pref("network.http.max-urgent-start-excessive-connections-per-host", 3);

// If connecting via a proxy, then a
// new connection will only be attempted if the number of active persistent
// connections to the proxy is less then max-persistent-connections-per-proxy.
pref("network.http.max-persistent-connections-per-proxy", 32);

// amount of time (in seconds) to suspend pending requests, before spawning a
// new connection, once the limit on the number of persistent connections per
// host has been reached.  however, a new connection will not be created if
// max-connections or max-connections-per-server has also been reached.
pref("network.http.request.max-start-delay", 10);

// If a connection is reset, we will retry it max-attempts times.
pref("network.http.request.max-attempts", 10);

// Maximum number of consecutive redirects before aborting.
pref("network.http.redirection-limit", 20);

// Enable http compression: comment this out in case of problems with 1.1
// NOTE: support for "compress" has been disabled per bug 196406.
// NOTE: separate values with comma+space (", "): see bug 576033
pref("network.http.accept-encoding", "gzip, deflate");
pref("network.http.accept-encoding.secure", "gzip, deflate, br");

// Prompt for redirects resulting in unsafe HTTP requests
pref("network.http.prompt-temp-redirect", false);

// If true generate CORRUPTED_CONTENT errors for entities that
// contain an invalid Assoc-Req response header
pref("network.http.assoc-req.enforce", false);

// On networks deploying QoS, it is recommended that these be lockpref()'d,
// since inappropriate marking can easily overwhelm bandwidth reservations
// for certain services (i.e. EF for VoIP, AF4x for interactive video,
// AF3x for broadcast/streaming video, etc)

// default value for HTTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class"
pref("network.http.qos", 0);

// The number of milliseconds after sending a SYN for an HTTP connection,
// to wait before trying a different connection. 0 means do not use a second
// connection.
pref("network.http.connection-retry-timeout", 250);

// The number of seconds after sending initial SYN for an HTTP connection
// to give up if the OS does not give up first
pref("network.http.connection-timeout", 90);

// Close a connection if tls handshake does not finish in given number of
// seconds.
pref("network.http.tls-handshake-timeout", 30);

// The number of seconds after which we time out a connection of a retry (fallback)
// socket when a certain IP family is already preferred.  This shorter connection
// timeout allows us to find out more quickly that e.g. an IPv6 host is no longer
// available and let us try an IPv4 address, if provided for the host name.
// Set to '0' to use the default connection timeout.
pref("network.http.fallback-connection-timeout", 5);

// The number of seconds to allow active connections to prove that they have
// traffic before considered stalled, after a network change has been detected
// and signalled.
pref("network.http.network-changed.timeout", 5);

// The maximum number of current global half open sockets allowable
// when starting a new speculative connection.
pref("network.http.speculative-parallel-limit", 6);

// Whether or not to block requests for non head js/css items (e.g. media)
// while those elements load.
pref("network.http.rendering-critical-requests-prioritization", true);

// Disable IPv6 for backup connections to workaround problems about broken
// IPv6 connectivity.
pref("network.http.fast-fallback-to-IPv4", true);

// Try and use SPDY when using SSL
pref("network.http.spdy.enabled", true);
pref("network.http.spdy.enabled.http2", true);
pref("network.http.spdy.enabled.deps", true);
pref("network.http.spdy.enforce-tls-profile", true);
pref("network.http.spdy.chunk-size", 16000);
pref("network.http.spdy.timeout", 170);
pref("network.http.spdy.coalesce-hostnames", true);
pref("network.http.spdy.persistent-settings", false);
pref("network.http.spdy.ping-threshold", 58);
pref("network.http.spdy.ping-timeout", 8);
pref("network.http.spdy.send-buffer-size", 131072);
pref("network.http.spdy.allow-push", true);
pref("network.http.spdy.push-allowance", 131072);   // 128KB
pref("network.http.spdy.pull-allowance", 12582912); // 12MB
pref("network.http.spdy.default-concurrent", 100);
pref("network.http.spdy.default-hpack-buffer", 65536); // 64k
pref("network.http.spdy.websockets", true);
pref("network.http.spdy.enable-hpack-dump", false);

// alt-svc allows separation of transport routing from
// the origin host without using a proxy.
pref("network.http.altsvc.enabled", true);
pref("network.http.altsvc.oe", true);

// Turn on 0RTT data for TLS 1.3
pref("security.tls.enable_0rtt_data", true);

// the origin extension impacts h2 coalescing
pref("network.http.originextension", true);

pref("network.http.diagnostics", false);

pref("network.http.pacing.requests.enabled", true);
pref("network.http.pacing.requests.min-parallelism", 6);
pref("network.http.pacing.requests.hz", 80);
pref("network.http.pacing.requests.burst", 10);

// TCP Keepalive config for HTTP connections.
pref("network.http.tcp_keepalive.short_lived_connections", true);
// Max time from initial request during which conns are considered short-lived.
pref("network.http.tcp_keepalive.short_lived_time", 60);
// Idle time of TCP connection until first keepalive probe sent.
pref("network.http.tcp_keepalive.short_lived_idle_time", 10);

pref("network.http.tcp_keepalive.long_lived_connections", true);
pref("network.http.tcp_keepalive.long_lived_idle_time", 600);

pref("network.http.enforce-framing.http1", false); // should be named "strict"
pref("network.http.enforce-framing.soft", true);
pref("network.http.enforce-framing.strict_chunked_encoding", true);

// Max size, in bytes, for received HTTP response header.
pref("network.http.max_response_header_size", 393216);

// If we should attempt to race the cache and network
pref("network.http.rcwn.enabled", true);
pref("network.http.rcwn.cache_queue_normal_threshold", 8);
pref("network.http.rcwn.cache_queue_priority_threshold", 2);
// We might attempt to race the cache with the network only if a resource
// is smaller than this size.
pref("network.http.rcwn.small_resource_size_kb", 256);

pref("network.http.rcwn.min_wait_before_racing_ms", 0);
pref("network.http.rcwn.max_wait_before_racing_ms", 500);

// The ratio of the transaction count for the focused window and the count of
// all available active connections.
pref("network.http.focused_window_transaction_ratio", "0.9");

// This is the size of the flow control window (KB) (i.e., the amount of data
// that the parent can send to the child before getting an ack). 0 for disable
// the flow control.
pref("network.http.send_window_size", 1024);

// Whether or not we give more priority to active tab.
// Note that this requires restart for changes to take effect.
#ifdef ANDROID
  // disabled because of bug 1382274
  pref("network.http.active_tab_priority", false);
#else
  pref("network.http.active_tab_priority", true);
#endif

// default values for FTP
// in a DSCP environment this should be 40 (0x28, or AF11), per RFC-4594,
// Section 4.8 "High-Throughput Data Service Class", and 80 (0x50, or AF22)
// per Section 4.7 "Low-Latency Data Service Class".
pref("network.ftp.data.qos", 0);
pref("network.ftp.control.qos", 0);
pref("network.ftp.enabled", true);

// The max time to spend on xpcom events between two polls in ms.
pref("network.sts.max_time_for_events_between_two_polls", 100);

// The number of seconds we don't let poll() handing indefinitely after network
// link change has been detected so we can detect breakout of the pollable event.
// Expected in seconds, 0 to disable.
pref("network.sts.poll_busy_wait_period", 50);

// The number of seconds we cap poll() timeout to during the network link change
// detection period.
// Expected in seconds, 0 to disable.
pref("network.sts.poll_busy_wait_period_timeout", 7);

// During shutdown we limit PR_Close calls. If time exceeds this pref (in ms)
// let sockets just leak.
pref("network.sts.max_time_for_pr_close_during_shutdown", 5000);

// When the polling socket pair we use to wake poll() up on demand doesn't
// get signalled (is not readable) within this timeout, we try to repair it.
// This timeout can be disabled by setting this pref to 0.
// The value is expected in seconds.
pref("network.sts.pollable_event_timeout", 6);

// Start a separate socket process. Performing networking on the socket process
// is control by a sepparate pref
// ("network.http.network_access_on_socket_process.enabled").
// Changing these prefs requires a restart.
pref("network.process.enabled", false);

// <ws>: WebSocket
pref("network.websocket.enabled", true);

// 2147483647 == PR_INT32_MAX == ~2 GB
pref("network.websocket.max-message-size", 2147483647);

// Should we automatically follow http 3xx redirects during handshake
pref("network.websocket.auto-follow-http-redirects", false);

// the number of seconds to wait for websocket connection to be opened
pref("network.websocket.timeout.open", 20);

// the number of seconds to wait for a clean close after sending the client
// close message
pref("network.websocket.timeout.close", 20);

// the number of seconds of idle read activity to sustain before sending a
// ping probe. 0 to disable.
pref("network.websocket.timeout.ping.request", 0);

// the deadline, expressed in seconds, for some read activity to occur after
// generating a ping. If no activity happens then an error and unclean close
// event is sent to the javascript websockets application
pref("network.websocket.timeout.ping.response", 10);

// Defines whether or not to try to negotiate the permessage compression
// extension with the websocket server.
pref("network.websocket.extensions.permessage-deflate", true);

// the maximum number of concurrent websocket sessions. By specification there
// is never more than one handshake oustanding to an individual host at
// one time.
pref("network.websocket.max-connections", 200);

// by default scripts loaded from a https:// origin can only open secure
// (i.e. wss://) websockets.
pref("network.websocket.allowInsecureFromHTTPS", false);

// by default we delay websocket reconnects to same host/port if previous
// connection failed, per RFC 6455 section 7.2.3
pref("network.websocket.delay-failed-reconnects", true);

// </ws>

// Server-Sent Events
// Equal to the DEFAULT_RECONNECTION_TIME_VALUE value in nsEventSource.cpp
pref("dom.server-events.default-reconnection-time", 5000); // in milliseconds

// This preference, if true, causes all UTF-8 domain names to be normalized to
// punycode.  The intention is to allow UTF-8 domain names as input, but never
// generate them from punycode.
pref("network.IDN_show_punycode", false);

// If "network.IDN.use_whitelist" is set to true, TLDs with
// "network.IDN.whitelist.tld" explicitly set to true are treated as
// IDN-safe. Otherwise, they're treated as unsafe and punycode will be used
// for displaying them in the UI (e.g. URL bar), unless they conform to one of
// the profiles specified in
// https://www.unicode.org/reports/tr39/#Restriction_Level_Detection
// If "network.IDN.restriction_profile" is "high", the Highly Restrictive
// profile is used.
// If "network.IDN.restriction_profile" is "moderate", the Moderately
// Restrictive profile is used.
// In all other cases, the ASCII-Only profile is used.
// Note that these preferences are referred to ONLY when
// "network.IDN_show_punycode" is false. In other words, all IDNs will be shown
// in punycode if "network.IDN_show_punycode" is true.
pref("network.IDN.restriction_profile", "high");
pref("network.IDN.use_whitelist", false);

// ccTLDs
pref("network.IDN.whitelist.ac", true);
pref("network.IDN.whitelist.ar", true);
pref("network.IDN.whitelist.at", true);
pref("network.IDN.whitelist.br", true);
pref("network.IDN.whitelist.ca", true);
pref("network.IDN.whitelist.ch", true);
pref("network.IDN.whitelist.cl", true);
pref("network.IDN.whitelist.cn", true);
pref("network.IDN.whitelist.de", true);
pref("network.IDN.whitelist.dk", true);
pref("network.IDN.whitelist.ee", true);
pref("network.IDN.whitelist.es", true);
pref("network.IDN.whitelist.fi", true);
pref("network.IDN.whitelist.fr", true);
pref("network.IDN.whitelist.gr", true);
pref("network.IDN.whitelist.gt", true);
pref("network.IDN.whitelist.hu", true);
pref("network.IDN.whitelist.il", true);
pref("network.IDN.whitelist.io", true);
pref("network.IDN.whitelist.ir", true);
pref("network.IDN.whitelist.is", true);
pref("network.IDN.whitelist.jp", true);
pref("network.IDN.whitelist.kr", true);
pref("network.IDN.whitelist.li", true);
pref("network.IDN.whitelist.lt", true);
pref("network.IDN.whitelist.lu", true);
pref("network.IDN.whitelist.lv", true);
pref("network.IDN.whitelist.no", true);
pref("network.IDN.whitelist.nu", true);
pref("network.IDN.whitelist.nz", true);
pref("network.IDN.whitelist.pl", true);
pref("network.IDN.whitelist.pm", true);
pref("network.IDN.whitelist.pr", true);
pref("network.IDN.whitelist.re", true);
pref("network.IDN.whitelist.se", true);
pref("network.IDN.whitelist.sh", true);
pref("network.IDN.whitelist.si", true);
pref("network.IDN.whitelist.tf", true);
pref("network.IDN.whitelist.th", true);
pref("network.IDN.whitelist.tm", true);
pref("network.IDN.whitelist.tw", true);
pref("network.IDN.whitelist.ua", true);
pref("network.IDN.whitelist.vn", true);
pref("network.IDN.whitelist.wf", true);
pref("network.IDN.whitelist.yt", true);

// IDN ccTLDs
// ae, UAE, .<Emarat>
pref("network.IDN.whitelist.xn--mgbaam7a8h", true);
// cn, China, .<China> with variants
pref("network.IDN.whitelist.xn--fiqz9s", true); // Traditional
pref("network.IDN.whitelist.xn--fiqs8s", true); // Simplified
// eg, Egypt, .<Masr>
pref("network.IDN.whitelist.xn--wgbh1c", true);
// hk, Hong Kong, .<Hong Kong>
pref("network.IDN.whitelist.xn--j6w193g", true);
// ir, Iran, <.Iran> with variants
pref("network.IDN.whitelist.xn--mgba3a4f16a", true);
pref("network.IDN.whitelist.xn--mgba3a4fra", true);
// jo, Jordan, .<Al-Ordon>
pref("network.IDN.whitelist.xn--mgbayh7gpa", true);
// lk, Sri Lanka, .<Lanka> and .<Ilangai>
pref("network.IDN.whitelist.xn--fzc2c9e2c", true);
pref("network.IDN.whitelist.xn--xkc2al3hye2a", true);
// qa, Qatar, .<Qatar>
pref("network.IDN.whitelist.xn--wgbl6a", true);
// rs, Serbia, .<Srb>
pref("network.IDN.whitelist.xn--90a3ac", true);
// ru, Russian Federation, .<RF>
pref("network.IDN.whitelist.xn--p1ai", true);
// sa, Saudi Arabia, .<al-Saudiah> with variants
pref("network.IDN.whitelist.xn--mgberp4a5d4ar", true);
pref("network.IDN.whitelist.xn--mgberp4a5d4a87g", true);
pref("network.IDN.whitelist.xn--mgbqly7c0a67fbc", true);
pref("network.IDN.whitelist.xn--mgbqly7cvafr", true);
// sy, Syria, .<Souria>
pref("network.IDN.whitelist.xn--ogbpf8fl", true);
// th, Thailand, .<Thai>
pref("network.IDN.whitelist.xn--o3cw4h", true);
// tw, Taiwan, <.Taiwan> with variants
pref("network.IDN.whitelist.xn--kpry57d", true);  // Traditional
pref("network.IDN.whitelist.xn--kprw13d", true);  // Simplified

// gTLDs
pref("network.IDN.whitelist.asia", true);
pref("network.IDN.whitelist.biz", true);
pref("network.IDN.whitelist.cat", true);
pref("network.IDN.whitelist.info", true);
pref("network.IDN.whitelist.museum", true);
pref("network.IDN.whitelist.org", true);
pref("network.IDN.whitelist.tel", true);

// NOTE: Before these can be removed, one of bug 414812's tests must be updated
//       or it will likely fail!  Please CC jwalden+bmo on the bug associated
//       with removing these so he can provide a patch to make the necessary
//       changes to avoid bustage.
// ".test" localised TLDs for ICANN's top-level IDN trial
pref("network.IDN.whitelist.xn--0zwm56d", true);
pref("network.IDN.whitelist.xn--11b5bs3a9aj6g", true);
pref("network.IDN.whitelist.xn--80akhbyknj4f", true);
pref("network.IDN.whitelist.xn--9t4b11yi5a", true);
pref("network.IDN.whitelist.xn--deba0ad", true);
pref("network.IDN.whitelist.xn--g6w251d", true);
pref("network.IDN.whitelist.xn--hgbk6aj7f53bba", true);
pref("network.IDN.whitelist.xn--hlcj6aya9esc7a", true);
pref("network.IDN.whitelist.xn--jxalpdlp", true);
pref("network.IDN.whitelist.xn--kgbechtv", true);
pref("network.IDN.whitelist.xn--zckzah", true);

// If a domain includes any of the blocklist characters, it may be a spoof
// attempt and so we always display the domain name as punycode. This would
// override the settings "network.IDN_show_punycode" and
// "network.IDN.whitelist.*".
// For a complete list of the blocked IDN characters see:
//   netwerk/dns/IDNCharacterBlocklist.inc

// This pref may contain characters that will override the hardcoded blocklist,
// so their presence in a domain name will not cause it to be displayed as
// punycode.
// Note that this only removes the characters from the blocklist, but there may
// be other rules in place that cause it to be displayed as punycode.
pref("network.IDN.extra_allowed_chars", "");
// This pref may contain additional blocklist characters
pref("network.IDN.extra_blocked_chars", "");

// This preference specifies a list of domains for which DNS lookups will be
// IPv4 only. Works around broken DNS servers which can't handle IPv6 lookups
// and/or allows the user to disable IPv6 on a per-domain basis. See bug 68796.
pref("network.dns.ipv4OnlyDomains", "");

// This preference can be used to turn off IPv6 name lookups. See bug 68796.
pref("network.dns.disableIPv6", false);

// This is the number of dns cache entries allowed
pref("network.dnsCacheEntries", 400);

// In the absence of OS TTLs, the DNS cache TTL value
pref("network.dnsCacheExpiration", 60);

// Get TTL; not supported on all platforms; nop on the unsupported ones.
pref("network.dns.get-ttl", true);

// For testing purposes! Makes the native resolver resolve IPv4 "localhost"
// instead of the actual given name.
pref("network.dns.native-is-localhost", false);

// The grace period allows the DNS cache to use expired entries, while kicking off
// a revalidation in the background.
pref("network.dnsCacheExpirationGracePeriod", 60);

// This preference can be used to turn off DNS prefetch.
pref("network.dns.disablePrefetch", false);

// This preference controls whether .onion hostnames are
// rejected before being given to DNS. RFC 7686
pref("network.dns.blockDotOnion", true);

// These domains are treated as localhost equivalent
pref("network.dns.localDomains", "");

// When non empty all non-localhost DNS queries (including IP addresses)
// resolve to this value. The value can be a name or an IP address.
// domains mapped to localhost with localDomains stay localhost.
pref("network.dns.forceResolve", "");

// Contols whether or not "localhost" should resolve when offline
pref("network.dns.offline-localhost", true);

// Defines how much longer resolver threads should stay idle before are shut down.
// A negative value will keep the thread alive forever.
pref("network.dns.resolver-thread-extra-idle-time-seconds", 60);

// Idle timeout for ftp control connections - 5 minute default
pref("network.ftp.idleConnectionTimeout", 300);

// enables the prefetch service (i.e., prefetching of <link rel="next"> and
// <link rel="prefetch"> URLs).
pref("network.prefetch-next", true);

// The following prefs pertain to the negotiate-auth extension (see bug 17578),
// which provides transparent Kerberos or NTLM authentication using the SPNEGO
// protocol.  Each pref is a comma-separated list of keys, where each key has
// the format:
//   [scheme "://"] [host [":" port]]
// For example, "foo.com" would match "http://www.foo.com/bar", etc.

// This list controls which URIs can use the negotiate-auth protocol.  This
// list should be limited to the servers you know you'll need to login to.
pref("network.negotiate-auth.trusted-uris", "");
// This list controls which URIs can support delegation.
pref("network.negotiate-auth.delegation-uris", "");

// Do not allow SPNEGO by default when challenged by a local server.
pref("network.negotiate-auth.allow-non-fqdn", false);

// Allow SPNEGO by default when challenged by a proxy server.
pref("network.negotiate-auth.allow-proxies", true);

// Path to a specific gssapi library
pref("network.negotiate-auth.gsslib", "");

// Specify if the gss lib comes standard with the OS
pref("network.negotiate-auth.using-native-gsslib", true);

#ifdef XP_WIN
  // Default to using the SSPI intead of GSSAPI on windows
  pref("network.auth.use-sspi", true);
#endif

// Controls which NTLM authentication implementation we default to. True forces
// the use of our generic (internal) NTLM authentication implementation vs. any
// native implementation provided by the os. This pref is for diagnosing issues
// with native NTLM. (See bug 520607 for details.) Using generic NTLM authentication
// can expose the user to reflection attack vulnerabilities. Do not change this
// unless you know what you're doing!
// This pref should be removed 6 months after the release of firefox 3.6.
pref("network.auth.force-generic-ntlm", false);

// The following prefs are used to enable automatic use of the operating
// system's NTLM implementation to silently authenticate the user with their
// Window's domain logon.  The trusted-uris pref follows the format of the
// trusted-uris pref for negotiate authentication.
pref("network.automatic-ntlm-auth.allow-proxies", true);
pref("network.automatic-ntlm-auth.allow-non-fqdn", false);
pref("network.automatic-ntlm-auth.trusted-uris", "");

// The string to return to the server as the 'workstation' that the
// user is using.  Bug 1046421 notes that the previous default, of the
// system hostname, could be used for user fingerprinting.
//
// However, in some network environments where allowedWorkstations is in use
// to provide a level of host-based access control, it must be set to a string
// that is listed in allowedWorkstations for the user's account in their
// AD Domain.
pref("network.generic-ntlm-auth.workstation", "WORKSTATION");

// This preference controls whether to allow sending default credentials (SSO) to
// NTLM/Negotiate servers allowed in the "trusted uri" list when navigating them
// in a Private Browsing window.
// If set to false, Private Browsing windows will not use default credentials and ask
// for credentials from the user explicitly.
// If set to true, and a server URL conforms other conditions for sending default
// credentials, those will be sent automatically in Private Browsing windows.
//
// This preference has no effect when the browser is set to "Never Remember History",
// in that case default credentials will always be used.
pref("network.auth.private-browsing-sso", false);

// Control how throttling of http responses works - number of ms that each
// suspend and resume period lasts (prefs named appropriately)
// This feature is occasionally causing visible regressions (download too slow for
// too long time, jitter in video/audio in background tabs...)
pref("network.http.throttle.enable", false);

// Make HTTP throttling v2 algorithm Nightly-only due to bug 1462906
#ifdef NIGHTLY_BUILD
  pref("network.http.throttle.version", 2);
#else
  pref("network.http.throttle.version", 1);
#endif

// V1 prefs
pref("network.http.throttle.suspend-for", 900);
pref("network.http.throttle.resume-for", 100);

// V2 prefs
pref("network.http.throttle.read-limit-bytes", 8000);
pref("network.http.throttle.read-interval-ms", 500);

// Common prefs
// Delay we resume throttled background responses after the last unthrottled
// response has finished.  Prevents resuming too soon during an active page load
// at which sub-resource reqeusts quickly come and go.
pref("network.http.throttle.hold-time-ms", 800);
// After the last transaction activation or last data chunk response we only
// throttle for this period of time.  This prevents comet and unresponsive
// http requests to engage long-standing throttling.
pref("network.http.throttle.max-time-ms", 500);

// Give higher priority to requests resulting from a user interaction event
// like click-to-play, image fancy-box zoom, navigation.
pref("network.http.on_click_priority", true);

// When the page load has not yet reached DOMContentLoaded point, tail requestes are delayed
// by (non-tailed requests count + 1) * delay-quantum milliseconds.
pref("network.http.tailing.delay-quantum", 600);
// The same as above, but applied after the document load reached DOMContentLoaded event.
pref("network.http.tailing.delay-quantum-after-domcontentloaded", 100);
// Upper limit for the calculated delay, prevents long standing and comet-like requests
// tail forever.  This is in milliseconds as well.
pref("network.http.tailing.delay-max", 6000);
// Total limit we delay tailed requests since a page load beginning.
pref("network.http.tailing.total-max", 45000);

pref("network.http.spdy.bug1563695", true);
pref("network.http.spdy.bug1556491", true);

pref("permissions.default.image",           1); // 1-Accept, 2-Deny, 3-dontAcceptForeign

pref("network.proxy.type",                  5);
pref("network.proxy.ftp",                   "");
pref("network.proxy.ftp_port",              0);
pref("network.proxy.http",                  "");
pref("network.proxy.http_port",             0);
pref("network.proxy.ssl",                   "");
pref("network.proxy.ssl_port",              0);
pref("network.proxy.socks",                 "");
pref("network.proxy.socks_port",            0);
pref("network.proxy.socks_version",         5);
pref("network.proxy.proxy_over_tls",        true);
pref("network.proxy.no_proxies_on",         "");
// Set true to allow resolving proxy for localhost
pref("network.proxy.allow_hijacking_localhost", false);
pref("network.proxy.failover_timeout",      1800); // 30 minutes
pref("network.online",                      true); //online/offline

// The interval in seconds to move the cookies in the child process.
// Set to 0 to disable moving the cookies.
pref("network.cookie.move.interval_sec",    10);

pref("network.cookie.maxNumber", 3000);
pref("network.cookie.maxPerHost", 180);
// Cookies quota for each host. If cookies exceed the limit maxPerHost,
// (maxPerHost - quotaPerHost) cookies will be evicted.
pref("network.cookie.quotaPerHost", 150);

// The PAC file to load.  Ignored unless network.proxy.type is 2.
pref("network.proxy.autoconfig_url", "");
// Strip off paths when sending URLs to PAC scripts
pref("network.proxy.autoconfig_url.include_path", false);

// If we cannot load the PAC file, then try again (doubling from interval_min
// until we reach interval_max or the PAC file is successfully loaded).
pref("network.proxy.autoconfig_retry_interval_min", 5);    // 5 seconds
pref("network.proxy.autoconfig_retry_interval_max", 300);  // 5 minutes
pref("network.proxy.enable_wpad_over_dhcp", true);

// Use the HSTS preload list by default
pref("network.stricttransportsecurity.preloadlist", true);

// Use JS mDNS as a fallback
pref("network.mdns.use_js_fallback", false);

// Cache SSL resumption tokens in necko
pref("network.ssl_tokens_cache_enabled", false);
// Capacity of the cache in kilobytes
pref("network.ssl_tokens_cache_capacity", 2048);

pref("converter.html2txt.structs",          true); // Output structured phrases (strong, em, code, sub, sup, b, i, u)
pref("converter.html2txt.header_strategy",  1); // 0 = no indention; 1 = indention, increased with header level; 2 = numbering and slight indention

pref("intl.accept_languages",               "chrome://global/locale/intl.properties");
pref("intl.menuitems.alwaysappendaccesskeys","chrome://global/locale/intl.properties");
pref("intl.menuitems.insertseparatorbeforeaccesskeys","chrome://global/locale/intl.properties");
pref("intl.charset.detector",               "chrome://global/locale/intl.properties");
pref("intl.charset.fallback.override",      "");
pref("intl.ellipsis",                       "chrome://global-platform/locale/intl.properties");
// this pref allows user to request that all internationalization formatters
// like date/time formatting, unit formatting, calendars etc. should use
// OS locale set instead of the app locale set.
pref("intl.regional_prefs.use_os_locales",  false);
// fallback charset list for Unicode conversion (converting from Unicode)
// currently used for mail send only to handle symbol characters (e.g Euro, trademark, smartquotes)
// for ISO-8859-1
pref("intl.fallbackCharsetList.ISO-8859-1", "windows-1252");
pref("font.language.group",                 "chrome://global/locale/intl.properties");

// If you use legacy Chinese IME which puts an ideographic space to composition
// string as placeholder, this pref might be useful.  If this is true and when
// web contents forcibly commits composition (e.g., moving focus), the
// ideographic space will be ignored (i.e., commits with empty string).
pref("intl.ime.remove_placeholder_character_at_commit", false);

pref("intl.uidirection", -1); // -1 to set from locale; 0 for LTR; 1 for RTL

// use en-US hyphenation by default for content tagged with plain lang="en"
pref("intl.hyphenation-alias.en", "en-us");
// and for other subtags of en-*, if no specific patterns are available
pref("intl.hyphenation-alias.en-*", "en-us");

pref("intl.hyphenation-alias.af-*", "af");
pref("intl.hyphenation-alias.bg-*", "bg");
pref("intl.hyphenation-alias.ca-*", "ca");
pref("intl.hyphenation-alias.cy-*", "cy");
pref("intl.hyphenation-alias.da-*", "da");
pref("intl.hyphenation-alias.eo-*", "eo");
pref("intl.hyphenation-alias.es-*", "es");
pref("intl.hyphenation-alias.et-*", "et");
pref("intl.hyphenation-alias.fi-*", "fi");
pref("intl.hyphenation-alias.fr-*", "fr");
pref("intl.hyphenation-alias.gl-*", "gl");
pref("intl.hyphenation-alias.hr-*", "hr");
pref("intl.hyphenation-alias.hsb-*", "hsb");
pref("intl.hyphenation-alias.hu-*", "hu");
pref("intl.hyphenation-alias.ia-*", "ia");
pref("intl.hyphenation-alias.is-*", "is");
pref("intl.hyphenation-alias.it-*", "it");
pref("intl.hyphenation-alias.kmr-*", "kmr");
pref("intl.hyphenation-alias.la-*", "la");
pref("intl.hyphenation-alias.lt-*", "lt");
pref("intl.hyphenation-alias.mn-*", "mn");
pref("intl.hyphenation-alias.nl-*", "nl");
pref("intl.hyphenation-alias.pl-*", "pl");
pref("intl.hyphenation-alias.pt-*", "pt");
pref("intl.hyphenation-alias.ru-*", "ru");
pref("intl.hyphenation-alias.sl-*", "sl");
pref("intl.hyphenation-alias.sv-*", "sv");
pref("intl.hyphenation-alias.tr-*", "tr");
pref("intl.hyphenation-alias.uk-*", "uk");

// use reformed (1996) German patterns by default unless specifically tagged as de-1901
// (these prefs may soon be obsoleted by better BCP47-based tag matching, but for now...)
pref("intl.hyphenation-alias.de", "de-1996");
pref("intl.hyphenation-alias.de-*", "de-1996");
pref("intl.hyphenation-alias.de-AT-1901", "de-1901");
pref("intl.hyphenation-alias.de-DE-1901", "de-1901");
pref("intl.hyphenation-alias.de-CH-*", "de-CH");

// patterns from TeX are tagged as "sh" (Serbo-Croatian) macrolanguage;
// alias "sr" (Serbian) and "bs" (Bosnian) to those patterns
// (Croatian has its own separate patterns).
pref("intl.hyphenation-alias.sr", "sh");
pref("intl.hyphenation-alias.bs", "sh");
pref("intl.hyphenation-alias.sh-*", "sh");
pref("intl.hyphenation-alias.sr-*", "sh");
pref("intl.hyphenation-alias.bs-*", "sh");

// Norwegian has two forms, Bokmål and Nynorsk, with "no" as a macrolanguage encompassing both.
// For "no", we'll alias to "nb" (Bokmål) as that is the more widely used written form.
pref("intl.hyphenation-alias.no", "nb");
pref("intl.hyphenation-alias.no-*", "nb");
pref("intl.hyphenation-alias.nb-*", "nb");
pref("intl.hyphenation-alias.nn-*", "nn");

// In German, we allow hyphenation of capitalized words; otherwise not.
pref("intl.hyphenate-capitalized.de-1996", true);
pref("intl.hyphenate-capitalized.de-1901", true);
pref("intl.hyphenate-capitalized.de-CH", true);

// All prefs of default font should be "auto".
pref("font.name.serif.ar", "");
pref("font.name.sans-serif.ar", "");
pref("font.name.monospace.ar", "");
pref("font.name.cursive.ar", "");

pref("font.name.serif.el", "");
pref("font.name.sans-serif.el", "");
pref("font.name.monospace.el", "");
pref("font.name.cursive.el", "");

pref("font.name.serif.he", "");
pref("font.name.sans-serif.he", "");
pref("font.name.monospace.he", "");
pref("font.name.cursive.he", "");

pref("font.name.serif.ja", "");
pref("font.name.sans-serif.ja", "");
pref("font.name.monospace.ja", "");
pref("font.name.cursive.ja", "");

pref("font.name.serif.ko", "");
pref("font.name.sans-serif.ko", "");
pref("font.name.monospace.ko", "");
pref("font.name.cursive.ko", "");

pref("font.name.serif.th", "");
pref("font.name.sans-serif.th", "");
pref("font.name.monospace.th", "");
pref("font.name.cursive.th", "");

pref("font.name.serif.x-cyrillic", "");
pref("font.name.sans-serif.x-cyrillic", "");
pref("font.name.monospace.x-cyrillic", "");
pref("font.name.cursive.x-cyrillic", "");

pref("font.name.serif.x-unicode", "");
pref("font.name.sans-serif.x-unicode", "");
pref("font.name.monospace.x-unicode", "");
pref("font.name.cursive.x-unicode", "");

pref("font.name.serif.x-western", "");
pref("font.name.sans-serif.x-western", "");
pref("font.name.monospace.x-western", "");
pref("font.name.cursive.x-western", "");

pref("font.name.serif.zh-CN", "");
pref("font.name.sans-serif.zh-CN", "");
pref("font.name.monospace.zh-CN", "");
pref("font.name.cursive.zh-CN", "");

pref("font.name.serif.zh-TW", "");
pref("font.name.sans-serif.zh-TW", "");
pref("font.name.monospace.zh-TW", "");
pref("font.name.cursive.zh-TW", "");

pref("font.name.serif.zh-HK", "");
pref("font.name.sans-serif.zh-HK", "");
pref("font.name.monospace.zh-HK", "");
pref("font.name.cursive.zh-HK", "");

pref("font.name.serif.x-devanagari", "");
pref("font.name.sans-serif.x-devanagari", "");
pref("font.name.monospace.x-devanagari", "");
pref("font.name.cursive.x-devanagari", "");

pref("font.name.serif.x-tamil", "");
pref("font.name.sans-serif.x-tamil", "");
pref("font.name.monospace.x-tamil", "");
pref("font.name.cursive.x-tamil", "");

pref("font.name.serif.x-armn", "");
pref("font.name.sans-serif.x-armn", "");
pref("font.name.monospace.x-armn", "");
pref("font.name.cursive.x-armn", "");

pref("font.name.serif.x-beng", "");
pref("font.name.sans-serif.x-beng", "");
pref("font.name.monospace.x-beng", "");
pref("font.name.cursive.x-beng", "");

pref("font.name.serif.x-cans", "");
pref("font.name.sans-serif.x-cans", "");
pref("font.name.monospace.x-cans", "");
pref("font.name.cursive.x-cans", "");

pref("font.name.serif.x-ethi", "");
pref("font.name.sans-serif.x-ethi", "");
pref("font.name.monospace.x-ethi", "");
pref("font.name.cursive.x-ethi", "");

pref("font.name.serif.x-geor", "");
pref("font.name.sans-serif.x-geor", "");
pref("font.name.monospace.x-geor", "");
pref("font.name.cursive.x-geor", "");

pref("font.name.serif.x-gujr", "");
pref("font.name.sans-serif.x-gujr", "");
pref("font.name.monospace.x-gujr", "");
pref("font.name.cursive.x-gujr", "");

pref("font.name.serif.x-guru", "");
pref("font.name.sans-serif.x-guru", "");
pref("font.name.monospace.x-guru", "");
pref("font.name.cursive.x-guru", "");

pref("font.name.serif.x-khmr", "");
pref("font.name.sans-serif.x-khmr", "");
pref("font.name.monospace.x-khmr", "");
pref("font.name.cursive.x-khmr", "");

pref("font.name.serif.x-mlym", "");
pref("font.name.sans-serif.x-mlym", "");
pref("font.name.monospace.x-mlym", "");
pref("font.name.cursive.x-mlym", "");

pref("font.name.serif.x-orya", "");
pref("font.name.sans-serif.x-orya", "");
pref("font.name.monospace.x-orya", "");
pref("font.name.cursive.x-orya", "");

pref("font.name.serif.x-telu", "");
pref("font.name.sans-serif.x-telu", "");
pref("font.name.monospace.x-telu", "");
pref("font.name.cursive.x-telu", "");

pref("font.name.serif.x-knda", "");
pref("font.name.sans-serif.x-knda", "");
pref("font.name.monospace.x-knda", "");
pref("font.name.cursive.x-knda", "");

pref("font.name.serif.x-sinh", "");
pref("font.name.sans-serif.x-sinh", "");
pref("font.name.monospace.x-sinh", "");
pref("font.name.cursive.x-sinh", "");

pref("font.name.serif.x-tibt", "");
pref("font.name.sans-serif.x-tibt", "");
pref("font.name.monospace.x-tibt", "");
pref("font.name.cursive.x-tibt", "");

pref("font.name.serif.x-math", "");
pref("font.name.sans-serif.x-math", "");
pref("font.name.monospace.x-math", "");
pref("font.name.cursive.x-math", "");

pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, serif");
pref("font.name-list.sans-serif.x-math", "sans-serif");
pref("font.name-list.monospace.x-math", "monospace");

// Some CJK fonts have bad underline offset, their CJK character glyphs are overlapped (or adjoined)  to its underline.
// These fonts are ignored the underline offset, instead of it, the underline is lowered to bottom of its em descent.
pref("font.blacklist.underline_offset", "FangSong,Gulim,GulimChe,MingLiU,MingLiU-ExtB,MingLiU_HKSCS,MingLiU-HKSCS-ExtB,MS Gothic,MS Mincho,MS PGothic,MS PMincho,MS UI Gothic,PMingLiU,PMingLiU-ExtB,SimHei,SimSun,SimSun-ExtB,Hei,Kai,Apple LiGothic,Apple LiSung,Osaka");

pref("security.directory",              "");

// security-sensitive dialogs should delay button enabling. In milliseconds.
pref("security.dialog_enable_delay", 1000);
pref("security.notification_enable_delay", 500);

#if defined(DEBUG)
  // For testing purposes only: Flipping this pref to true allows
  // to skip the assertion that every about page ships with a CSP.
  pref("csp.skip_about_page_has_csp_assert", false);
  // For testing purposes only: Flipping this pref to true allows
  // to skip the allowlist for about: pages and do not ship with a
  // CSP and NS_ASSERT right away.
  pref("csp.skip_about_page_csp_allowlist_and_assert", false);
  // For testing purposes only: Flipping this pref to true allows
  // to skip the assertion that HTML fragments (e.g. innerHTML) can
  // not be used within chrome code or about: pages.
  pref("domsecurity.skip_html_fragment_assertion", false);
  // For testing purposes only; Flipping this pref to true allows
  // to skip the assertion that remote scripts can not be loaded
  // in system privileged contexts.
  pref("domsecurity.skip_remote_script_assertion_in_system_priv_context", false);
#endif

#ifdef EARLY_BETA_OR_EARLIER
  // Disallow web documents loaded with the SystemPrincipal
  pref("security.disallow_non_local_systemprincipal_in_tests", false);
#endif

// Sub-resource integrity
pref("security.sri.enable", true);

// OCSP must-staple
pref("security.ssl.enable_ocsp_must_staple", true);

// Insecure Form Field Warning
pref("security.insecure_field_warning.contextual.enabled", false);
pref("security.insecure_field_warning.ignore_local_ip_address", true);

// Disable pinning checks by default.
pref("security.cert_pinning.enforcement_level", 0);
// Do not process hpkp headers rooted by not built in roots by default.
// This is to prevent accidental pinning from MITM devices and is used
// for tests.
pref("security.cert_pinning.process_headers_from_non_builtin_roots", false);

// Controls whether or not HPKP (the HTTP Public Key Pinning header) is enabled.
// If true, the header is processed and collected HPKP information is consulted
// when looking for pinning information.
// If false, the header is not processed and collected HPKP information is not
// consulted when looking for pinning information. Preloaded pins are not
// affected by this preference.
// Default: false
pref("security.cert_pinning.hpkp.enabled", false);

// The percentage of clients who will report uptake telemetry as
// events instead of just a histogram. This only applies on Release;
// other channels always report events.
pref("services.common.uptake.sampleRate", 1);   // 1%

// Security state OneCRL.
pref("services.settings.security.onecrl.bucket", "security-state");
pref("services.settings.security.onecrl.collection", "onecrl");
pref("services.settings.security.onecrl.signer", "onecrl.content-signature.mozilla.org");
pref("services.settings.security.onecrl.checked", 0);

// Blocklist preferences
pref("extensions.blocklist.enabled", false);
// OneCRL freshness checking depends on this value, so if you change it,
// please also update security.onecrl.maximum_staleness_in_seconds.
pref("extensions.blocklist.interval", 86400);
// Required blocklist freshness for OneCRL OCSP bypass
// (default is 1.25x extensions.blocklist.interval, or 30 hours)
pref("security.onecrl.maximum_staleness_in_seconds", 108000);
pref("extensions.blocklist.url", "data:text/plain,");
pref("extensions.blocklist.detailsURL", "data:text/plain,");
pref("extensions.blocklist.itemURL", "data:text/plain,");
// Controls what level the blocklist switches from warning about items to forcibly
// blocking them.
pref("extensions.blocklist.level", 2);
// Blocklist via settings server (Kinto)
pref("services.blocklist.bucket", "blocklists");
pref("services.blocklist.addons.collection", "addons");
pref("services.blocklist.addons.checked", 0);
pref("services.blocklist.addons.signer", "remote-settings.content-signature.mozilla.org");
pref("services.blocklist.plugins.collection", "plugins");
pref("services.blocklist.plugins.checked", 0);
pref("services.blocklist.plugins.signer", "remote-settings.content-signature.mozilla.org");
pref("services.blocklist.pinning.enabled", true);
pref("services.blocklist.pinning.bucket", "pinning");
pref("services.blocklist.pinning.collection", "pins");
pref("services.blocklist.pinning.checked", 0);
pref("services.blocklist.pinning.signer", "pinning-preload.content-signature.mozilla.org");
pref("services.blocklist.gfx.collection", "gfx");
pref("services.blocklist.gfx.checked", 0);
pref("services.blocklist.gfx.signer", "remote-settings.content-signature.mozilla.org");

// Modifier key prefs: default to Windows settings,
// menu access key = alt, accelerator key = control.
// Use 17 for Ctrl, 18 for Alt, 224 for Meta, 91 for Win, 0 for none. Mac settings in macprefs.js
pref("ui.key.accelKey", 17);
pref("ui.key.menuAccessKey", 18);

// Middle-mouse handling
pref("middlemouse.paste", false);
pref("middlemouse.contentLoadURL", false);
pref("middlemouse.scrollbarPosition", false);

// Clipboard only supports text/plain
pref("clipboard.plainTextOnly", false);

#ifdef XP_WIN
  // Setting false you can disable 4th button and/or 5th button of your mouse.
  // 4th button is typically mapped to "Back" and 5th button is typically mapped
  // to "Forward" button.
  pref("mousebutton.4th.enabled", true);
  pref("mousebutton.5th.enabled", true);
#endif

// mousewheel.*.action can specify the action when you use mosue wheel.
// When no modifier keys are pressed or two or more modifires are pressed,
// .default is used.
// 0: Nothing happens
// 1: Scrolling contents
// 2: Go back or go forward, in your history
// 3: Zoom in or out (reflowing zoom).
// 4: Treat vertical wheel as horizontal scroll
//      This treats vertical wheel operation (i.e., deltaY) as horizontal
//      scroll.  deltaX and deltaZ are always ignored.  So, only
//      "delta_multiplier_y" pref affects the scroll speed.
// 5: Zoom in or out (pinch zoom).
pref("mousewheel.default.action", 1);
pref("mousewheel.with_alt.action", 2);
pref("mousewheel.with_control.action", 3);
pref("mousewheel.with_meta.action", 1);  // command key on Mac
pref("mousewheel.with_shift.action", 4);
pref("mousewheel.with_win.action", 1);

// mousewheel.*.action.override_x will override the action
// when the mouse wheel is rotated along the x direction.
// -1: Don't override the action.
// 0 to 3: Override the action with the specified value.
// Note that 4 isn't available because it doesn't make sense to apply the
// default action only for y direction to this pref.
pref("mousewheel.default.action.override_x", -1);
pref("mousewheel.with_alt.action.override_x", -1);
pref("mousewheel.with_control.action.override_x", -1);
pref("mousewheel.with_meta.action.override_x", -1);  // command key on Mac
pref("mousewheel.with_shift.action.override_x", -1);
pref("mousewheel.with_win.action.override_x", -1);

// mousewheel.*.delta_multiplier_* can specify the value muliplied by the delta
// value.  The values will be used after divided by 100.  I.e., 100 means 1.0,
// -100 means -1.0.  If the values were negative, the direction would be
// reverted.  The absolue value must be 100 or larger.
pref("mousewheel.default.delta_multiplier_x", 100);
pref("mousewheel.default.delta_multiplier_y", 100);
pref("mousewheel.default.delta_multiplier_z", 100);
pref("mousewheel.with_alt.delta_multiplier_x", 100);
pref("mousewheel.with_alt.delta_multiplier_y", 100);
pref("mousewheel.with_alt.delta_multiplier_z", 100);
pref("mousewheel.with_control.delta_multiplier_x", 100);
pref("mousewheel.with_control.delta_multiplier_y", 100);
pref("mousewheel.with_control.delta_multiplier_z", 100);
pref("mousewheel.with_meta.delta_multiplier_x", 100);  // command key on Mac
pref("mousewheel.with_meta.delta_multiplier_y", 100);  // command key on Mac
pref("mousewheel.with_meta.delta_multiplier_z", 100);  // command key on Mac
pref("mousewheel.with_shift.delta_multiplier_x", 100);
pref("mousewheel.with_shift.delta_multiplier_y", 100);
pref("mousewheel.with_shift.delta_multiplier_z", 100);
pref("mousewheel.with_win.delta_multiplier_x", 100);
pref("mousewheel.with_win.delta_multiplier_y", 100);
pref("mousewheel.with_win.delta_multiplier_z", 100);

// We can show it anytime from menus
pref("profile.manage_only_at_launch", false);

// ------------------
//  Text Direction
// ------------------
// 1 = directionLTRBidi *
// 2 = directionRTLBidi
pref("bidi.direction", 1);
// ------------------
//  Text Type
// ------------------
// 1 = charsettexttypeBidi *
// 2 = logicaltexttypeBidi
// 3 = visualtexttypeBidi
pref("bidi.texttype", 1);
// ------------------
//  Numeral Style
// ------------------
// 0 = nominalnumeralBidi *
// 1 = regularcontextnumeralBidi
// 2 = hindicontextnumeralBidi
// 3 = arabicnumeralBidi
// 4 = hindinumeralBidi
// 5 = persiancontextnumeralBidi
// 6 = persiannumeralBidi
pref("bidi.numeral", 0);

// Setting this pref to |true| forces Bidi UI menu items and keyboard shortcuts
// to be exposed, and enables the directional caret hook. By default, only
// expose it for bidi-associated system locales.
pref("bidi.browser.ui", false);

// Override DPI. A value of -1 means use the maximum of 96 and the system DPI.
// A value of 0 means use the system DPI. A positive value is used as the DPI.
// This sets the physical size of a device pixel and thus controls the
// interpretation of physical units such as "pt".
pref("layout.css.dpi", -1);

// pref for which side vertical scrollbars should be on
// 0 = end-side in UI direction
// 1 = end-side in document/content direction
// 2 = right
// 3 = left
pref("layout.scrollbar.side", 0);

// pref to control whether layout warnings that are hit quite often are enabled
pref("layout.spammy_warnings.enabled", false);

// if true, allow plug-ins to override internal imglib decoder mime types in full-page mode
pref("plugin.override_internal_types", false);

// enable single finger gesture input (win7+ tablets)
pref("gestures.enable_single_finger_input", true);

pref("editor.resizing.preserve_ratio",       true);
pref("editor.positioning.offset",            0);

pref("dom.use_watchdog", false);
pref("dom.max_chrome_script_run_time", 20);
pref("dom.max_script_run_time", 10);
pref("dom.max_ext_content_script_run_time", 5);

// Stop all scripts in a compartment when the "stop script" dialog is used.
pref("dom.global_stop_script", true);

// Support the input event queue on the main thread of content process
pref("input_event_queue.supported", true);

// The maximum and minimum time (milliseconds) we reserve for handling input
// events in each frame.
pref("input_event_queue.duration.max", 8);
pref("input_event_queue.duration.min", 1);

// The default amount of time (milliseconds) required for handling a input
// event.
pref("input_event_queue.default_duration_per_event", 1);

// The number of processed input events we use to predict the amount of time
// required to process the following input events.
pref("input_event_queue.count_for_prediction", 9);

pref("plugins.load_appdir_plugins", false);
// If true, plugins will be click to play
pref("plugins.click_to_play", false);

// This only supports one hidden ctp plugin, edit nsPluginArray.cpp if adding a second
pref("plugins.navigator.hidden_ctp_plugin", "");

// The default value for nsIPluginTag.enabledState (STATE_ENABLED = 2)
pref("plugin.default.state", 2);

// This pref can take 3 possible string values:
// "always"     - always use favor fallback mode
// "follow-ctp" - activate if ctp is active for the given
//                plugin object (could be due to a plugin-wide
//                setting or a site-specific setting)
// "never"      - never use favor fallback mode
pref("plugins.favorfallback.mode", "never");

// A comma-separated list of rules to follow when deciding
// whether an object has been provided with good fallback content.
// The valid values can be found at nsObjectLoadingContent::HasGoodFallback.
pref("plugins.favorfallback.rules", "");


// Set IPC timeouts for plugins and tabs, except in leak-checking and
// dynamic analysis builds.  (NS_FREE_PERMANENT_DATA is C++ only, so
// approximate its definition here.)
#if !defined(DEBUG) && !defined(MOZ_ASAN) && !defined(MOZ_VALGRIND) && !defined(MOZ_TSAN)
  // How long a plugin is allowed to process a synchronous IPC message
  // before we consider it "hung".
  pref("dom.ipc.plugins.timeoutSecs", 45);
  // How long a plugin process will wait for a response from the parent
  // to a synchronous request before terminating itself. After this
  // point the child assumes the parent is hung. Currently disabled.
  pref("dom.ipc.plugins.parentTimeoutSecs", 0);
  // How long a plugin in e10s is allowed to process a synchronous IPC
  // message before we notify the chrome process of a hang.
  pref("dom.ipc.plugins.contentTimeoutSecs", 10);
  // How long a plugin launch is allowed to take before
  // we consider it failed.
  pref("dom.ipc.plugins.processLaunchTimeoutSecs", 45);
  #ifdef XP_WIN
    // How long a plugin is allowed to process a synchronous IPC message
    // before we display the plugin hang UI
    pref("dom.ipc.plugins.hangUITimeoutSecs", 11);
    // Minimum time that the plugin hang UI will be displayed
    pref("dom.ipc.plugins.hangUIMinDisplaySecs", 10);
  #endif
#else
  // No timeout in leak-checking builds
  pref("dom.ipc.plugins.timeoutSecs", 0);
  pref("dom.ipc.plugins.contentTimeoutSecs", 0);
  pref("dom.ipc.plugins.processLaunchTimeoutSecs", 0);
  pref("dom.ipc.plugins.parentTimeoutSecs", 0);
  #ifdef XP_WIN
    pref("dom.ipc.plugins.hangUITimeoutSecs", 0);
    pref("dom.ipc.plugins.hangUIMinDisplaySecs", 0);
  #endif
#endif

// Whether or not to collect a paired minidump when force-killing a
// content process.
#ifdef RELEASE_OR_BETA
  pref("dom.ipc.tabs.createKillHardCrashReports", false);
#else
  pref("dom.ipc.tabs.createKillHardCrashReports", true);
#endif

pref("dom.ipc.plugins.flash.subprocess.crashreporter.enabled", true);
pref("dom.ipc.plugins.reportCrashURL", true);

// How long we wait before unloading an idle plugin process.
// Defaults to 30 seconds.
pref("dom.ipc.plugins.unloadTimeoutSecs", 30);

// Force the accelerated direct path for a subset of Flash wmode values
pref("dom.ipc.plugins.forcedirect.enabled", true);

pref("dom.ipc.processCount", 3);

// Default to allow only one file:// URL content process.
pref("dom.ipc.processCount.file", 1);

// WebExtensions only support a single extension process.
pref("dom.ipc.processCount.extension", 1);

// Privileged content only supports a single content process.
pref("dom.ipc.processCount.privileged", 1);

// Keep a single privileged content process alive for performance reasons.
// e.g. we do not want to throw content processes out every time we navigate
// away from about:newtab.
pref("dom.ipc.keepProcessesAlive.privileged", 1);

// Disable support for SVG
pref("svg.disabled", false);

// Override default dom.ipc.processCount for some remote content process types.
pref("dom.ipc.processCount.webLargeAllocation", 10);

// Enable the Large-Allocation header
pref("dom.largeAllocationHeader.enabled", true);

// Disable e10s for Gecko by default. This is overridden in firefox.js.
pref("browser.tabs.remote.autostart", false);

// Disable fission for Gecko by default. Lock it on release and beta because
// it is not ready for use and can leak URIs to telemetry until bug 1561653 is
// fixed.
#ifdef RELEASE_OR_BETA
  pref("fission.autostart", false, locked);
#else
  pref("fission.autostart", false);
#endif

// Pref to control whether we use separate content processes for top-level load
// of file:// URIs.
pref("browser.tabs.remote.separateFileUriProcess", true);

// Pref that enables top level web content pages that are opened from file://
// URI pages to run in the file content process.
// This has been added in case breaking any window references between these
// sorts of pages, which we have to do when we run them in the normal web
// content process, causes compatibility issues.
pref("browser.tabs.remote.allowLinkedWebInFileUriProcess", true);

// Pref to control whether we use separate privileged content processes.
pref("browser.tabs.remote.separatePrivilegedContentProcess", false);

// When this pref is enabled top level loads with a mismatched
// Cross-Origin-Opener-Policy header will be loaded in a separate process.
pref("browser.tabs.remote.useCrossOriginOpenerPolicy", false);

// Default font types and sizes by locale
pref("font.default.ar", "sans-serif");
pref("font.minimum-size.ar", 0);
pref("font.size.variable.ar", 16);
pref("font.size.monospace.ar", 13);

pref("font.default.el", "serif");
pref("font.minimum-size.el", 0);
pref("font.size.variable.el", 16);
pref("font.size.monospace.el", 13);

pref("font.default.he", "sans-serif");
pref("font.minimum-size.he", 0);
pref("font.size.variable.he", 16);
pref("font.size.monospace.he", 13);

pref("font.default.ja", "sans-serif");
pref("font.minimum-size.ja", 0);
pref("font.size.variable.ja", 16);
pref("font.size.monospace.ja", 16);

pref("font.default.ko", "sans-serif");
pref("font.minimum-size.ko", 0);
pref("font.size.variable.ko", 16);
pref("font.size.monospace.ko", 16);

pref("font.default.th", "sans-serif");
pref("font.minimum-size.th", 0);
pref("font.size.variable.th", 16);
pref("font.size.monospace.th", 13);

pref("font.default.x-cyrillic", "serif");
pref("font.minimum-size.x-cyrillic", 0);
pref("font.size.variable.x-cyrillic", 16);
pref("font.size.monospace.x-cyrillic", 13);

pref("font.default.x-devanagari", "serif");
pref("font.minimum-size.x-devanagari", 0);
pref("font.size.variable.x-devanagari", 16);
pref("font.size.monospace.x-devanagari", 13);

pref("font.default.x-tamil", "serif");
pref("font.minimum-size.x-tamil", 0);
pref("font.size.variable.x-tamil", 16);
pref("font.size.monospace.x-tamil", 13);

pref("font.default.x-armn", "serif");
pref("font.minimum-size.x-armn", 0);
pref("font.size.variable.x-armn", 16);
pref("font.size.monospace.x-armn", 13);

pref("font.default.x-beng", "serif");
pref("font.minimum-size.x-beng", 0);
pref("font.size.variable.x-beng", 16);
pref("font.size.monospace.x-beng", 13);

pref("font.default.x-cans", "serif");
pref("font.minimum-size.x-cans", 0);
pref("font.size.variable.x-cans", 16);
pref("font.size.monospace.x-cans", 13);

pref("font.default.x-ethi", "serif");
pref("font.minimum-size.x-ethi", 0);
pref("font.size.variable.x-ethi", 16);
pref("font.size.monospace.x-ethi", 13);

pref("font.default.x-geor", "serif");
pref("font.minimum-size.x-geor", 0);
pref("font.size.variable.x-geor", 16);
pref("font.size.monospace.x-geor", 13);

pref("font.default.x-gujr", "serif");
pref("font.minimum-size.x-gujr", 0);
pref("font.size.variable.x-gujr", 16);
pref("font.size.monospace.x-gujr", 13);

pref("font.default.x-guru", "serif");
pref("font.minimum-size.x-guru", 0);
pref("font.size.variable.x-guru", 16);
pref("font.size.monospace.x-guru", 13);

pref("font.default.x-khmr", "serif");
pref("font.minimum-size.x-khmr", 0);
pref("font.size.variable.x-khmr", 16);
pref("font.size.monospace.x-khmr", 13);

pref("font.default.x-mlym", "serif");
pref("font.minimum-size.x-mlym", 0);
pref("font.size.variable.x-mlym", 16);
pref("font.size.monospace.x-mlym", 13);

pref("font.default.x-orya", "serif");
pref("font.minimum-size.x-orya", 0);
pref("font.size.variable.x-orya", 16);
pref("font.size.monospace.x-orya", 13);

pref("font.default.x-telu", "serif");
pref("font.minimum-size.x-telu", 0);
pref("font.size.variable.x-telu", 16);
pref("font.size.monospace.x-telu", 13);

pref("font.default.x-knda", "serif");
pref("font.minimum-size.x-knda", 0);
pref("font.size.variable.x-knda", 16);
pref("font.size.monospace.x-knda", 13);

pref("font.default.x-sinh", "serif");
pref("font.minimum-size.x-sinh", 0);
pref("font.size.variable.x-sinh", 16);
pref("font.size.monospace.x-sinh", 13);

pref("font.default.x-tibt", "serif");
pref("font.minimum-size.x-tibt", 0);
pref("font.size.variable.x-tibt", 16);
pref("font.size.monospace.x-tibt", 13);

pref("font.default.x-unicode", "serif");
pref("font.minimum-size.x-unicode", 0);
pref("font.size.variable.x-unicode", 16);
pref("font.size.monospace.x-unicode", 13);

pref("font.default.x-western", "serif");
pref("font.minimum-size.x-western", 0);
pref("font.size.variable.x-western", 16);
pref("font.size.monospace.x-western", 13);

pref("font.default.zh-CN", "sans-serif");
pref("font.minimum-size.zh-CN", 0);
pref("font.size.variable.zh-CN", 16);
pref("font.size.monospace.zh-CN", 16);

pref("font.default.zh-HK", "sans-serif");
pref("font.minimum-size.zh-HK", 0);
pref("font.size.variable.zh-HK", 16);
pref("font.size.monospace.zh-HK", 16);

pref("font.default.zh-TW", "sans-serif");
pref("font.minimum-size.zh-TW", 0);
pref("font.size.variable.zh-TW", 16);
pref("font.size.monospace.zh-TW", 16);

// mathml.css sets font-size to "inherit" and font-family to "serif" so only
// font.name.*.x-math and font.minimum-size.x-math are really relevant.
pref("font.default.x-math", "serif");
pref("font.minimum-size.x-math", 0);
pref("font.size.variable.x-math", 16);
pref("font.size.monospace.x-math", 13);

/*
 * When enabled, the touch.radius and mouse.radius prefs allow events to be dispatched
 * to nearby elements that are sensitive to the event. See PositionedEventTargeting.cpp.
 * The 'mm' prefs define a rectangle around the nominal event target point within which
 * we will search for suitable elements. 'visitedWeight' is a percentage weight;
 * a value > 100 makes a visited link be treated as further away from the event target
 * than it really is, while a value < 100 makes a visited link be treated as closer
 * to the event target than it really is.
 */
pref("ui.touch.radius.enabled", false);
pref("ui.touch.radius.leftmm", 8);
pref("ui.touch.radius.topmm", 12);
pref("ui.touch.radius.rightmm", 8);
pref("ui.touch.radius.bottommm", 4);
pref("ui.touch.radius.visitedWeight", 120);

pref("ui.mouse.radius.enabled", false);
pref("ui.mouse.radius.leftmm", 8);
pref("ui.mouse.radius.topmm", 12);
pref("ui.mouse.radius.rightmm", 8);
pref("ui.mouse.radius.bottommm", 4);
pref("ui.mouse.radius.visitedWeight", 120);

// When true, the ui.mouse.radius.* prefs will only affect simulated mouse events generated by touch input.
// When false, the prefs will be used for all mouse events.
pref("ui.mouse.radius.inputSource.touchOnly", true);

#ifdef XP_WIN

  pref("font.name-list.emoji", "Twemoji Mozilla, Segoe UI Emoji");

  pref("font.name-list.serif.ar", "Times New Roman");
  pref("font.name-list.sans-serif.ar", "Segoe UI, Tahoma, Arial");
  pref("font.name-list.monospace.ar", "Courier New");
  pref("font.name-list.cursive.ar", "Comic Sans MS");

  pref("font.name-list.serif.el", "Times New Roman");
  pref("font.name-list.sans-serif.el", "Arial");
  pref("font.name-list.monospace.el", "Courier New");
  pref("font.name-list.cursive.el", "Comic Sans MS");

  pref("font.name-list.serif.he", "Narkisim, David");
  pref("font.name-list.sans-serif.he", "Arial");
  pref("font.name-list.monospace.he", "Fixed Miriam Transparent, Miriam Fixed, Rod, Courier New");
  pref("font.name-list.cursive.he", "Guttman Yad, Ktav, Arial");

  pref("font.name-list.serif.ja", "Yu Mincho, MS PMincho, MS Mincho, Meiryo, Yu Gothic, MS PGothic, MS Gothic");
  pref("font.name-list.sans-serif.ja", "Meiryo, Yu Gothic, MS PGothic, MS Gothic, Yu Mincho, MS PMincho, MS Mincho");
  pref("font.name-list.monospace.ja", "MS Gothic, MS Mincho, Meiryo, Yu Gothic, Yu Mincho, MS PGothic, MS PMincho");

  pref("font.name-list.serif.ko", "Batang, Gulim");
  pref("font.name-list.sans-serif.ko", "Malgun Gothic, Gulim");
  pref("font.name-list.monospace.ko", "GulimChe");
  pref("font.name-list.cursive.ko", "Gungsuh");

  pref("font.name-list.serif.th", "Tahoma");
  pref("font.name-list.sans-serif.th", "Tahoma");
  pref("font.name-list.monospace.th", "Tahoma");
  pref("font.name-list.cursive.th", "Tahoma");

  pref("font.name-list.serif.x-cyrillic", "Times New Roman");
  pref("font.name-list.sans-serif.x-cyrillic", "Arial");
  pref("font.name-list.monospace.x-cyrillic", "Courier New");
  pref("font.name-list.cursive.x-cyrillic", "Comic Sans MS");

  pref("font.name-list.serif.x-unicode", "Times New Roman");
  pref("font.name-list.sans-serif.x-unicode", "Arial");
  pref("font.name-list.monospace.x-unicode", "Courier New");
  pref("font.name-list.cursive.x-unicode", "Comic Sans MS");

  pref("font.name-list.serif.x-western", "Times New Roman");
  pref("font.name-list.sans-serif.x-western", "Arial");
  pref("font.name-list.monospace.x-western", "Courier New");
  pref("font.name-list.cursive.x-western", "Comic Sans MS");

  pref("font.name-list.serif.zh-CN", "SimSun, MS Song, SimSun-ExtB");
  pref("font.name-list.sans-serif.zh-CN", "Microsoft YaHei, SimHei");
  pref("font.name-list.monospace.zh-CN", "SimSun, MS Song, SimSun-ExtB");
  pref("font.name-list.cursive.zh-CN", "KaiTi, KaiTi_GB2312");

  // Per Taiwanese users' demand. They don't want to use TC fonts for
  // rendering Latin letters. (bug 88579)
  pref("font.name-list.serif.zh-TW", "Times New Roman, PMingLiu, MingLiU, MingLiU-ExtB");
  #ifdef EARLY_BETA_OR_EARLIER
    pref("font.name-list.sans-serif.zh-TW", "Arial, Microsoft JhengHei, PMingLiU, MingLiU, MingLiU-ExtB");
  #else
    pref("font.name-list.sans-serif.zh-TW", "Arial, PMingLiU, MingLiU, MingLiU-ExtB, Microsoft JhengHei");
  #endif
  pref("font.name-list.monospace.zh-TW", "MingLiU, MingLiU-ExtB");
  pref("font.name-list.cursive.zh-TW", "DFKai-SB");

  // hkscsm3u.ttf (HKSCS-2001) :  http://www.microsoft.com/hk/hkscs
  // Hong Kong users have the same demand about glyphs for Latin letters (bug 88579)
  pref("font.name-list.serif.zh-HK", "Times New Roman, MingLiu_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
  pref("font.name-list.sans-serif.zh-HK", "Arial, MingLiU_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
  pref("font.name-list.monospace.zh-HK", "MingLiU_HKSCS, Ming(for ISO10646), MingLiU, MingLiU_HKSCS-ExtB");
  pref("font.name-list.cursive.zh-HK", "DFKai-SB");

  pref("font.name-list.serif.x-devanagari", "Kokila, Raghindi");
  pref("font.name-list.sans-serif.x-devanagari", "Nirmala UI, Mangal");
  pref("font.name-list.monospace.x-devanagari", "Mangal, Nirmala UI");

  pref("font.name-list.serif.x-tamil", "Latha");
  pref("font.name-list.monospace.x-tamil", "Latha");

  // http://www.alanwood.net/unicode/fonts.html

  pref("font.name-list.serif.x-armn", "Sylfaen");
  pref("font.name-list.sans-serif.x-armn", "Arial AMU");
  pref("font.name-list.monospace.x-armn", "Arial AMU");

  pref("font.name-list.serif.x-beng", "Vrinda, Akaash, Likhan, Ekushey Punarbhaba");
  pref("font.name-list.sans-serif.x-beng", "Vrinda, Akaash, Likhan, Ekushey Punarbhaba");
  pref("font.name-list.monospace.x-beng", "Mitra Mono, Likhan, Mukti Narrow");

  pref("font.name-list.serif.x-cans", "Aboriginal Serif, BJCree Uni");
  pref("font.name-list.sans-serif.x-cans", "Aboriginal Sans");
  pref("font.name-list.monospace.x-cans", "Aboriginal Sans, OskiDakelh, Pigiarniq, Uqammaq");

  pref("font.name-list.serif.x-ethi", "Visual Geez Unicode, Visual Geez Unicode Agazian");
  pref("font.name-list.sans-serif.x-ethi", "GF Zemen Unicode");
  pref("font.name-list.monospace.x-ethi", "Ethiopia Jiret");
  pref("font.name-list.cursive.x-ethi", "Visual Geez Unicode Title");

  pref("font.name-list.serif.x-geor", "Sylfaen, BPG Paata Khutsuri U, TITUS Cyberbit Basic");
  pref("font.name-list.sans-serif.x-geor", "BPG Classic 99U");
  pref("font.name-list.monospace.x-geor", "BPG Classic 99U");

  pref("font.name-list.serif.x-gujr", "Shruti");
  pref("font.name-list.sans-serif.x-gujr", "Shruti");
  pref("font.name-list.monospace.x-gujr", "Shruti");

  pref("font.name-list.serif.x-guru", "Raavi, Saab");
  pref("font.name-list.sans-serif.x-guru", "");
  pref("font.name-list.monospace.x-guru", "Raavi, Saab");

  pref("font.name-list.serif.x-khmr", "PhnomPenh OT,.Mondulkiri U GR 1.5, Khmer OS");
  pref("font.name-list.sans-serif.x-khmr", "Khmer OS");
  pref("font.name-list.monospace.x-khmr", "Khmer OS, Khmer OS System");

  pref("font.name-list.serif.x-mlym", "Rachana_w01, AnjaliOldLipi, Kartika, ThoolikaUnicode");
  pref("font.name-list.sans-serif.x-mlym", "Rachana_w01, AnjaliOldLipi, Kartika, ThoolikaUnicode");
  pref("font.name-list.monospace.x-mlym", "Rachana_w01, AnjaliOldLipi, Kartika, ThoolikaUnicode");

  pref("font.name-list.serif.x-orya", "ori1Uni, Kalinga");
  pref("font.name-list.sans-serif.x-orya", "ori1Uni, Kalinga");
  pref("font.name-list.monospace.x-orya", "ori1Uni, Kalinga");

  pref("font.name-list.serif.x-telu", "Gautami, Akshar Unicode");
  pref("font.name-list.sans-serif.x-telu", "Gautami, Akshar Unicode");
  pref("font.name-list.monospace.x-telu", "Gautami, Akshar Unicode");

  pref("font.name-list.serif.x-knda", "Tunga, AksharUnicode");
  pref("font.name-list.sans-serif.x-knda", "Tunga, AksharUnicode");
  pref("font.name-list.monospace.x-knda", "Tunga, AksharUnicode");

  pref("font.name-list.serif.x-sinh", "Iskoola Pota, AksharUnicode");
  pref("font.name-list.sans-serif.x-sinh", "Iskoola Pota, AksharUnicode");
  pref("font.name-list.monospace.x-sinh", "Iskoola Pota, AksharUnicode");

  pref("font.name-list.serif.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");
  pref("font.name-list.sans-serif.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");
  pref("font.name-list.monospace.x-tibt", "Tibetan Machine Uni, Jomolhari, Microsoft Himalaya");

  pref("font.minimum-size.th", 10);

  pref("font.default.x-devanagari", "sans-serif");

  pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Times New Roman");
  pref("font.name-list.sans-serif.x-math", "Arial");
  pref("font.name-list.monospace.x-math", "Courier New");
  pref("font.name-list.cursive.x-math", "Comic Sans MS");

  // ClearType tuning parameters for directwrite/d2d.
  //
  // Allows overriding of underlying registry values in:
  //   HKCU/Software/Microsoft/Avalon.Graphics/<display> (contrast and level)
  //   HKLM/Software/Microsoft/Avalon.Graphics/<display> (gamma, pixel structure)
  // and selection of the ClearType/antialiasing mode.
  //
  // A value of -1 implies use the default value, otherwise value ranges
  // follow registry settings:
  //   gamma [1000, 2200]  default: based on screen, typically 2200 (== 2.2)
  //   enhanced contrast [0, 1000] default: 50
  //   cleartype level [0, 100] default: 100
  //   pixel structure [0, 2] default: 0 (flat/RGB/BGR)
  //   rendering mode [0, 5] default: 0
  //     0 = use default for font & size;
  //     1 = aliased;
  //     2 = GDI Classic;
  //     3 = GDI Natural Widths;
  //     4 = Natural;
  //     5 = Natural Symmetric
  //
  // See:
  //   http://msdn.microsoft.com/en-us/library/aa970267.aspx
  //   http://msdn.microsoft.com/en-us/library/dd368190%28v=VS.85%29.aspx
  // Note: DirectWrite uses the "Enhanced Contrast Level" value rather than the
  // "Text Contrast Level" value

  pref("gfx.font_rendering.cleartype_params.gamma", -1);
  pref("gfx.font_rendering.cleartype_params.enhanced_contrast", -1);
  pref("gfx.font_rendering.cleartype_params.cleartype_level", -1);
  pref("gfx.font_rendering.cleartype_params.pixel_structure", -1);
  pref("gfx.font_rendering.cleartype_params.rendering_mode", -1);

  // A comma-separated list of font family names. Fonts in these families will
  // be forced to use "GDI Classic" ClearType mode, provided the value
  // of gfx.font_rendering.cleartype_params.rendering_mode is -1
  // (i.e. a specific rendering_mode has not been explicitly set).
  // Currently we apply this setting to the sans-serif Microsoft "core Web fonts".
  pref("gfx.font_rendering.cleartype_params.force_gdi_classic_for_families",
       "Arial,Consolas,Courier New,Microsoft Sans Serif,Segoe UI,Tahoma,Trebuchet MS,Verdana");
  // The maximum size at which we will force GDI classic mode using
  // force_gdi_classic_for_families.
  pref("gfx.font_rendering.cleartype_params.force_gdi_classic_max_size", 15);

  // Locate plugins by scanning the Adobe Acrobat installation directory with a minimum version
  pref("plugin.scan.Acrobat", "5.0");

  // Locate plugins by scanning the Quicktime installation directory with a minimum version
  pref("plugin.scan.Quicktime", "5.0");

  // Locate and scan the Window Media Player installation directory for plugins with a minimum version
  pref("plugin.scan.WindowsMediaPlayer", "7.0");

  // Locate plugins by the directories specified in the Windows registry for PLIDs
  // Which is currently HKLM\Software\MozillaPlugins\xxxPLIDxxx\Path
  pref("plugin.scan.plid.all", true);

  // Switch the keyboard layout per window
  pref("intl.keyboard.per_window_layout", false);

  // Whether Gecko sets input scope of the URL bar to IS_DEFAULT when black
  // listed IMEs are active.  If you use tablet mode mainly and you want to
  // use touch keyboard for URL when you set focus to the URL bar, you can
  // set this to false.  Then, you'll see, e.g., ".com" key on the keyboard.
  // However, if you set this to false, such IMEs set its open state to "closed"
  // when you set focus to the URL bar.  I.e., input mode is automatically
  // changed to English input mode.
  // Black listed IMEs:
  //   - Microsoft IME for Japanese
  //   - Google Japanese Input
  //   - Microsoft Bopomofo
  //   - Microsoft ChangJie
  //   - Microsoft Phonetic
  //   - Microsoft Quick
  //   - Microsoft New ChangJie
  //   - Microsoft New Phonetic
  //   - Microsoft New Quick
  //   - Microsoft Pinyin
  //   - Microsoft Pinyin New Experience Input Style
  //   - Microsoft Wubi
  //   - Microsoft IME for Korean (except on Win7)
  //   - Microsoft Old Hangul
  pref("intl.ime.hack.set_input_scope_of_url_bar_to_default", true);

  #ifdef NS_ENABLE_TSF
    // Enable/Disable TSF support.
    pref("intl.tsf.enable", true);

    // Support IMEs implemented with IMM in TSF mode.
    pref("intl.tsf.support_imm", true);

    // This is referred only when both "intl.tsf.enable" and
    // "intl.tsf.support_imm" are true.  When this is true, default IMC is
    // associated with focused window only when active keyboard layout is a
    // legacy IMM-IME.
    pref("intl.tsf.associate_imc_only_when_imm_ime_is_active", false);

    // Enables/Disables hack for specific TIP.

    // On Windows 10 Build 17643 (an Insider Preview build of RS5), Microsoft
    // have fixed the caller of ITextACPStore::GetTextExt() to return
    // TS_E_NOLAYOUT to TIP as-is, rather than converting to E_FAIL.
    // Therefore, if TIP supports asynchronous layout computation perfectly, we
    // can return TS_E_NOLAYOUT and TIP waits next OnLayoutChange()
    // notification.  However, some TIPs still have some bugs of asynchronous
    // layout support.  We keep hacking the result of GetTextExt() like running
    // on Windows 10, however, there could be unknown TIP bugs if we stop
    // hacking the result.  So, user can stop checking build ID to make Gecko
    // hack the result forcibly.
    #ifdef EARLY_BETA_OR_EARLIER
      pref("intl.tsf.hack.allow_to_stop_hacking_on_build_17643_or_later", true);
    #else
      pref("intl.tsf.hack.allow_to_stop_hacking_on_build_17643_or_later", false);
    #endif

    // Whether creates native caret for ATOK or not.
    pref("intl.tsf.hack.atok.create_native_caret", true);
    // Whether use available composition string rect for result of
    // ITextStoreACP::GetTextExt() even if the specified range is same as the
    // range of composition string but some character rects of them are not
    // available.  Note that this is ignored if active ATOK is or older than
    // 2016 and create_native_caret is true.
    pref("intl.tsf.hack.atok.do_not_return_no_layout_error_of_composition_string", true);
    // Whether use available composition string rect for result of
    // ITextStoreACP::GetTextExt() even if the specified range is same as or is
    // in the range of composition string but some character rects of them are
    // not available.
    pref("intl.tsf.hack.japanist10.do_not_return_no_layout_error_of_composition_string", true);
    // Whether use composition start position for the result of
    // ITfContextView::GetTextExt() if the specified range is larger than
    // composition start offset.
    // For Free ChangJie 2010
    pref("intl.tsf.hack.free_chang_jie.do_not_return_no_layout_error", true);
    // For Microsoft Pinyin and Microsoft Wubi
    pref("intl.tsf.hack.ms_simplified_chinese.do_not_return_no_layout_error", true);
    // For Microsoft ChangJie and Microsoft Quick
    pref("intl.tsf.hack.ms_traditional_chinese.do_not_return_no_layout_error", true);
    // Whether use previous character rect for the result of
    // ITfContextView::GetTextExt() if the specified range is the first
    // character of selected clause of composition string.
    pref("intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_first_char", true);
    // Whether use previous character rect for the result of
    // ITfContextView::GetTextExt() if the specified range is the caret of
    // composition string.
    pref("intl.tsf.hack.ms_japanese_ime.do_not_return_no_layout_error_at_caret", true);
    // Whether hack ITextStoreACP::QueryInsert() or not.  The method should
    // return new selection after specified length text is inserted at
    // specified range. However, Microsoft's some Chinese TIPs expect that the
    // result is same as specified range.  If following prefs are true,
    // ITextStoreACP::QueryInsert() returns specified range only when one of
    // the TIPs is active. For Microsoft Pinyin and Microsoft Wubi.
    pref("intl.tsf.hack.ms_simplified_chinese.query_insert_result", true);
    // For Microsoft ChangJie and Microsoft Quick
    pref("intl.tsf.hack.ms_traditional_chinese.query_insert_result", true);
  #endif // NS_ENABLE_TSF

  // If composition_font is set, Gecko sets the font to IME.  IME may use
  // the fonts on their window like candidate window.  If they are empty,
  // Gecko uses the system default font which is set to the IM context.
  // The font name must not start with '@'.  When the writing mode is vertical,
  // Gecko inserts '@' to the start of the font name automatically.
  // FYI: Changing these prefs requires to restart.
  pref("intl.imm.composition_font", "");

  // Japanist 2003's candidate window is broken if the font is "@System" which
  // is default composition font for vertical writing mode.
  // You can specify font to use on candidate window of Japanist 2003.
  // This value must not start with '@'.
  // FYI: Changing this pref requires to restart.
  pref("intl.imm.composition_font.japanist_2003", "MS PGothic");

  // Even if IME claims that they support vertical writing mode but it may not
  // support vertical writing mode for its candidate window.  This pref allows
  // to ignore the claim.
  // FYI: Changing this pref requires to restart.
  pref("intl.imm.vertical_writing.always_assume_not_supported", false);

  // We cannot retrieve active IME name with IMM32 API if a TIP of TSF is
  // active. This pref can specify active IME name when Japanese TIP is active.
  // For example:
  //   Google Japanese Input: "Google 日本語入力 IMM32 モジュール"
  //   ATOK 2011: "ATOK 2011" (similarly, e.g., ATOK 2013 is "ATOK 2013")
  pref("intl.imm.japanese.assume_active_tip_name_as", "");

  // See bug 448927, on topmost panel, some IMEs are not usable on Windows.
  pref("ui.panel.default_level_parent", false);

  // Enable system settings cache for mouse wheel message handling.
  // Note that even if this pref is set to true, Gecko may not cache the system
  // settings if Gecko detects that the cache won't be refreshed properly when
  // the settings are changed.
  pref("mousewheel.system_settings_cache.enabled", true);

  // This is a pref to test system settings cache for mouse wheel message
  // handling.  If this is set to true, Gecko forcibly use the cache.
  pref("mousewheel.system_settings_cache.force_enabled", false);

  // High resolution scrolling with supported mouse drivers on Vista or later.
  pref("mousewheel.enable_pixel_scrolling", true);

  // If your mouse drive sends WM_*SCROLL messages when you turn your mouse
  // wheel, set this to true.  Then, gecko processes them as mouse wheel
  // messages.
  pref("mousewheel.emulate_at_wm_scroll", false);

  // Some odd touchpad utils give focus to window under cursor when user tries
  // to scroll.  If this is true, Gecko tries to emulate such odd behavior.
  // Don't make this true unless you want to debug.  Enabling this pref causes
  // making damage to the performance.
  pref("mousewheel.debug.make_window_under_cursor_foreground", false);

  // Enables or disabled the TrackPoint hack, -1 is autodetect, 0 is off,
  // and 1 is on.  Set this to 1 if TrackPoint scrolling is not working.
  pref("ui.trackpoint_hack.enabled", -1);

  // Setting this to a non-empty string overrides the Win32 "window class" used
  // for "normal" windows. Setting this to MozillaUIWindowClass might make
  // some trackpad drivers behave better.
  pref("ui.window_class_override", "");

  // Enables or disables the Elantech gesture hacks.  -1 is autodetect, 0 is
  // off, and 1 is on.  Set this to 1 if three-finger swipe gestures do not
  // cause page back/forward actions, or if pinch-to-zoom does not work.
  pref("ui.elantech_gesture_hacks.enabled", -1);

  // Show the Windows on-screen keyboard (osk.exe) when a text field is focused.
  pref("ui.osk.enabled", true);
  // Only show the on-screen keyboard if there are no physical keyboards
  // attached to the device.
  pref("ui.osk.detect_physical_keyboard", true);
  // Path to TabTip.exe on local machine. Cached for performance reasons.
  pref("ui.osk.on_screen_keyboard_path", "");
  // Only try to show the on-screen keyboard on Windows 10 and later. Setting
  // this pref to false will allow the OSK to show on Windows 8 and 8.1.
  pref("ui.osk.require_win10", false);
  // This pref stores the "reason" that the on-screen keyboard was either
  // shown or not shown when focus is moved to an editable text field. It is
  // used to help debug why the keyboard is either not appearing when expected
  // or appearing when it is not expected.
  pref("ui.osk.debug.keyboardDisplayReason", "");

#endif // XP_WIN

#ifdef XP_MACOSX
  // Mac specific preference defaults
  pref("browser.drag_out_of_frame_style", 1);
  pref("ui.key.saveLink.shift", false); // true = shift, false = meta

  // default fonts (in UTF8 and using canonical names)
  // to determine canonical font names, use a debug build and
  // enable NSPR logging for module fontInfoLog:5
  // canonical names immediately follow '(fontinit) family:' in the log

  pref("font.name-list.emoji", "Apple Color Emoji");

  pref("font.name-list.serif.ar", "Al Bayan");
  pref("font.name-list.sans-serif.ar", "Geeza Pro");
  pref("font.name-list.monospace.ar", "Geeza Pro");
  pref("font.name-list.cursive.ar", "DecoType Naskh");
  pref("font.name-list.fantasy.ar", "KufiStandardGK");

  pref("font.name-list.serif.el", "Times, Times New Roman");
  pref("font.name-list.sans-serif.el", "Helvetica, Lucida Grande");
  pref("font.name-list.monospace.el", "Courier New, Lucida Grande");
  pref("font.name-list.cursive.el", "Lucida Grande, Times");
  pref("font.name-list.fantasy.el", "Lucida Grande, Times");

  pref("font.name-list.serif.he", "Times New Roman");
  pref("font.name-list.sans-serif.he", "Arial");
  pref("font.name-list.monospace.he", "Courier New");
  pref("font.name-list.cursive.he", "Times New Roman");
  pref("font.name-list.fantasy.he", "Times New Roman");

  pref("font.name-list.serif.ja", "Hiragino Mincho ProN, Hiragino Mincho Pro");
  pref("font.name-list.sans-serif.ja", "Hiragino Kaku Gothic ProN, Hiragino Kaku Gothic Pro");
  pref("font.name-list.monospace.ja", "Osaka-Mono");

  pref("font.name-list.serif.ko", "AppleMyungjo");
  pref("font.name-list.sans-serif.ko", "Apple SD Gothic Neo, AppleGothic");
  pref("font.name-list.monospace.ko", "Apple SD Gothic Neo, AppleGothic");

  pref("font.name-list.serif.th", "Thonburi");
  pref("font.name-list.sans-serif.th", "Thonburi");
  pref("font.name-list.monospace.th", "Ayuthaya");

  pref("font.name-list.serif.x-armn", "Mshtakan");
  pref("font.name-list.sans-serif.x-armn", "Mshtakan");
  pref("font.name-list.monospace.x-armn", "Mshtakan");

  // SolaimanLipi, Rupali http://ekushey.org/?page/mac_download
  pref("font.name-list.serif.x-beng", "Bangla MN");
  pref("font.name-list.sans-serif.x-beng", "Bangla Sangam MN");
  pref("font.name-list.monospace.x-beng", "Bangla Sangam MN");

  pref("font.name-list.serif.x-cans", "Euphemia UCAS");
  pref("font.name-list.sans-serif.x-cans", "Euphemia UCAS");
  pref("font.name-list.monospace.x-cans", "Euphemia UCAS");

  pref("font.name-list.serif.x-cyrillic", "Times, Times New Roman");
  pref("font.name-list.sans-serif.x-cyrillic", "Helvetica, Arial");
  pref("font.name-list.monospace.x-cyrillic", "Monaco, Courier New");
  pref("font.name-list.cursive.x-cyrillic", "Geneva");
  pref("font.name-list.fantasy.x-cyrillic", "Charcoal CY");

  pref("font.name-list.serif.x-devanagari", "Devanagari MT");
  pref("font.name-list.sans-serif.x-devanagari", "Devanagari Sangam MN, Devanagari MT");
  pref("font.name-list.monospace.x-devanagari", "Devanagari Sangam MN, Devanagari MT");

  // Abyssinica SIL http://scripts.sil.org/AbyssinicaSIL_Download
  pref("font.name-list.serif.x-ethi", "Kefa, Abyssinica SIL");
  pref("font.name-list.sans-serif.x-ethi", "Kefa, Abyssinica SIL");
  pref("font.name-list.monospace.x-ethi", "Kefa, Abyssinica SIL");

  // no suitable fonts for georgian ship with mac os x
  // however some can be freely downloaded
  // TITUS Cyberbit Basic http://titus.fkidg1.uni-frankfurt.de/unicode/tituut.asp
  // Zuzumbo http://homepage.mac.com/rsiradze/FileSharing91.html
  pref("font.name-list.serif.x-geor", "TITUS Cyberbit Basic");
  pref("font.name-list.sans-serif.x-geor", "Zuzumbo");
  pref("font.name-list.monospace.x-geor", "Zuzumbo");

  pref("font.name-list.serif.x-gujr", "Gujarati MT");
  pref("font.name-list.sans-serif.x-gujr", "Gujarati Sangam MN, Gujarati MT");
  pref("font.name-list.monospace.x-gujr", "Gujarati Sangam MN, Gujarati MT");

  pref("font.name-list.serif.x-guru", "Gurmukhi MT");
  pref("font.name-list.sans-serif.x-guru", "Gurmukhi MT");
  pref("font.name-list.monospace.x-guru", "Gurmukhi MT");

  pref("font.name-list.serif.x-khmr", "Khmer MN");
  pref("font.name-list.sans-serif.x-khmr", "Khmer Sangam MN");
  pref("font.name-list.monospace.x-khmr", "Khmer Sangam MN");

  pref("font.name-list.serif.x-mlym", "Malayalam MN");
  pref("font.name-list.sans-serif.x-mlym", "Malayalam Sangam MN");
  pref("font.name-list.monospace.x-mlym", "Malayalam Sangam MN");

  pref("font.name-list.serif.x-orya", "Oriya MN");
  pref("font.name-list.sans-serif.x-orya", "Oriya Sangam MN");
  pref("font.name-list.monospace.x-orya", "Oriya Sangam MN");

  // Pothana http://web.nickshanks.com/typography/telugu/
  pref("font.name-list.serif.x-telu", "Telugu MN, Pothana");
  pref("font.name-list.sans-serif.x-telu", "Telugu Sangam MN, Pothana");
  pref("font.name-list.monospace.x-telu", "Telugu Sangam MN, Pothana");

  // Kedage http://web.nickshanks.com/typography/kannada/
  pref("font.name-list.serif.x-knda", "Kannada MN, Kedage");
  pref("font.name-list.sans-serif.x-knda", "Kannada Sangam MN, Kedage");
  pref("font.name-list.monospace.x-knda", "Kannada Sangam MN, Kedage");

  pref("font.name-list.serif.x-sinh", "Sinhala MN");
  pref("font.name-list.sans-serif.x-sinh", "Sinhala Sangam MN");
  pref("font.name-list.monospace.x-sinh", "Sinhala Sangam MN");

  pref("font.name-list.serif.x-tamil", "InaiMathi");
  pref("font.name-list.sans-serif.x-tamil", "InaiMathi");
  pref("font.name-list.monospace.x-tamil", "InaiMathi");

  // Kailasa ships with mac os x >= 10.5
  pref("font.name-list.serif.x-tibt", "Kailasa");
  pref("font.name-list.sans-serif.x-tibt", "Kailasa");
  pref("font.name-list.monospace.x-tibt", "Kailasa");

  pref("font.name-list.serif.x-unicode", "Times");
  pref("font.name-list.sans-serif.x-unicode", "Helvetica");
  pref("font.name-list.monospace.x-unicode", "Courier");
  pref("font.name-list.cursive.x-unicode", "Apple Chancery");
  pref("font.name-list.fantasy.x-unicode", "Papyrus");

  pref("font.name-list.serif.x-western", "Times, Times New Roman");
  pref("font.name-list.sans-serif.x-western", "Helvetica, Arial");
  pref("font.name-list.monospace.x-western", "Courier, Courier New");
  pref("font.name-list.cursive.x-western", "Apple Chancery");
  pref("font.name-list.fantasy.x-western", "Papyrus");

  pref("font.name-list.serif.zh-CN", "Times, Songti SC, STSong, Heiti SC");
  pref("font.name-list.sans-serif.zh-CN", "Helvetica, PingFang SC, STHeiti, Heiti SC");
  pref("font.name-list.monospace.zh-CN", "Courier, PingFang SC, STHeiti, Heiti SC");
  pref("font.name-list.cursive.zh-CN", "Kaiti SC");

  pref("font.name-list.serif.zh-TW", "Times, Songti TC, LiSong Pro, Heiti TC");
  pref("font.name-list.sans-serif.zh-TW", "Helvetica, PingFang TC, Heiti TC, LiHei Pro");
  pref("font.name-list.monospace.zh-TW", "Courier, PingFang TC, Heiti TC, LiHei Pro");
  pref("font.name-list.cursive.zh-TW", "Kaiti TC");

  pref("font.name-list.serif.zh-HK", "Times, Songti TC, LiSong Pro, Heiti TC");
  pref("font.name-list.sans-serif.zh-HK", "Helvetica, PingFang TC, Heiti TC, LiHei Pro");
  pref("font.name-list.monospace.zh-HK", "Courier, PingFang TC, Heiti TC, LiHei Pro");
  pref("font.name-list.cursive.zh-HK", "Kaiti TC");

  // XP_MACOSX changes to default font sizes
  pref("font.minimum-size.th", 10);

  // Apple's Symbol is Unicode so use it
  pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Symbol, Times");
  pref("font.name-list.sans-serif.x-math", "Helvetica");
  pref("font.name-list.monospace.x-math", "Courier");
  pref("font.name-list.cursive.x-math", "Apple Chancery");
  pref("font.name-list.fantasy.x-math", "Papyrus");

  // Individual font faces to be treated as independent families,
  // listed as <Postscript name of face:Owning family name>
  pref("font.single-face-list", "Osaka-Mono:Osaka");

  // optimization hint for fonts with localized names to be read in at startup, otherwise read in at lookup miss
  // names are canonical family names (typically English names)
  pref("font.preload-names-list", "Hiragino Kaku Gothic ProN,Hiragino Mincho ProN,STSong");

  // Override font-weight values for some problematic families Apple ships
  // (see bug 931426).
  // The name here is the font's PostScript name, which can be checked in
  // the Font Book utility or other tools.
  pref("font.weight-override.AppleSDGothicNeo-Thin", 100); // Ensure Thin < UltraLight < Light
  pref("font.weight-override.AppleSDGothicNeo-UltraLight", 200);
  pref("font.weight-override.AppleSDGothicNeo-Light", 300);
  pref("font.weight-override.AppleSDGothicNeo-Heavy", 900); // Ensure Heavy > ExtraBold (800)

  pref("font.weight-override.Avenir-Book", 300); // Ensure Book < Roman (400)
  pref("font.weight-override.Avenir-BookOblique", 300);
  pref("font.weight-override.Avenir-MediumOblique", 500); // Harmonize MediumOblique with Medium
  pref("font.weight-override.Avenir-Black", 900); // Ensure Black > Heavy (800)
  pref("font.weight-override.Avenir-BlackOblique", 900);

  pref("font.weight-override.AvenirNext-MediumItalic", 500); // Harmonize MediumItalic with Medium
  pref("font.weight-override.AvenirNextCondensed-MediumItalic", 500);

  pref("font.weight-override.HelveticaNeue-Light", 300); // Ensure Light > Thin (200)
  pref("font.weight-override.HelveticaNeue-LightItalic", 300);
  pref("font.weight-override.HelveticaNeue-MediumItalic", 500); // Harmonize MediumItalic with Medium

  // Override the Windows settings: no menu key, meta accelerator key. ctrl for general access key in HTML/XUL
  // Use 17 for Ctrl, 18 for Option, 224 for Cmd, 0 for none
  pref("ui.key.menuAccessKey", 0);
  pref("ui.key.accelKey", 224);

  // See bug 404131, topmost <panel> element wins to Dashboard on MacOSX.
  pref("ui.panel.default_level_parent", false);

  pref("ui.plugin.cancel_composition_at_input_source_changed", false);

  // Macbook touchpad two finger pixel scrolling
  pref("mousewheel.enable_pixel_scrolling", true);

#endif // XP_MACOSX

#ifdef ANDROID
  // Handled differently under Mac/Windows
  pref("network.protocol-handler.warn-external.file", false);
  pref("browser.drag_out_of_frame_style", 1);

  // Middle-mouse handling
  pref("middlemouse.paste", true);
  pref("middlemouse.openNewWindow", true);
  pref("middlemouse.scrollbarPosition", true);

  // Tab focus model bit field:
  // 1 focuses text controls, 2 focuses other form elements, 4 adds links.
  // Leave this at the default, 7, to match mozilla1.0-era user expectations.
  // pref("accessibility.tabfocus", 1);

  pref("helpers.global_mime_types_file", "/etc/mime.types");
  pref("helpers.global_mailcap_file", "/etc/mailcap");
  pref("helpers.private_mime_types_file", "~/.mime.types");
  pref("helpers.private_mailcap_file", "~/.mailcap");
  pref("print.printer_list", ""); // list of printers, separated by spaces
  pref("print.print_reversed", false);
  pref("print.print_in_color", true);

  /* PostScript print module prefs */
  // pref("print.postscript.enabled",      true);

  // Setting default_level_parent to true makes the default level for popup
  // windows "top" instead of "parent".  On GTK2 platform, this is implemented
  // with override-redirect windows which is the normal way to implement
  // temporary popup windows.  Setting this to false would make the default
  // level "parent" which is implemented with managed windows.
  // A problem with using managed windows is that metacity sometimes deactivates
  // the parent window when the managed popup is shown.
  pref("ui.panel.default_level_parent", true);

  // Forward downloads with known OMA MIME types to Android's download manager
  // instead of downloading them in the browser.
  pref("browser.download.forward_oma_android_download_manager", false);

#endif // ANDROID

#if !defined(ANDROID) && !defined(XP_MACOSX) && defined(XP_UNIX)
  // Handled differently under Mac/Windows
  pref("network.protocol-handler.warn-external.file", false);
  pref("browser.drag_out_of_frame_style", 1);

  // Middle-mouse handling
  pref("middlemouse.paste", true);
  pref("middlemouse.openNewWindow", true);
  pref("middlemouse.scrollbarPosition", true);

  // Tab focus model bit field:
  // 1 focuses text controls, 2 focuses other form elements, 4 adds links.
  // Leave this at the default, 7, to match mozilla1.0-era user expectations.
  // pref("accessibility.tabfocus", 1);

  pref("helpers.global_mime_types_file", "/etc/mime.types");
  pref("helpers.global_mailcap_file", "/etc/mailcap");
  pref("helpers.private_mime_types_file", "~/.mime.types");
  pref("helpers.private_mailcap_file", "~/.mailcap");
  pref("print.printer_list", ""); // list of printers, separated by spaces
  pref("print.print_reversed", false);
  pref("print.print_in_color", true);

  // font names

  // fontconfig doesn't support emoji yet
  // https://lists.freedesktop.org/archives/fontconfig/2016-October/005842.html
  pref("font.name-list.emoji", "Twemoji Mozilla");

  pref("font.name-list.serif.ar", "serif");
  pref("font.name-list.sans-serif.ar", "sans-serif");
  pref("font.name-list.monospace.ar", "monospace");
  pref("font.name-list.cursive.ar", "cursive");
  pref("font.size.monospace.ar", 12);

  pref("font.name-list.serif.el", "serif");
  pref("font.name-list.sans-serif.el", "sans-serif");
  pref("font.name-list.monospace.el", "monospace");
  pref("font.name-list.cursive.el", "cursive");
  pref("font.size.monospace.el", 12);

  pref("font.name-list.serif.he", "serif");
  pref("font.name-list.sans-serif.he", "sans-serif");
  pref("font.name-list.monospace.he", "monospace");
  pref("font.name-list.cursive.he", "cursive");
  pref("font.size.monospace.he", 12);

  pref("font.name-list.serif.ja", "serif");
  pref("font.name-list.sans-serif.ja", "sans-serif");
  pref("font.name-list.monospace.ja", "monospace");
  pref("font.name-list.cursive.ja", "cursive");

  pref("font.name-list.serif.ko", "serif");
  pref("font.name-list.sans-serif.ko", "sans-serif");
  pref("font.name-list.monospace.ko", "monospace");
  pref("font.name-list.cursive.ko", "cursive");

  pref("font.name-list.serif.th", "serif");
  pref("font.name-list.sans-serif.th", "sans-serif");
  pref("font.name-list.monospace.th", "monospace");
  pref("font.name-list.cursive.th", "cursive");
  pref("font.minimum-size.th", 13);

  pref("font.name-list.serif.x-armn", "serif");
  pref("font.name-list.sans-serif.x-armn", "sans-serif");
  pref("font.name-list.monospace.x-armn", "monospace");
  pref("font.name-list.cursive.x-armn", "cursive");

  pref("font.name-list.serif.x-beng", "serif");
  pref("font.name-list.sans-serif.x-beng", "sans-serif");
  pref("font.name-list.monospace.x-beng", "monospace");
  pref("font.name-list.cursive.x-beng", "cursive");

  pref("font.name-list.serif.x-cans", "serif");
  pref("font.name-list.sans-serif.x-cans", "sans-serif");
  pref("font.name-list.monospace.x-cans", "monospace");
  pref("font.name-list.cursive.x-cans", "cursive");

  pref("font.name-list.serif.x-cyrillic", "serif");
  pref("font.name-list.sans-serif.x-cyrillic", "sans-serif");
  pref("font.name-list.monospace.x-cyrillic", "monospace");
  pref("font.name-list.cursive.x-cyrillic", "cursive");
  pref("font.size.monospace.x-cyrillic", 12);

  pref("font.name-list.serif.x-devanagari", "serif");
  pref("font.name-list.sans-serif.x-devanagari", "sans-serif");
  pref("font.name-list.monospace.x-devanagari", "monospace");
  pref("font.name-list.cursive.x-devanagari", "cursive");

  pref("font.name-list.serif.x-ethi", "serif");
  pref("font.name-list.sans-serif.x-ethi", "sans-serif");
  pref("font.name-list.monospace.x-ethi", "monospace");
  pref("font.name-list.cursive.x-ethi", "cursive");

  pref("font.name-list.serif.x-geor", "serif");
  pref("font.name-list.sans-serif.x-geor", "sans-serif");
  pref("font.name-list.monospace.x-geor", "monospace");
  pref("font.name-list.cursive.x-geor", "cursive");

  pref("font.name-list.serif.x-gujr", "serif");
  pref("font.name-list.sans-serif.x-gujr", "sans-serif");
  pref("font.name-list.monospace.x-gujr", "monospace");
  pref("font.name-list.cursive.x-gujr", "cursive");

  pref("font.name-list.serif.x-guru", "serif");
  pref("font.name-list.sans-serif.x-guru", "sans-serif");
  pref("font.name-list.monospace.x-guru", "monospace");
  pref("font.name-list.cursive.x-guru", "cursive");

  pref("font.name-list.serif.x-khmr", "serif");
  pref("font.name-list.sans-serif.x-khmr", "sans-serif");
  pref("font.name-list.monospace.x-khmr", "monospace");
  pref("font.name-list.cursive.x-khmr", "cursive");

  pref("font.name-list.serif.x-knda", "serif");
  pref("font.name-list.sans-serif.x-knda", "sans-serif");
  pref("font.name-list.monospace.x-knda", "monospace");
  pref("font.name-list.cursive.x-knda", "cursive");

  pref("font.name-list.serif.x-mlym", "serif");
  pref("font.name-list.sans-serif.x-mlym", "sans-serif");
  pref("font.name-list.monospace.x-mlym", "monospace");
  pref("font.name-list.cursive.x-mlym", "cursive");

  pref("font.name-list.serif.x-orya", "serif");
  pref("font.name-list.sans-serif.x-orya", "sans-serif");
  pref("font.name-list.monospace.x-orya", "monospace");
  pref("font.name-list.cursive.x-orya", "cursive");

  pref("font.name-list.serif.x-sinh", "serif");
  pref("font.name-list.sans-serif.x-sinh", "sans-serif");
  pref("font.name-list.monospace.x-sinh", "monospace");
  pref("font.name-list.cursive.x-sinh", "cursive");

  pref("font.name-list.serif.x-tamil", "serif");
  pref("font.name-list.sans-serif.x-tamil", "sans-serif");
  pref("font.name-list.monospace.x-tamil", "monospace");
  pref("font.name-list.cursive.x-tamil", "cursive");

  pref("font.name-list.serif.x-telu", "serif");
  pref("font.name-list.sans-serif.x-telu", "sans-serif");
  pref("font.name-list.monospace.x-telu", "monospace");
  pref("font.name-list.cursive.x-telu", "cursive");

  pref("font.name-list.serif.x-tibt", "serif");
  pref("font.name-list.sans-serif.x-tibt", "sans-serif");
  pref("font.name-list.monospace.x-tibt", "monospace");
  pref("font.name-list.cursive.x-tibt", "cursive");

  pref("font.name-list.serif.x-unicode", "serif");
  pref("font.name-list.sans-serif.x-unicode", "sans-serif");
  pref("font.name-list.monospace.x-unicode", "monospace");
  pref("font.name-list.cursive.x-unicode", "cursive");
  pref("font.size.monospace.x-unicode", 12);

  pref("font.name-list.serif.x-western", "serif");
  pref("font.name-list.sans-serif.x-western", "sans-serif");
  pref("font.name-list.monospace.x-western", "monospace");
  pref("font.name-list.cursive.x-western", "cursive");
  pref("font.size.monospace.x-western", 12);

  pref("font.name-list.serif.zh-CN", "serif");
  pref("font.name-list.sans-serif.zh-CN", "sans-serif");
  pref("font.name-list.monospace.zh-CN", "monospace");
  pref("font.name-list.cursive.zh-CN", "cursive");

  pref("font.name-list.serif.zh-HK", "serif");
  pref("font.name-list.sans-serif.zh-HK", "sans-serif");
  pref("font.name-list.monospace.zh-HK", "monospace");
  pref("font.name-list.cursive.zh-HK", "cursive");

  pref("font.name-list.serif.zh-TW", "serif");
  pref("font.name-list.sans-serif.zh-TW", "sans-serif");
  pref("font.name-list.monospace.zh-TW", "monospace");
  pref("font.name-list.cursive.zh-TW", "cursive");

  /* PostScript print module prefs */
  // pref("print.postscript.enabled",      true);

  // On GTK2 platform, we should use topmost window level for the default window
  // level of <panel> element of XUL. GTK2 has only two window types. One is
  // normal top level window, other is popup window. The popup window is always
  // topmost window level, therefore, we are using normal top level window for
  // non-topmost panel, but it is pretty hacky. On some Window Managers, we have
  // 2 problems:
  // 1. The non-topmost panel steals focus from its parent window at showing.
  // 2. The parent of non-topmost panel is not activated when the panel is hidden.
  // So, we have no reasons we should use non-toplevel window for popup.
  pref("ui.panel.default_level_parent", true);

  pref("intl.ime.use_simple_context_on_password_field", false);

  // uim may use key snooper to listen to key events.  Unfortunately, we cannot
  // know whether it uses or not since it's a build option.  So, we need to make
  // distribution switchable whether we think uim uses key snooper or not with
  // this pref.  Debian 9.x still uses uim as their default IM and it uses key
  // snooper.  So, let's use true for its default value.
  pref("intl.ime.hack.uim.using_key_snooper", true);

  #ifdef MOZ_WIDGET_GTK
    // maximum number of fonts to substitute for a generic
    pref("gfx.font_rendering.fontconfig.max_generic_substitutions", 3);
  #endif

#endif // !ANDROID && !XP_MACOSX && XP_UNIX

#if defined(ANDROID)

  pref("font.size.monospace.ar", 12);

  pref("font.default.el", "sans-serif");
  pref("font.size.monospace.el", 12);

  pref("font.size.monospace.he", 12);

  pref("font.default.x-cyrillic", "sans-serif");
  pref("font.size.monospace.x-cyrillic", 12);

  pref("font.default.x-unicode", "sans-serif");
  pref("font.size.monospace.x-unicode", 12);

  pref("font.default.x-western", "sans-serif");
  pref("font.size.monospace.x-western", 12);

#endif // ANDROID

#if defined(ANDROID)
  // We use the bundled Charis SIL Compact as serif font for Firefox for Android

  pref("font.name-list.emoji", "Noto Color Emoji");

  pref("font.name-list.serif.ar", "Noto Naskh Arabic, Noto Serif, Droid Serif");
  pref("font.name-list.sans-serif.ar", "Noto Naskh Arabic, Roboto, Google Sans, Droid Sans");
  pref("font.name-list.monospace.ar", "Noto Naskh Arabic");

  pref("font.name-list.serif.el", "Droid Serif, Noto Serif"); // not Charis SIL Compact, only has a few Greek chars
  pref("font.name-list.sans-serif.el", "Roboto, Google Sans, Droid Sans");
  pref("font.name-list.monospace.el", "Droid Sans Mono");

  pref("font.name-list.serif.he", "Droid Serif, Noto Serif, Noto Serif Hebrew");
  pref("font.name-list.sans-serif.he", "Roboto, Google Sans, Noto Sans Hebrew, Droid Sans Hebrew, Droid Sans, Arial");
  pref("font.name-list.monospace.he", "Droid Sans Mono");

  pref("font.name-list.serif.ja", "Charis SIL Compact, Noto Serif CJK JP, Noto Serif, Droid Serif");
  pref("font.name-list.sans-serif.ja", "Roboto, Google Sans, Droid Sans, MotoyaLMaru, MotoyaLCedar, Noto Sans JP, Noto Sans CJK JP, Droid Sans Japanese");
  pref("font.name-list.monospace.ja", "MotoyaLMaru, MotoyaLCedar, Noto Sans Mono CJK JP, Droid Sans Mono");

  pref("font.name-list.serif.ko", "Charis SIL Compact, Noto Serif CJK KR, Noto Serif, Droid Serif, HYSerif");
  pref("font.name-list.sans-serif.ko", "Roboto, Google Sans, SmartGothic, NanumGothic, Noto Sans KR, Noto Sans CJK KR, DroidSansFallback, Droid Sans Fallback");
  pref("font.name-list.monospace.ko", "Droid Sans Mono, Noto Sans Mono CJK KR");

  pref("font.name-list.serif.th", "Charis SIL Compact, Noto Serif, Noto Serif Thai, Droid Serif");
  pref("font.name-list.sans-serif.th", "Roboto, Google Sans, Noto Sans Thai, Droid Sans Thai, Droid Sans");
  pref("font.name-list.monospace.th", "Droid Sans Mono");

  pref("font.name-list.serif.x-armn", "Noto Serif Armenian");
  pref("font.name-list.sans-serif.x-armn", "Noto Sans Armenian");

  pref("font.name-list.serif.x-beng", "Noto Serif Bengali");
  pref("font.name-list.sans-serif.x-beng", "Noto Sans Bengali");

  pref("font.name-list.serif.x-cyrillic", "Charis SIL Compact, Noto Serif, Droid Serif");
  pref("font.name-list.sans-serif.x-cyrillic", "Roboto, Google Sans, Droid Sans");
  pref("font.name-list.monospace.x-cyrillic", "Droid Sans Mono");

  pref("font.name-list.serif.x-devanagari", "Noto Serif Devanagari");
  pref("font.name-list.sans-serif.x-devanagari", "Noto Sans Devanagari");

  pref("font.name-list.serif.x-ethi", "Noto Serif Ethiopic");
  pref("font.name-list.sans-serif.x-ethi", "Noto Sans Ethiopic");

  pref("font.name-list.serif.x-geor", "Noto Serif Georgian");
  pref("font.name-list.sans-serif.x-geor", "Noto Sans Georgian");

  pref("font.name-list.serif.x-gujr", "Noto Serif Gujarati");
  pref("font.name-list.sans-serif.x-gujr", "Noto Sans Gujarati");

  pref("font.name-list.serif.x-guru", "Noto Serif Gurmukhi");
  pref("font.name-list.sans-serif.x-guru", "Noto Sans Gurmukhi");

  pref("font.name-list.serif.x-khmr", "Noto Serif Khmer");
  pref("font.name-list.sans-serif.x-khmr", "Noto Sans Khmer");

  pref("font.name-list.serif.x-knda", "Noto Serif Kannada");
  pref("font.name-list.sans-serif.x-knda", "Noto Sans Kannada");

  pref("font.name-list.serif.x-mlym", "Noto Serif Malayalam");
  pref("font.name-list.sans-serif.x-mlym", "Noto Sans Malayalam");

  pref("font.name-list.sans-serif.x-orya", "Noto Sans Oriya");

  pref("font.name-list.serif.x-sinh", "Noto Serif Sinhala");
  pref("font.name-list.sans-serif.x-sinh", "Noto Sans Sinhala");

  pref("font.name-list.serif.x-tamil", "Noto Serif Tamil");
  pref("font.name-list.sans-serif.x-tamil", "Noto Sans Tamil");

  pref("font.name-list.serif.x-telu", "Noto Serif Telugu");
  pref("font.name-list.sans-serif.x-telu", "Noto Sans Telugu");

  pref("font.name-list.serif.x-tibt", "Noto Serif Tibetan");
  pref("font.name-list.sans-serif.x-tibt", "Noto Sans Tibetan");

  pref("font.name-list.serif.x-unicode", "Charis SIL Compact, Noto Serif, Droid Serif");
  pref("font.name-list.sans-serif.x-unicode", "Roboto, Google Sans, Droid Sans");
  pref("font.name-list.monospace.x-unicode", "Droid Sans Mono");

  pref("font.name-list.serif.x-western", "Charis SIL Compact, Noto Serif, Droid Serif");
  pref("font.name-list.sans-serif.x-western", "Roboto, Google Sans, Droid Sans");
  pref("font.name-list.monospace.x-western", "Droid Sans Mono");

  pref("font.name-list.serif.zh-CN", "Charis SIL Compact, Noto Serif CJK SC, Noto Serif, Droid Serif, Droid Sans Fallback");
  pref("font.name-list.sans-serif.zh-CN", "Roboto, Google Sans, Droid Sans, Noto Sans SC, Noto Sans CJK SC, Droid Sans Fallback");
  pref("font.name-list.monospace.zh-CN", "Droid Sans Mono, Noto Sans Mono CJK SC, Droid Sans Fallback");

  pref("font.name-list.serif.zh-HK", "Charis SIL Compact, Noto Serif CJK TC, Noto Serif, Droid Serif, Droid Sans Fallback");
  pref("font.name-list.sans-serif.zh-HK", "Roboto, Google Sans, Droid Sans, Noto Sans TC, Noto Sans SC, Noto Sans CJK TC, Droid Sans Fallback");
  pref("font.name-list.monospace.zh-HK", "Droid Sans Mono, Noto Sans Mono CJK TC, Droid Sans Fallback");

  pref("font.name-list.serif.zh-TW", "Charis SIL Compact, Noto Serif CJK TC, Noto Serif, Droid Serif, Droid Sans Fallback");
  pref("font.name-list.sans-serif.zh-TW", "Roboto, Google Sans, Droid Sans, Noto Sans TC, Noto Sans SC, Noto Sans CJK TC, Droid Sans Fallback");
  pref("font.name-list.monospace.zh-TW", "Droid Sans Mono, Noto Sans Mono CJK TC, Droid Sans Fallback");

  pref("font.name-list.serif.x-math", "Latin Modern Math, STIX Two Math, XITS Math, Cambria Math, Libertinus Math, DejaVu Math TeX Gyre, TeX Gyre Bonum Math, TeX Gyre Pagella Math, TeX Gyre Schola, TeX Gyre Termes Math, STIX Math, Asana Math, STIXGeneral, DejaVu Serif, DejaVu Sans, Charis SIL Compact");
  pref("font.name-list.sans-serif.x-math", "Roboto, Google Sans");
  pref("font.name-list.monospace.x-math", "Droid Sans Mono");

#endif

#if OS_ARCH==AIX

  // Override default Japanese fonts
  pref("font.name-list.serif.ja", "dt-interface system-jisx0208.1983-0");
  pref("font.name-list.sans-serif.ja", "dt-interface system-jisx0208.1983-0");
  pref("font.name-list.monospace.ja", "dt-interface user-jisx0208.1983-0");

  // Override default Cyrillic fonts
  pref("font.name-list.serif.x-cyrillic", "dt-interface system-iso8859-5");
  pref("font.name-list.sans-serif.x-cyrillic", "dt-interface system-iso8859-5");
  pref("font.name-list.monospace.x-cyrillic", "dt-interface user-iso8859-5");

  // Override default Unicode fonts
  pref("font.name-list.serif.x-unicode", "dt-interface system-ucs2.cjk_japan-0");
  pref("font.name-list.sans-serif.x-unicode", "dt-interface system-ucs2.cjk_japan-0");
  pref("font.name-list.monospace.x-unicode", "dt-interface user-ucs2.cjk_japan-0");

#endif // AIX

// Login Manager prefs
pref("signon.rememberSignons",              true);
pref("signon.rememberSignons.visibilityToggle", true);
pref("signon.autofillForms",                true);
pref("signon.autofillForms.autocompleteOff", true);
pref("signon.autofillForms.http",           false);
pref("signon.autologin.proxy",              false);
pref("signon.formlessCapture.enabled",      true);
pref("signon.generation.available",         false);
pref("signon.generation.enabled",           false);
pref("signon.privateBrowsingCapture.enabled", false);
pref("signon.storeWhenAutocompleteOff",     true);
pref("signon.debug",                        false);
pref("signon.recipes.path",                 "chrome://passwordmgr/content/recipes.json");
pref("signon.schemeUpgrades",               false);
// This temporarily prevents the master password to reprompt for autocomplete.
pref("signon.masterPasswordReprompt.timeout_ms", 900000); // 15 Minutes
pref("signon.showAutoCompleteFooter", false);
pref("signon.showAutoCompleteOrigins", false);

// Satchel (Form Manager) prefs
pref("browser.formfill.debug",            false);
pref("browser.formfill.enable",           true);
pref("browser.formfill.expire_days",      180);
pref("browser.formfill.agedWeight",       2);
pref("browser.formfill.bucketSize",       1);
pref("browser.formfill.maxTimeGroupings", 25);
pref("browser.formfill.timeGroupingSize", 604800);
pref("browser.formfill.boundaryWeight",   25);
pref("browser.formfill.prefixWeight",     5);

// Zoom prefs
pref("browser.zoom.full", false);
pref("toolkit.zoomManager.zoomValues", ".3,.5,.67,.8,.9,1,1.1,1.2,1.33,1.5,1.7,2,2.4,3,4,5,7,8,9,10");

//
// Image-related prefs
//

// The default Accept header sent for images loaded over HTTP(S)
pref("image.http.accept", "");

//
// Image memory management prefs
//

// Allows image locking of decoded image data in content processes.
pref("image.mem.allow_locking_in_content_processes", true);

pref("webgl.enable-debug-renderer-info", true);
pref("webgl.renderer-string-override", "");
pref("webgl.vendor-string-override", "");

// sendbuffer of 0 means use OS default, sendbuffer unset means use
// gecko default which varies depending on windows version and is OS
// default on non windows
// pref("network.tcp.sendbuffer", 0);

// TCP Keepalive
pref("network.tcp.keepalive.enabled", true);
// Default idle time before first TCP keepalive probe; same time for interval
// between successful probes. Can be overridden in socket transport API.
// Win, Linux and Mac.
pref("network.tcp.keepalive.idle_time", 600); // seconds; 10 mins
// Default timeout for retransmission of unack'd keepalive probes.
// Win and Linux only; not configurable on Mac.
#if defined(XP_UNIX) && !defined(XP_MACOSX) || defined(XP_WIN)
  pref("network.tcp.keepalive.retry_interval", 1); // seconds
#endif
// Default maximum probe retransmissions.
// Linux only; not configurable on Win and Mac; fixed at 10 and 8 respectively.
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  pref("network.tcp.keepalive.probe_count", 4);
#endif

pref("network.tcp.tcp_fastopen_enable", false);

pref("network.tcp.tcp_fastopen_consecutive_failure_limit", 5);
// We are trying to detect stalled tcp connections that use TFO and TLS
// (bug 1395494).
// This is only happening if a connection is idle for more than 10s, but we
// will make this a pref. If tcp_fastopen_http_stalls_limit of stalls are
// detected the TCP fast open will be disabled.
// If tcp_fastopen_http_check_for_stalls_only_if_idle_for is set to 0 the
// check will not be performed.
pref("network.tcp.tcp_fastopen_http_check_for_stalls_only_if_idle_for", 10);
pref("network.tcp.tcp_fastopen_http_stalls_limit", 3);
pref("network.tcp.tcp_fastopen_http_stalls_timeout", 20);

#ifdef MOZ_X11
  #ifdef MOZ_WIDGET_GTK
    pref("gfx.xrender.enabled",false);
    pref("widget.content.allow-gtk-dark-theme", false);
  #endif
#endif

// Timeout for outbound network geolocation provider XHR
pref("geo.wifi.xhr.timeout", 60000);

// Enable/Disable the device storage API for content
pref("device.storage.enabled", false);

// Push/Pop/Replace State prefs
pref("browser.history.maxStateObjectSize", 2097152);

pref("browser.meta_refresh_when_inactive.disabled", false);

// XPInstall prefs
pref("xpinstall.whitelist.required", true);
pref("extensions.webExtensionsMinPlatformVersion", "42.0a1");

// Other webextensions prefs
pref("extensions.webextensions.keepStorageOnUninstall", false);
pref("extensions.webextensions.keepUuidOnUninstall", false);
// Redirect basedomain used by identity api
pref("extensions.webextensions.identity.redirectDomain", "extensions.allizom.org");

// Whether or not the moz-extension resource loads are remoted. For debugging
// purposes only. Setting this to false will break moz-extension URI loading
// unless other process sandboxing and extension remoting prefs are changed.
pref("extensions.webextensions.protocol.remote", true);

// Enable tab hiding API by default.
pref("extensions.webextensions.tabhide.enabled", true);

// Enable userScripts API by default.
pref("extensions.webextensions.userScripts.enabled", true);

pref("extensions.webextensions.background-delayed-startup", false);

// Whether or not the installed extensions should be migrated to the storage.local IndexedDB backend.
pref("extensions.webextensions.ExtensionStorageIDB.enabled", true);

// if enabled, store execution times for API calls
pref("extensions.webextensions.enablePerformanceCounters", true);

// Maximum age in milliseconds of performance counters in children
// When reached, the counters are sent to the main process and
// reset, so we reduce memory footprint.
pref("extensions.webextensions.performanceCountersMaxAge", 5000);

// Whether to allow the inline options browser in HTML about:addons page.
pref("extensions.htmlaboutaddons.inline-options.enabled", true);
// Show recommendations on the extension and theme list views.
pref("extensions.htmlaboutaddons.recommendations.enabled", true);

pref("network.buffer.cache.count", 24);
pref("network.buffer.cache.size",  32768);

// Web Notification
pref("dom.webnotifications.requireinteraction.count", 3);

// Show favicons in web notifications.
pref("alerts.showFavicons", false);

// Whether to use platform-specific backends for showing desktop notifications.
// If no such backend is available, or if the pref is false, then XUL
// notifications are used.

// Linux and macOS turn on system level notification as default, but Windows is
// Nightly only due to unstable yet.
#if defined(XP_WIN)
  #if defined(NIGHTLY_BUILD)
    pref("alerts.useSystemBackend", true);
  #else
    pref("alerts.useSystemBackend", false);
  #endif
#else
  pref("alerts.useSystemBackend", true);
#endif

// DOM full-screen API.
// whether to prevent the top level widget from going fullscreen
pref("full-screen-api.ignore-widgets", false);
pref("full-screen-api.pointer-lock.enabled", true);
// transition duration of fade-to-black and fade-from-black, unit: ms
#ifndef MOZ_WIDGET_GTK
  pref("full-screen-api.transition-duration.enter", "0 0");
  pref("full-screen-api.transition-duration.leave", "0 0");
#else
  pref("full-screen-api.transition-duration.enter", "0 0");
  pref("full-screen-api.transition-duration.leave", "0 0");
#endif
// timeout for black screen in fullscreen transition, unit: ms
pref("full-screen-api.transition.timeout", 0);
// time for the warning box stays on the screen before sliding out, unit: ms
pref("full-screen-api.warning.timeout", 3000);
// delay for the warning box to show when pointer stays on the top, unit: ms
pref("full-screen-api.warning.delay", 500);

// DOM pointerlock API
// time for the warning box stays on the screen before sliding out, unit: ms
pref("pointer-lock-api.warning.timeout", 3000);

// Push

pref("dom.push.loglevel", "Error");

pref("dom.push.serverURL", "wss://push.services.mozilla.com/");
pref("dom.push.userAgentID", "");

// The maximum number of push messages that a service worker can receive
// without user interaction.
pref("dom.push.maxQuotaPerSubscription", 16);

// The maximum number of recent message IDs to store for each push
// subscription, to avoid duplicates for unacknowledged messages.
pref("dom.push.maxRecentMessageIDsPerSubscription", 10);

// The delay between receiving a push message and updating the quota for a
// subscription.
pref("dom.push.quotaUpdateDelay", 3000); // 3 seconds

// Is the network connection allowed to be up?
// This preference should be used in UX to enable/disable push.
pref("dom.push.connection.enabled", true);

// Exponential back-off start is 5 seconds like in HTTP/1.1.
// Maximum back-off is pingInterval.
pref("dom.push.retryBaseInterval", 5000);

// Interval at which to ping PushServer to check connection status. In
// milliseconds. If no reply is received within requestTimeout, the connection
// is considered closed.
pref("dom.push.pingInterval", 1800000); // 30 minutes

// How long before we timeout
pref("dom.push.requestTimeout", 10000);

// WebPush prefs:
pref("dom.push.http2.reset_retry_count_after_ms", 60000);
pref("dom.push.http2.maxRetries", 2);
pref("dom.push.http2.retryInterval", 5000);

// W3C touch events
// 0 - disabled, 1 - enabled, 2 - autodetect
// Autodetection is currently only supported on Windows and GTK3
#if defined(XP_MACOSX)
  pref("dom.w3c_touch_events.enabled", 0);
#else
  pref("dom.w3c_touch_events.enabled", 2);
#endif

// W3C MediaDevices devicechange fake event
pref("media.ondevicechange.fakeDeviceChangeEvent.enabled", false);

// How long must we wait before declaring that a window is a "ghost" (i.e., a
// likely leak)?  This should be longer than it usually takes for an eligible
// window to be collected via the GC/CC.
pref("memory.ghost_window_timeout_seconds", 60);

// Don't dump memory reports on OOM, by default.
pref("memory.dump_reports_on_oom", false);

// Number of stack frames to capture in createObjectURL for about:memory.
pref("memory.blob_report.stack_frames", 0);

// Activates the activity monitor
pref("io.activity.enabled", false);

// path to OSVR DLLs
pref("gfx.vr.osvr.utilLibPath", "");
pref("gfx.vr.osvr.commonLibPath", "");
pref("gfx.vr.osvr.clientLibPath", "");
pref("gfx.vr.osvr.clientKitLibPath", "");

// nsMemoryInfoDumper can watch a fifo in the temp directory and take various
// actions when the fifo is written to.  Disable this in general.
pref("memory_info_dumper.watch_fifo.enabled", false);

// If minInterval is 0, the check will only happen
// when the service has a strong suspicion we are in a captive portal
pref("network.captive-portal-service.minInterval", 60000); // 60 seconds
pref("network.captive-portal-service.maxInterval", 1500000); // 25 minutes
// Every 10 checks, the delay is increased by a factor of 5
pref("network.captive-portal-service.backoffFactor", "5.0");
pref("network.captive-portal-service.enabled", false);

// DNS Trusted Recursive Resolver
// 0 - default off, 1 - reserved/off, 2 - TRR first, 3 - TRR only, 4 - reserved/off, 5 off by choice
pref("network.trr.mode", 0);
// DNS-over-HTTP service to use, must be HTTPS://
pref("network.trr.uri", "data:text/plain,");
// DNS-over-HTTP service options, must be HTTPS://
pref("network.trr.resolvers", "[{ \"name\": \"NextDNS\", \"url\": \"https://firefox.dns.nextdns.io/\" }]");
// credentials to pass to DOH end-point
pref("network.trr.credentials", "");
pref("network.trr.custom_uri", "");
// Wait for captive portal confirmation before enabling TRR
#if defined(ANDROID)
  // On Android, the captive portal is handled by the OS itself
  pref("network.trr.wait-for-portal", false);
#else
  pref("network.trr.wait-for-portal", false);
#endif
// Allow RFC1918 address in responses?
pref("network.trr.allow-rfc1918", false);
// Use GET (rather than POST)
pref("network.trr.useGET", false);
// Before TRR is widely used the NS record for this host is fetched
// from the DOH end point to ensure proper configuration
pref("network.trr.confirmationNS", "example.com");
// hardcode the resolution of the hostname in network.trr.uri instead of
// relying on the system resolver to do it for you
pref("network.trr.bootstrapAddress", "");
// TRR blacklist entry expire time (in seconds). Default is one minute.
// Meant to survive basically a page load.
pref("network.trr.blacklist-duration", 60);
// Allow AAAA entries to be used "early", before the A results are in
pref("network.trr.early-AAAA", false);
// When true, it only sends AAAA when the system has IPv6 connectivity
pref("network.trr.skip-AAAA-when-not-supported", true);
// When true, the DNS request will wait for both A and AAAA responses
// (if both have been requested) before notifying the listeners.
// When true, it effectively cancels `network.trr.early-AAAA`
pref("network.trr.wait-for-A-and-AAAA", true);
// Explicitly disable ECS (EDNS Client Subnet, RFC 7871)
pref("network.trr.disable-ECS", true);
// After this many failed TRR requests in a row, consider TRR borked
pref("network.trr.max-fails", 5);
// Comma separated list of domains that we should not use TRR for
pref("network.trr.excluded-domains", "");
pref("network.trr.builtin-excluded-domains", "localhost,local");
// When true, the DNS+TRR cache will be cleared when a relevant TRR pref
// changes. (uri, bootstrapAddress, excluded-domains)
pref("network.trr.clear-cache-on-pref-change", true);

pref("captivedetect.canonicalURL", "http://detectportal.firefox.com/success.txt");
pref("captivedetect.canonicalContent", "success\n");
pref("captivedetect.maxWaitingTime", 5000);
pref("captivedetect.pollingTime", 3000);
pref("captivedetect.maxRetryCount", 5);

// The tables used for Safebrowsing phishing and malware checks
pref("urlclassifier.malwareTable", "goog-malware-proto,goog-unwanted-proto,test-harmful-simple,test-malware-simple,test-unwanted-simple");
#ifdef MOZILLA_OFFICIAL
  // In official builds, we are allowed to use Google's private phishing
  // list (see bug 1288840).
  pref("urlclassifier.phishTable", "goog-phish-proto,test-phish-simple");
#else
  pref("urlclassifier.phishTable", "googpub-phish-proto,test-phish-simple");
#endif

// Tables for application reputation
pref("urlclassifier.downloadAllowTable", "goog-downloadwhite-proto");
pref("urlclassifier.downloadBlockTable", "goog-badbinurl-proto");

// Tables for login reputation
pref("urlclassifier.passwordAllowTable", "goog-passwordwhite-proto");

// Tables for anti-tracking features
pref("urlclassifier.trackingAnnotationTable", "test-track-simple,ads-track-digest256,social-track-digest256,analytics-track-digest256,content-track-digest256");
pref("urlclassifier.trackingAnnotationWhitelistTable", "test-trackwhite-simple,mozstd-trackwhite-digest256");
pref("urlclassifier.trackingTable", "test-track-simple,base-track-digest256");
pref("urlclassifier.trackingWhitelistTable", "test-trackwhite-simple,mozstd-trackwhite-digest256");

pref("urlclassifier.features.fingerprinting.blacklistTables", "base-fingerprinting-track-digest256");
pref("urlclassifier.features.fingerprinting.whitelistTables", "mozstd-trackwhite-digest256");
pref("urlclassifier.features.cryptomining.blacklistTables", "base-cryptomining-track-digest256");
pref("urlclassifier.features.cryptomining.whitelistTables", "mozstd-trackwhite-digest256");

// These tables will never trigger a gethash call.
pref("urlclassifier.disallow_completions", "goog-downloadwhite-digest256,base-track-digest256,mozstd-trackwhite-digest256,content-track-digest256,mozplugin-block-digest256,mozplugin2-block-digest256,block-flash-digest256,except-flash-digest256,allow-flashallow-digest256,except-flashallow-digest256,block-flashsubdoc-digest256,except-flashsubdoc-digest256,goog-passwordwhite-proto,ads-track-digest256,social-track-digest256,analytics-track-digest256,base-fingerprinting-track-digest256,content-fingerprinting-track-digest256,base-cryptomining-track-digest256,content-cryptomining-track-digest256,fanboyannoyance-ads-digest256,fanboysocial-ads-digest256,easylist-ads-digest256,easyprivacy-ads-digest256,adguard-ads-digest256");

// Workaround for Google Recaptcha
pref("urlclassifier.trackingAnnotationSkipURLs", "google.com/recaptcha/,*.google.com/recaptcha/");

// Number of random entries to send with a gethash request
pref("urlclassifier.gethashnoise", 4);

// Gethash timeout for Safe Browsing
pref("urlclassifier.gethash.timeout_ms", 5000);
// Update server response timeout for Safe Browsing
pref("urlclassifier.update.response_timeout_ms", 30000);
// Download update timeout for Safe Browsing
pref("urlclassifier.update.timeout_ms", 90000);

// Name of the about: page to display Safe Browsing warnings (bug 399233)
pref("urlclassifier.alternate_error_page", "blocked");

// Enable safe-browsing debugging
pref("browser.safebrowsing.debug", false);

// Allow users to ignore Safe Browsing warnings.
pref("browser.safebrowsing.allowOverride", true);

// These names are approved by the Google Safe Browsing team.
// Any changes must be coordinated with them.
#ifdef MOZILLA_OFFICIAL
  pref("browser.safebrowsing.id", "navclient-auto-ffox");
#else
  pref("browser.safebrowsing.id", "Firefox");
#endif

// Download protection
pref("browser.safebrowsing.downloads.enabled", false);
pref("browser.safebrowsing.downloads.remote.enabled", false);
pref("browser.safebrowsing.downloads.remote.timeout_ms", 15000);
pref("browser.safebrowsing.downloads.remote.url", "");
pref("browser.safebrowsing.downloads.remote.block_dangerous",            true);
pref("browser.safebrowsing.downloads.remote.block_dangerous_host",       true);
pref("browser.safebrowsing.downloads.remote.block_potentially_unwanted", true);
pref("browser.safebrowsing.downloads.remote.block_uncommon",             true);

// Google Safe Browsing provider (legacy)
pref("browser.safebrowsing.provider.google.pver", "2.2");
pref("browser.safebrowsing.provider.google.lists", "");
pref("browser.safebrowsing.provider.google.updateURL", "");
pref("browser.safebrowsing.provider.google.gethashURL", "");
pref("browser.safebrowsing.provider.google.reportURL", "");
pref("browser.safebrowsing.provider.google.reportPhishMistakeURL", "");
pref("browser.safebrowsing.provider.google.reportMalwareMistakeURL", "");
pref("browser.safebrowsing.provider.google.advisoryURL", "");
pref("browser.safebrowsing.provider.google.advisoryName", "");

// Google Safe Browsing provider
pref("browser.safebrowsing.provider.google4.pver", "4");
pref("browser.safebrowsing.provider.google4.lists", "");
pref("browser.safebrowsing.provider.google4.updateURL", "");
pref("browser.safebrowsing.provider.google4.gethashURL", "");
pref("browser.safebrowsing.provider.google4.reportURL", "");
pref("browser.safebrowsing.provider.google4.reportPhishMistakeURL", "");
pref("browser.safebrowsing.provider.google4.reportMalwareMistakeURL", "");
pref("browser.safebrowsing.provider.google4.advisoryURL", "");
pref("browser.safebrowsing.provider.google4.advisoryName", "");
pref("browser.safebrowsing.provider.google4.dataSharingURL", "");
pref("browser.safebrowsing.provider.google4.dataSharing.enabled", false);

pref("browser.safebrowsing.reportPhishURL", "");

// Mozilla Safe Browsing provider (for tracking protection and plugin blocking)
pref("browser.safebrowsing.provider.mozilla.pver", "2.2");
pref("browser.safebrowsing.provider.mozilla.lists", "");
pref("browser.safebrowsing.provider.mozilla.updateURL", "");
pref("browser.safebrowsing.provider.mozilla.gethashURL", "");
// Set to a date in the past to force immediate download in new profiles.
pref("browser.safebrowsing.provider.mozilla.nextupdatetime", "1");
// Block lists for tracking protection. The name values will be used as the keys
// to lookup the localized name in preferences.properties.
pref("browser.safebrowsing.provider.mozilla.lists.base", "moz-std");
pref("browser.safebrowsing.provider.mozilla.lists.content", "moz-full");

// The table and global pref for blocking plugin content
pref("urlclassifier.blockedTable", "test-block-simple,mozplugin-block-digest256");

// Flash blocking tables
pref("urlclassifier.flashAllowTable", "allow-flashallow-digest256");
pref("urlclassifier.flashAllowExceptTable", "except-flashallow-digest256");
pref("urlclassifier.flashTable", "block-flash-digest256");
pref("urlclassifier.flashExceptTable", "except-flash-digest256");
pref("urlclassifier.flashSubDocTable", "block-flashsubdoc-digest256");
pref("urlclassifier.flashSubDocExceptTable", "except-flashsubdoc-digest256");

// Turn off Spatial navigation by default.
pref("snav.enabled", false);

// Wakelock is disabled by default.
pref("dom.wakelock.enabled", false);

// The URL of the Firefox Accounts auth server backend
pref("identity.fxaccounts.auth.uri", "");

// Presentation Device
pref("dom.presentation.tcp_server.debug", false);
pref("dom.presentation.discovery.enabled", false);
pref("dom.presentation.discovery.timeout_ms", 10000);
pref("dom.presentation.discoverable", false);
pref("dom.presentation.discoverable.encrypted", true);
pref("dom.presentation.discoverable.retry_ms", 5000);
pref("dom.presentation.session_transport.data_channel.enable", false);

#ifdef XP_MACOSX
  #if !defined(RELEASE_OR_BETA) || defined(DEBUG)
    // In non-release builds we crash by default on insecure text input (when a
    // password editor has focus but secure event input isn't enabled).  The
    // following pref, when turned on, disables this behavior.  See bug 1188425.
    pref("intl.allow-insecure-text-input", false);
  #endif
#endif // XP_MACOSX

// Search service settings
pref("browser.search.log", false);
pref("browser.search.suggest.enabled", false);
pref("browser.search.geoSpecificDefaults", false);
pref("browser.search.geoip.url", "");
pref("browser.search.geoip.timeout", 3000);

#ifdef MOZ_OFFICIAL_BRANDING
  // {moz:official} expands to "official"
  pref("browser.search.official", true);
#endif

// GMPInstallManager prefs

// User-settable override to media.gmp-manager.url for testing purposes.
//pref("media.gmp-manager.url.override", "");

// Update service URL for GMP install/updates:
pref("media.gmp-manager.url", "");

// When |media.gmp-manager.cert.requireBuiltIn| is true or not specified the
// final certificate and all certificates the connection is redirected to before
// the final certificate for the url specified in the |media.gmp-manager.url|
// preference must be built-in.
pref("media.gmp-manager.cert.requireBuiltIn", true);

// The |media.gmp-manager.certs.| preference branch contains branches that are
// sequentially numbered starting at 1 that contain attribute name / value
// pairs for the certificate used by the server that hosts the update xml file
// as specified in the |media.gmp-manager.url| preference. When these preferences are
// present the following conditions apply for a successful update check:
// 1. the uri scheme must be https
// 2. the preference name must exist as an attribute name on the certificate and
//    the value for the name must be the same as the value for the attribute name
//    on the certificate.
// If these conditions aren't met it will be treated the same as when there is
// no update available. This validation will not be performed when the
// |media.gmp-manager.url.override| user preference has been set for testing updates or
// when the |media.gmp-manager.cert.checkAttributes| preference is set to false. Also,
// the |media.gmp-manager.url.override| preference should ONLY be used for testing.
// IMPORTANT! app.update.certs.* prefs should also be updated if these
// are updated.
pref("media.gmp-manager.cert.checkAttributes", false);
pref("media.gmp-manager.certs.1.issuerName", "CN=DigiCert SHA2 Secure Server CA,O=DigiCert Inc,C=US");
pref("media.gmp-manager.certs.1.commonName", "");
pref("media.gmp-manager.certs.2.issuerName", "CN=thawte SSL CA - G2,O=\"thawte, Inc.\",C=US");
pref("media.gmp-manager.certs.2.commonName", "");

// Whether or not to perform reader mode article parsing on page load.
// If this pref is disabled, we will never show a reader mode icon in the toolbar.
pref("reader.parse-on-load.enabled", true);

// After what size document we don't bother running Readability on it
// because it'd slow things down too much
pref("reader.parse-node-limit", 3000);

// Force-enables reader mode parsing, even on low-memory platforms, where it
// is disabled by default.
pref("reader.parse-on-load.force-enabled", false);

// Whether we include full URLs in browser console errors. This is disabled
// by default because some platforms will persist these, leading to privacy issues.
pref("reader.errors.includeURLs", false);

// The default relative font size in reader mode (1-9)
pref("reader.font_size", 5);

// The default relative content width in reader mode (1-9)
pref("reader.content_width", 3);

// The default relative line height in reader mode (1-9)
pref("reader.line_height", 4);

// The default color scheme in reader mode (light, dark, sepia, auto)
// auto = color automatically adjusts according to ambient light level
// (auto only works on platforms where the 'devicelight' event is enabled)
pref("reader.color_scheme", "light");

// Color scheme values available in reader mode UI.
pref("reader.color_scheme.values", "[\"light\",\"dark\",\"sepia\"]");

// The font type in reader (sans-serif, serif)
pref("reader.font_type", "sans-serif");

// Whether or not the user has interacted with the reader mode toolbar.
// This is used to show a first-launch tip in reader mode.
pref("reader.has_used_toolbar", false);

// Whether to use a vertical or horizontal toolbar.
pref("reader.toolbar.vertical", true);

#if !defined(ANDROID)
  pref("narrate.enabled", true);
#else
  pref("narrate.enabled", false);
#endif

pref("narrate.test", false);
pref("narrate.rate", 0);
pref("narrate.voice", " { \"default\": \"automatic\" }");
// Only make voices that match content language available.
pref("narrate.filter-voices", true);

// Allow control characters appear in composition string.
// When this is false, control characters except
// CHARACTER TABULATION (horizontal tab) are removed from
// both composition string and data attribute of compositionupdate
// and compositionend events.
pref("dom.compositionevent.allow_control_characters", false);

pref("memory.report_concurrency", 10);

// Add Mozilla AudioChannel APIs.
pref("media.useAudioChannelAPI", false);

pref("toolkit.pageThumbs.screenSizeDivisor", 7);
pref("toolkit.pageThumbs.minWidth", 0);
pref("toolkit.pageThumbs.minHeight", 0);

pref("webextensions.tests", false);

// 16MB default non-parseable upload limit for requestBody.raw.bytes
pref("webextensions.webRequest.requestBodyMaxRawBytes", 16777216);

pref("webextensions.storage.sync.enabled", false);
pref("webextensions.storage.sync.serverURL", "");

// Allow customization of the fallback directory for file uploads
pref("dom.input.fallbackUploadDir", "");

// Turn rewriting of youtube embeds on/off
pref("plugins.rewrite_youtube_embeds", true);

// Default media volume
pref("media.default_volume", "1.0");

// return the maximum number of cores that navigator.hardwareCurrency returns
pref("dom.maxHardwareConcurrency", 16);

// Shutdown the osfile worker if its no longer needed.
#if !defined(RELEASE_OR_BETA)
  pref("osfile.reset_worker_delay", 30000);
#endif

// If true, all toplevel data: URI navigations will be blocked.
// Please note that manually entering a data: URI in the
// URL-Bar will not be blocked when flipping this pref.
pref("security.data_uri.block_toplevel_data_uri_navigations", true);

pref("dom.storageManager.prompt.testing", false);
pref("dom.storageManager.prompt.testing.allow", false);


pref("browser.storageManager.pressureNotification.minIntervalMS", 1200000);
pref("browser.storageManager.pressureNotification.usageThresholdGB", 5);

pref("browser.sanitizer.loglevel", "Warn");

// When a user cancels this number of authentication dialogs coming from
// a single web page in a row, all following authentication dialogs will
// be blocked (automatically canceled) for that page. The counter resets
// when the page is reloaded.
// To disable all auth prompting, set the limit to 0.
// To disable blocking of auth prompts, set the limit to -1.
pref("prompts.authentication_dialog_abuse_limit", 2);

#ifdef MOZ_ASAN_REPORTER
  pref("asanreporter.apiurl", "");
  pref("asanreporter.clientid", "unknown");
  pref("toolkit.telemetry.overrideUpdateChannel", "nightly-asan");
#endif

// Control whether clients.openWindow() opens windows in the same process
// that called the API vs following our normal multi-process selection
// algorithm.  Restricting openWindow to same process improves service worker
// web compat in the short term.  Once the SW multi-e10s refactor is complete
// this can be removed.
pref("dom.clients.openwindow_favors_same_process", true);

#ifdef RELEASE_OR_BETA
  pref("toolkit.aboutPerformance.showInternals", false);
#else
  pref("toolkit.aboutPerformance.showInternals", true);
#endif

// When a crash happens, whether to include heap regions of the crash context
// in the minidump. Enabled by default on nightly and aurora.
#ifdef RELEASE_OR_BETA
  pref("toolkit.crashreporter.include_context_heap", false);
#else
  pref("toolkit.crashreporter.include_context_heap", true);
#endif

#if defined(XP_WIN) || defined(XP_MACOSX) || defined(MOZ_WIDGET_GTK)
  pref("layers.omtp.enabled", true);
#else
  pref("layers.omtp.enabled", false);
#endif

// Limits the depth of recursive conversion of data when opening
// a content to view.  This is mostly intended to prevent infinite
// loops with faulty converters involved.
pref("general.document_open_conversion_depth_limit", 20);

// Turn off fission frameloader swapping while regressions are being fixed.
// Should be turned back on to resolve bug 1551993.
pref("fission.rebuild_frameloaders_on_remoteness_change", false);

// Support for legacy customizations that rely on checking the
// user profile directory for these stylesheets:
//  * userContent.css
//  * userChrome.css
pref("toolkit.legacyUserProfileCustomizations.stylesheets", false);

// Enable support for XMLDocument.load and .async for webcompat (bug 1569102).
pref("dom.xmldocument.load.enabled", true);
pref("dom.xmldocument.async.enabled", true);

#ifdef MOZ_DATA_REPORTING
  pref("datareporting.policy.dataSubmissionEnabled", false);
  pref("datareporting.policy.dataSubmissionPolicyNotifiedTime", "0");
  pref("datareporting.policy.dataSubmissionPolicyAcceptedVersion", 0);
  pref("datareporting.policy.dataSubmissionPolicyBypassNotification", false);
  pref("datareporting.policy.currentPolicyVersion", 2);
  pref("datareporting.policy.minimumPolicyVersion", 1);
  pref("datareporting.policy.minimumPolicyVersion.channel-beta", 2);
  pref("datareporting.policy.firstRunURL", "");
#endif

#ifdef MOZ_SERVICES_HEALTHREPORT
  #if !defined(ANDROID)
    pref("datareporting.healthreport.infoURL", "");

    // Health Report is enabled by default on all channels.
    pref("datareporting.healthreport.uploadEnabled", false);
  #endif
#endif

pref("services.common.log.logger.rest.request", "Debug");
pref("services.common.log.logger.rest.response", "Debug");
pref("services.common.log.logger.tokenserverclient", "Debug");

#ifdef MOZ_SERVICES_SYNC
  pref("services.sync.lastversion", "firstrun");
  pref("services.sync.sendVersionInfo", true);

  pref("services.sync.scheduler.idleInterval", 3600);  // 1 hour
  pref("services.sync.scheduler.activeInterval", 600);   // 10 minutes
  pref("services.sync.scheduler.immediateInterval", 90);    // 1.5 minutes
  pref("services.sync.scheduler.idleTime", 300);   // 5 minutes

  pref("services.sync.scheduler.fxa.singleDeviceInterval", 3600); // 1 hour

  // Note that new engines are typically added with a default of disabled, so
  // when an existing sync user gets the Firefox upgrade that supports the engine
  // it starts as disabled until the user has explicitly opted in.
  // The sync "create account" process typically *will* offer these engines, so
  // they may be flipped to enabled at that time.
  pref("services.sync.engine.addons", true);
  pref("services.sync.engine.addresses", false);
  pref("services.sync.engine.bookmarks", true);
  #ifdef EARLY_BETA_OR_EARLIER
    // Enable the new bookmark sync engine through early Beta, but not release
    // candidates or Release.
    pref("services.sync.engine.bookmarks.buffer", true);
  #else
    pref("services.sync.engine.bookmarks.buffer", false);
  #endif
  pref("services.sync.engine.creditcards", false);
  pref("services.sync.engine.history", true);
  pref("services.sync.engine.passwords", true);
  pref("services.sync.engine.prefs", true);
  pref("services.sync.engine.tabs", true);
  pref("services.sync.engine.tabs.filteredUrls", "^(about:.*|resource:.*|chrome:.*|wyciwyg:.*|file:.*|blob:.*|moz-extension:.*)$");

  // The addresses and CC engines might not actually be available at all.
  pref("services.sync.engine.addresses.available", false);
  pref("services.sync.engine.creditcards.available", false);

  // If true, add-on sync ignores changes to the user-enabled flag. This
  // allows people to have the same set of add-ons installed across all
  // profiles while maintaining different enabled states.
  pref("services.sync.addons.ignoreUserEnabledChanges", false);

  // Comma-delimited list of hostnames to trust for add-on install.
  pref("services.sync.addons.trustedSourceHostnames", "addons.mozilla.org");

  pref("services.sync.log.appender.console", "Fatal");
  pref("services.sync.log.appender.dump", "Error");
  pref("services.sync.log.appender.file.level", "Trace");
  pref("services.sync.log.appender.file.logOnError", true);
  #if defined(NIGHTLY_BUILD)
    pref("services.sync.log.appender.file.logOnSuccess", true);
  #else
    pref("services.sync.log.appender.file.logOnSuccess", false);
  #endif
  pref("services.sync.log.appender.file.maxErrorAge", 864000); // 10 days

  // The default log level for all "Sync.*" logs. Adjusting this pref will
  // adjust the level for *all* Sync logs (except engines, and that's only
  // because we supply a default for the engines below.)
  pref("services.sync.log.logger", "Debug");

  // Prefs for Sync engines can be controlled globally or per-engine.
  // We only define the global level here, but manually creating prefs
  // like "services.sync.log.logger.engine.bookmarks" will control just
  // that engine.
  pref("services.sync.log.logger.engine", "Debug");
  pref("services.sync.log.cryptoDebug", false);

  pref("services.sync.fxa.termsURL", "https://accounts.firefox.com/legal/terms");
  pref("services.sync.fxa.privacyURL", "https://accounts.firefox.com/legal/privacy");

  pref("services.sync.telemetry.submissionInterval", 43200); // 12 hours in seconds
  pref("services.sync.telemetry.maxPayloadCount", 500);

  #ifdef EARLY_BETA_OR_EARLIER
    // Enable the (fairly costly) client/server validation through early Beta, but
    // not release candidates or Release.
    pref("services.sync.engine.bookmarks.validation.enabled", true);
    pref("services.sync.engine.passwords.validation.enabled", true);
  #endif

  #if defined(NIGHTLY_BUILD)
    // Enable repair of bookmarks on Nightly only - requires validation also be
    // enabled.
    pref("services.sync.engine.bookmarks.repair.enabled", true);
  #endif

  // We consider validation this frequently. After considering validation, even
  // if we don't end up validating, we won't try again unless this much time has passed.
  pref("services.sync.engine.bookmarks.validation.interval", 86400); // 24 hours in seconds
  pref("services.sync.engine.passwords.validation.interval", 86400); // 24 hours in seconds

  // We only run validation `services.sync.validation.percentageChance` percent of
  // the time, even if it's been the right amount of time since the last validation,
  // and you meet the maxRecord checks.
  pref("services.sync.engine.bookmarks.validation.percentageChance", 10);
  pref("services.sync.engine.passwords.validation.percentageChance", 10);

  // We won't validate an engine if it has more than this many records on the server.
  pref("services.sync.engine.bookmarks.validation.maxRecords", 1000);
  pref("services.sync.engine.passwords.validation.maxRecords", 1000);

  // The maximum number of immediate resyncs to trigger for changes made during
  // a sync.
  pref("services.sync.maxResyncs", 5);
#endif // MOZ_SERVICES_SYNC

// Marionette is the remote protocol that lets OOP programs communicate with,
// instrument, and control Gecko.

// Starts and stops the Marionette server.
pref("marionette.enabled", false);

// Delay server startup until a modal dialogue has been clicked to allow time
// for user to set breakpoints in the Browser Toolbox.
pref("marionette.debugging.clicktostart", false);

// Verbosity of Marionette logger repository.
//
// Available levels are, in descending order of severity, "trace", "debug",
// "config", "info", "warn", "error", and "fatal". The value is treated
// case-insensitively.
pref("marionette.log.level", "Info");

// Certain log messages that are known to be long are truncated. This
// preference causes them to not be truncated.
pref("marionette.log.truncate", true);

// Port to start Marionette server on.
pref("marionette.port", 2828);

// Sets recommended automation preferences when Marionette is started.
pref("marionette.prefs.recommended", true);

// Whether content scripts can be safely reused.
//
// Deprecated and scheduled for removal with
// https://bugzil.la/marionette-window-tracking
pref("marionette.contentListener", false);

// Indicates whether the remote agent is enabled. If it is false, the remote
// agent will not be loaded.
pref("remote.enabled", false);

// Limits remote agent to listen on loopback devices, e.g. 127.0.0.1,
// localhost, and ::1.
pref("remote.force-local", true);

// Defines the verbosity of the internal logger.
//
// Available levels are, in descending order of severity, "Trace", "Debug",
// "Config", "Info", "Warn", "Error", and "Fatal". The value is treated
// case-sensitively.
pref("remote.log.level", "Info");

// Enable the JSON View tool (an inspector for application/json documents).
pref("devtools.jsonview.enabled", true);

// Default theme ("dark" or "light").
#ifdef MOZ_DEV_EDITION
  pref("devtools.theme", "dark", sticky);
#else
  pref("devtools.theme", "light", sticky);
#endif

// Completely disable DevTools entry points, as well as all DevTools command
// line arguments This should be merged with devtools.enabled, see Bug 1440675.
pref("devtools.policy.disabled", false);

// Tells if DevTools have been explicitely enabled by the user. This pref
// allows to disable all features related to DevTools for users that never use
// them. Until bug 1361080 lands, we always consider them enabled.
pref("devtools.enabled", true);

// Enable deprecation warnings.
pref("devtools.errorconsole.deprecation_warnings", true);

#ifdef NIGHTLY_BUILD
  // Don't show the Browser Toolbox prompt on local builds / nightly.
  pref("devtools.debugger.prompt-connection", false, sticky);
#else
  pref("devtools.debugger.prompt-connection", true, sticky);
#endif

#ifdef MOZILLA_OFFICIAL
  // Disable debugging chrome.
  pref("devtools.chrome.enabled", false, sticky);
  // Disable remote debugging connections.
  pref("devtools.debugger.remote-enabled", false, sticky);
#else
  // In local builds, enable the browser toolbox by default.
  pref("devtools.chrome.enabled", true, sticky);
  pref("devtools.debugger.remote-enabled", true, sticky);
#endif

// Disable remote debugging protocol logging.
pref("devtools.debugger.log", false);
pref("devtools.debugger.log.verbose", false);

pref("devtools.debugger.remote-port", 6000);
pref("devtools.debugger.remote-websocket", false);
// Force debugger server binding on the loopback interface.
pref("devtools.debugger.force-local", true);

// Limit for intercepted request and response bodies (1 MB).
// Possible values:
// 0 => the response body has no limit
// n => represents max number of bytes stored
pref("devtools.netmonitor.responseBodyLimit", 1048576);
pref("devtools.netmonitor.requestBodyLimit", 1048576);

// Limit for WebSocket frames (100 KB).
pref("devtools.netmonitor.ws.messageDataLimit", 100000);

// DevTools default color unit.
pref("devtools.defaultColorUnit", "authored");

// Used for devtools debugging.
pref("devtools.dump.emit", false);

// Disable device discovery logging.
pref("devtools.discovery.log", false);
// Whether to scan for DevTools devices via WiFi.
pref("devtools.remote.wifi.scan", true);
// Client must complete TLS handshake within this window (ms).
pref("devtools.remote.tls-handshake-timeout", 10000);

// The extension ID for devtools-adb-extension.
pref("devtools.remote.adb.extensionID", "adb@mozilla.org");
// The URL for for devtools-adb-extension (overridden in tests to a local
// path).
pref("devtools.remote.adb.extensionURL", "https://ftp.mozilla.org/pub/mozilla.org/labs/devtools/adb-extension/#OS#/adb-extension-latest-#OS#.xpi");

// URL of the remote JSON catalog used for device simulation.
pref("devtools.devices.url", "https://code.cdn.mozilla.net/devices/devices.json");

// Enable Inactive CSS detection; used both by the client and the server.
pref("devtools.inspector.inactive.css.enabled", true);
