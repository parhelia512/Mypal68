/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test private browsing mode.

var test_generator = do_run_test();

function run_test() {
  do_test_pending();
  do_run_generator(test_generator);
}

function finish_test() {
  executeSoon(function() {
    test_generator.return();
    do_test_finished();
  });
}

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function* do_run_test() {
  // Set up a profile.
  let profile = do_get_profile();

  // We don't want to have CookieSettings blocking this test.
  Services.prefs.setBoolPref(
    "network.cookieSettings.unblocked_for_testing",
    true
  );

  // Test with cookies enabled.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  // Create URIs pointing to foo.com and bar.com.
  let uri1 = NetUtil.newURI("http://foo.com/foo.html");
  let principal1 = Services.scriptSecurityManager.createContentPrincipal(uri1, {
    privateBrowsingId: 1,
  });
  let uri2 = NetUtil.newURI("http://bar.com/bar.html");
  let principal2 = Services.scriptSecurityManager.createContentPrincipal(uri2, {
    privateBrowsingId: 1,
  });

  // Set a cookie for host 1.
  Services.cookies.setCookieString(uri1, "oh=hai; max-age=1000", null);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri1.host), 1);

  // Enter private browsing mode, set a cookie for host 2, and check the counts.
  var chan1 = make_channel(uri1.spec);
  chan1.QueryInterface(Ci.nsIPrivateBrowsingChannel);
  chan1.setPrivate(true);

  var chan2 = make_channel(uri2.spec);
  chan2.QueryInterface(Ci.nsIPrivateBrowsingChannel);
  chan2.setPrivate(true);

  Services.cookies.setCookieString(uri2, "oh=hai; max-age=1000", chan2);
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal1), "");
  Assert.equal(
    Services.cookiemgr.getCookieStringForPrincipal(principal2),
    "oh=hai"
  );

  // Remove cookies and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal1), "");
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal2), "");

  Services.cookies.setCookieString(uri2, "oh=hai; max-age=1000", chan2);
  Assert.equal(
    Services.cookiemgr.getCookieStringForPrincipal(principal2),
    "oh=hai"
  );

  // Leave private browsing mode and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri2.host), 0);

  // Fake a profile change.
  do_close_profile(test_generator);
  yield;
  do_load_profile();

  // Check that the right cookie persisted.
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri2.host), 0);

  // Enter private browsing mode, set a cookie for host 2, and check the counts.
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal1), "");
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal2), "");
  Services.cookies.setCookieString(uri2, "oh=hai; max-age=1000", chan2);
  Assert.equal(
    Services.cookiemgr.getCookieStringForPrincipal(principal2),
    "oh=hai"
  );

  // Fake a profile change.
  do_close_profile(test_generator);
  yield;
  do_load_profile();

  // We're still in private browsing mode, but should have a new session.
  // Check counts.
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal1), "");
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal2), "");

  // Leave private browsing mode and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri2.host), 0);

  // Enter private browsing mode.

  // Fake a profile change, but wait for async read completion.
  do_close_profile(test_generator);
  yield;
  do_load_profile(test_generator);
  yield;

  // We're still in private browsing mode, but should have a new session.
  // Check counts.
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal1), "");
  Assert.equal(Services.cookiemgr.getCookieStringForPrincipal(principal2), "");

  // Leave private browsing mode and check counts.
  Services.obs.notifyObservers(null, "last-pb-context-exited");
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri1.host), 1);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri2.host), 0);

  finish_test();
}
