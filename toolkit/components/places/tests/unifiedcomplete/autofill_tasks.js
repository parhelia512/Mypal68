/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../head_common.js */
/* import-globals-from ./head_autocomplete.js */

function addAutofillTasks(origins) {
  // Helpful reminder of the `autofilled` and `completed` properties in the
  // object passed to check_autocomplete:
  //
  //   autofilled: expected input.value after autofill
  //    completed: expected input.value after autofill and enter is pressed
  //
  // `completed` is the URL that the controller sets to input.value, and the URL
  // that will ultimately be loaded when you press enter.  When you press enter,
  // the code path is:
  //
  //   (1) urlbar.handleEnter
  //   (2) nsAutoCompleteController::HandleEnter
  //   (3) nsAutoCompleteController::EnterMatch (sets input.value)
  //   (4) input.onTextEntered
  //   (5) urlbar.handleCommand (loads input.value)

  let path;
  let search;
  let searchCase;
  let comment;
  if (origins) {
    path = "/";
    search = "ex";
    searchCase = "EX";
    comment = "example.com";
  } else {
    path = "/foo";
    search = "example.com/f";
    searchCase = "EXAMPLE.COM/f";
    comment = "example.com/foo";
  }

  let host = "example.com";
  let url = host + path;

  add_task(async function init() {
    await cleanup();
  });

  // "ex" should match http://example.com/.
  add_task(async function basic() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "EX" should match http://example.com/.
  add_task(async function basicCase() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: searchCase,
      autofilled: searchCase + url.substr(searchCase.length),
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "ex" should match http://www.example.com/.
  add_task(async function noWWWShouldMatchWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www." + url,
      },
    ]);
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://www." + url,
      matches: [
        {
          value: url,
          comment: "www." + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "EX" should match http://www.example.com/.
  add_task(async function noWWWShouldMatchWWWCase() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www." + url,
      },
    ]);
    await check_autocomplete({
      search: searchCase,
      autofilled: searchCase + url.substr(searchCase.length),
      completed: "http://www." + url,
      matches: [
        {
          value: url,
          comment: "www." + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "www.ex" should *not* match http://example.com/.
  add_task(async function wwwShouldNotMatchNoWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: "www." + search,
      matches: [],
    });
    await cleanup();
  });

  // "http://ex" should match http://example.com/.
  add_task(async function prefix() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: "http://" + search,
      autofilled: "http://" + url,
      completed: "http://" + url,
      matches: [
        {
          value: "http://" + url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "HTTP://EX" should match http://example.com/.
  add_task(async function prefixCase() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: "HTTP://" + searchCase,
      autofilled: "HTTP://" + searchCase + url.substr(searchCase.length),
      completed: "http://" + url,
      matches: [
        {
          value: "http://" + url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "http://ex" should match http://www.example.com/.
  add_task(async function prefixNoWWWShouldMatchWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www." + url,
      },
    ]);
    await check_autocomplete({
      search: "http://" + search,
      autofilled: "http://" + url,
      completed: "http://www." + url,
      matches: [
        {
          value: "http://" + url,
          comment: "www." + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "HTTP://EX" should match http://www.example.com/.
  add_task(async function prefixNoWWWShouldMatchWWWCase() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://www." + url,
      },
    ]);
    await check_autocomplete({
      search: "HTTP://" + searchCase,
      autofilled: "HTTP://" + searchCase + url.substr(searchCase.length),
      completed: "http://www." + url,
      matches: [
        {
          value: "http://" + url,
          comment: "www." + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "http://www.ex" should *not* match http://example.com/.
  add_task(async function prefixWWWShouldNotMatchNoWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: "http://www." + search,
      matches: [],
    });
    await cleanup();
  });

  // "http://ex" should *not* match https://example.com/.
  add_task(async function httpPrefixShouldNotMatchHTTPS() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://" + url,
      },
    ]);
    await check_autocomplete({
      search: "http://" + search,
      matches: [
        {
          value: "https://" + url,
          comment: "test visit for https://" + url,
          style: ["favicon"],
        },
      ],
    });
    await cleanup();
  });

  // "ex" should match https://example.com/.
  add_task(async function httpsBasic() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://" + url,
      },
    ]);
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://" + url,
      matches: [
        {
          value: url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "ex" should match https://www.example.com/.
  add_task(async function httpsNoWWWShouldMatchWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://www." + url,
      },
    ]);
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://www." + url,
      matches: [
        {
          value: url,
          comment: "https://www." + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "www.ex" should *not* match https://example.com/.
  add_task(async function httpsWWWShouldNotMatchNoWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://" + url,
      },
    ]);
    await check_autocomplete({
      search: "www." + search,
      matches: [],
    });
    await cleanup();
  });

  // "https://ex" should match https://example.com/.
  add_task(async function httpsPrefix() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://" + url,
      },
    ]);
    await check_autocomplete({
      search: "https://" + search,
      autofilled: "https://" + url,
      completed: "https://" + url,
      matches: [
        {
          value: "https://" + url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "https://ex" should match https://www.example.com/.
  add_task(async function httpsPrefixNoWWWShouldMatchWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://www." + url,
      },
    ]);
    await check_autocomplete({
      search: "https://" + search,
      autofilled: "https://" + url,
      completed: "https://www." + url,
      matches: [
        {
          value: "https://" + url,
          comment: "https://www." + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });
    await cleanup();
  });

  // "https://www.ex" should *not* match https://example.com/.
  add_task(async function httpsPrefixWWWShouldNotMatchNoWWW() {
    await PlacesTestUtils.addVisits([
      {
        uri: "https://" + url,
      },
    ]);
    await check_autocomplete({
      search: "https://www." + search,
      matches: [],
    });
    await cleanup();
  });

  // "https://ex" should *not* match http://example.com/.
  add_task(async function httpsPrefixShouldNotMatchHTTP() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: "https://" + search,
      matches: [
        {
          value: "http://" + url,
          comment: "test visit for http://" + url,
          style: ["favicon"],
        },
      ],
    });
    await cleanup();
  });

  // "https://ex" should *not* match http://example.com/, even if the latter is
  // more frecent and both could be autofilled.
  add_task(async function httpsPrefixShouldNotMatchMoreFrecentHTTP() {
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
        transition: PlacesUtils.history.TRANSITIONS.TYPED,
      },
      {
        uri: "http://" + url,
      },
      {
        uri: "https://" + url,
        transition: PlacesUtils.history.TRANSITIONS.TYPED,
      },
      {
        uri: "http://otherpage",
      },
    ]);
    await check_autocomplete({
      search: "https://" + search,
      autofilled: "https://" + url,
      completed: "https://" + url,
      matches: [
        {
          value: "https://" + url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "http://" + url,
          comment: "test visit for http://" + url,
          style: ["favicon"],
        },
      ],
    });
    await cleanup();
  });

  // Autofill should respond to frecency changes.
  add_task(async function frecency() {
    // Start with an http visit.  It should be completed.
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    // Add two https visits.  https should now be completed.
    for (let i = 0; i < 2; i++) {
      await PlacesTestUtils.addVisits([{ uri: "https://" + url }]);
    }
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://" + url,
      matches: [
        {
          value: url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "http://" + url,
          comment: "test visit for http://" + url,
          style: ["favicon"],
        },
      ],
    });

    // Add two more http visits, three total.  http should now be completed
    // again.
    for (let i = 0; i < 2; i++) {
      await PlacesTestUtils.addVisits([{ uri: "http://" + url }]);
    }
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "https://" + url,
          comment: "test visit for https://" + url,
          style: ["favicon"],
        },
      ],
    });

    // Add four www https visits.  www https should now be completed.
    for (let i = 0; i < 4; i++) {
      await PlacesTestUtils.addVisits([{ uri: "https://www." + url }]);
    }
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://www." + url,
      matches: [
        {
          value: url,
          comment: "https://www." + comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "http://" + url,
          comment: "test visit for http://" + url,
          style: ["favicon"],
        },
        {
          value: "https://" + url,
          comment: "test visit for https://" + url,
          style: ["favicon"],
        },
      ],
    });

    // Remove the www https page.
    await PlacesUtils.history.remove(["https://www." + url]);

    // http should now be completed again.
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "https://" + url,
          comment: "test visit for https://" + url,
          style: ["favicon"],
        },
      ],
    });

    // Remove the http page.
    await PlacesUtils.history.remove(["http://" + url]);

    // https should now be completed again.
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://" + url,
      matches: [
        {
          value: url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    // Add a visit with a different host so that "ex" doesn't autofill it.
    // https://example.com/ should still have a higher frecency though, so it
    // should still be autofilled.
    await PlacesTestUtils.addVisits([{ uri: "https://not-" + url }]);
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://" + url,
      matches: [
        {
          value: url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "https://not-" + url,
          comment: "test visit for https://not-" + url,
          style: ["favicon"],
        },
      ],
    });

    // Now add 10 more visits to the different host so that the frecency of
    // https://example.com/ falls below the autofill threshold.  It should not
    // be autofilled now.
    for (let i = 0; i < 10; i++) {
      await PlacesTestUtils.addVisits([{ uri: "https://not-" + url }]);
    }

    // Enable actions.  In the `origins` case, the failure to make an autofill
    // match should not interrupt creating another type of heuristic match, in
    // this case a search (for "ex").  In the `!origins` case, autofill should
    // still happen since there's no threshold comparison.
    if (origins) {
      await check_autocomplete({
        search,
        searchParam: "enable-actions",
        matches: [
          makeSearchMatch(search, { style: ["heuristic"] }),
          {
            value: "https://not-" + url,
            comment: "test visit for https://not-" + url,
            style: ["favicon"],
          },
          {
            value: "https://" + url,
            comment: "test visit for https://" + url,
            style: ["favicon"],
          },
        ],
      });
    } else {
      await check_autocomplete({
        search,
        searchParam: "enable-actions",
        autofilled: url,
        completed: "https://" + url,
        matches: [
          {
            value: url,
            comment: "https://" + comment,
            style: ["autofill", "heuristic"],
          },
          {
            value: "https://not-" + url,
            comment: "test visit for https://not-" + url,
            style: ["favicon"],
          },
        ],
      });
    }

    // Remove the visits to the different host.
    await PlacesUtils.history.remove(["https://not-" + url]);

    // https should be completed again.
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "https://" + url,
      matches: [
        {
          value: url,
          comment: "https://" + comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    // Remove the https visits.
    await PlacesUtils.history.remove(["https://" + url]);

    // Now nothing should be completed.
    await check_autocomplete({
      search,
      matches: [],
    });

    await cleanup();
  });

  // Bookmarked places should always be autofilled, even when they don't meet
  // the threshold.
  add_task(async function bookmarkBelowThreshold() {
    // Add some visits to a URL so that the origin autofill threshold is large.
    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits([
        {
          uri: "http://not-" + url,
        },
      ]);
    }

    // Now bookmark another URL.
    await addBookmark({
      uri: "http://" + url,
    });

    // Make sure the bookmarked origin and place frecencies are below the
    // threshold so that the origin/URL otherwise would not be autofilled.
    let placeFrecency = await PlacesTestUtils.fieldInDB(
      "http://" + url,
      "frecency"
    );
    let originFrecency = await getOriginFrecency("http://", host);
    let threshold = await getOriginAutofillThreshold();
    Assert.ok(
      placeFrecency < threshold,
      `Place frecency should be below the threshold: ` +
        `placeFrecency=${placeFrecency} threshold=${threshold}`
    );
    Assert.ok(
      originFrecency < threshold,
      `Origin frecency should be below the threshold: ` +
        `originFrecency=${originFrecency} threshold=${threshold}`
    );

    // The bookmark should be autofilled.
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
        {
          value: "http://not-" + url,
          comment: "test visit for http://not-" + url,
          style: ["favicon"],
        },
      ],
    });

    await cleanup();
  });

  // Bookmarked places should be autofilled when they *do* meet the threshold.
  add_task(async function bookmarkAboveThreshold() {
    // Bookmark a URL.
    await addBookmark({
      uri: "http://" + url,
    });

    // The frecencies of the place and origin should be >= the threshold.  In
    // fact they should be the same as the threshold since the place is the only
    // place in the database.
    let placeFrecency = await PlacesTestUtils.fieldInDB(
      "http://" + url,
      "frecency"
    );
    let originFrecency = await getOriginFrecency("http://", host);
    let threshold = await getOriginAutofillThreshold();
    Assert.equal(placeFrecency, threshold);
    Assert.equal(originFrecency, threshold);

    // The bookmark should be autofilled.
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    await cleanup();
  });

  // Autofill should respect the browser.urlbar.suggest.history pref -- i.e., it
  // should complete only bookmarked pages when that pref is false.
  add_task(async function suggestHistoryFalse() {
    // Force only bookmarked pages to be suggested and therefore only bookmarked
    // pages to be completed.
    Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);

    // Add a non-bookmarked page.  It should not be suggested or completed.
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search,
      matches: [],
    });

    // Bookmark the page.  It should be suggested and completed.
    await addBookmark({
      uri: "http://" + url,
    });
    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    await cleanup();
  });

  // Same as previous but the search contains a prefix.
  add_task(async function suggestHistoryFalsePrefix() {
    // Force only bookmarked pages to be suggested and therefore only bookmarked
    // pages to be completed.
    Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);

    // Add a non-bookmarked page.  It should not be suggested or completed.
    await PlacesTestUtils.addVisits([
      {
        uri: "http://" + url,
      },
    ]);
    await check_autocomplete({
      search: "http://" + search,
      matches: [],
    });

    // Bookmark the page.  It should be suggested and completed.
    await addBookmark({
      uri: "http://" + url,
    });
    await check_autocomplete({
      search: "http://" + search,
      autofilled: "http://" + url,
      completed: "http://" + url,
      matches: [
        {
          value: "http://" + url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    await cleanup();
  });

  // Bookmark a page and then clear history.  The bookmarked origin/URL should
  // be autofilled even though its frecency is <= 0 since the autofill threshold
  // is 0.
  add_task(async function zeroThreshold() {
    await addBookmark({
      uri: "http://" + url,
    });

    await PlacesUtils.history.clear();

    // Make sure the place's frecency is <= 0.  (It will be reset to -1 on the
    // history.clear() above, and then on idle it will be reset to 0.  xpcshell
    // tests disable the idle service, so in practice it should always be -1,
    // but in order to avoid possible intermittent failures in the future, don't
    // assume that.)
    let placeFrecency = await PlacesTestUtils.fieldInDB(
      "http://" + url,
      "frecency"
    );
    Assert.ok(placeFrecency <= 0);

    // Make sure the origin's frecency is 0.
    let originFrecency = await getOriginFrecency("http://", host);
    Assert.equal(originFrecency, 0);

    // Make sure the autofill threshold is 0.
    let threshold = await getOriginAutofillThreshold();
    Assert.equal(threshold, 0);

    await check_autocomplete({
      search,
      autofilled: url,
      completed: "http://" + url,
      matches: [
        {
          value: url,
          comment,
          style: ["autofill", "heuristic"],
        },
      ],
    });

    await cleanup();
  });
}

/**
 * Returns the frecency of an origin.
 *
 * @param   {string} prefix
 *          The origin's prefix, e.g., "http://".
 * @param   {string} host
 *          The origin's host.
 * @returns {number} The origin's frecency.
 */
async function getOriginFrecency(prefix, host) {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(
    `
    SELECT frecency
    FROM moz_origins
    WHERE prefix = :prefix AND host = :host
  `,
    { prefix, host }
  );
  Assert.equal(rows.length, 1);
  return rows[0].getResultByIndex(0);
}

/**
 * Returns the origin frecency stats.
 *
 * @returns {object}
 *          An object { count, sum, squares }.
 */
async function getOriginFrecencyStats() {
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`
    SELECT
      IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_count'), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum'), 0),
      IFNULL((SELECT value FROM moz_meta WHERE key = 'origin_frecency_sum_of_squares'), 0)
  `);
  let count = rows[0].getResultByIndex(0);
  let sum = rows[0].getResultByIndex(1);
  let squares = rows[0].getResultByIndex(2);
  return { count, sum, squares };
}

/**
 * Returns the origin autofill frecency threshold.
 *
 * @returns {number}
 *          The threshold.
 */
async function getOriginAutofillThreshold() {
  let { count, sum, squares } = await getOriginFrecencyStats();
  if (!count) {
    return 0;
  }
  if (count == 1) {
    return sum;
  }
  let stddevMultiplier = UrlbarPrefs.get("autoFill.stddevMultiplier");
  return (
    sum / count +
    stddevMultiplier * Math.sqrt((squares - (sum * sum) / count) / count)
  );
}
