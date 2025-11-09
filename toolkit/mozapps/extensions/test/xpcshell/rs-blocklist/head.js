// Appease eslint.
/* import-globals-from ../head_addons.js */

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

{
  let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
  Services.prefs.setBoolPref("extensions.blocklist.useXML", false);
}
