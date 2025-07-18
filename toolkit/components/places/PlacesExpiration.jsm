/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This component handles history and orphans expiration through asynchronous
 * Storage statements.
 * Expiration runs:
 * - At idle, but just once, we stop any other kind of expiration during idle
 *   to preserve batteries in portable devices.
 * - At shutdown, only if the database is dirty, we should still avoid to
 *   expire too heavily on shutdown.
 * - On a repeating timer we expire in small chunks.
 *
 * Expiration algorithm will adapt itself based on:
 * - Memory size of the device.
 * - Status of the database (clean or dirty).
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

// Constants

// Last expiration step should run before the final sync.
const TOPIC_PREF_CHANGED = "nsPref:changed";
const TOPIC_DEBUG_START_EXPIRATION = "places-debug-start-expiration";
const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
const TOPIC_IDLE_BEGIN = "idle";
const TOPIC_IDLE_END = "active";
const TOPIC_IDLE_DAILY = "idle-daily";
const TOPIC_TESTING_MODE = "testing-mode";
const TOPIC_TEST_INTERVAL_CHANGED = "test-interval-changed";

// Branch for all expiration preferences.
const PREF_BRANCH = "places.history.expiration.";

// Max number of unique URIs to retain in history.
// Notice this is a lazy limit.  This means we will start to expire if we will
// go over it, but we won't ensure that we will stop exactly when we reach it,
// instead we will stop after the next expiration step that will bring us
// below it.
// If this preference does not exist or has a negative value, we will calculate
// a limit based on current hardware.
const PREF_MAX_URIS = "max_pages";
const PREF_MAX_URIS_NOTSET = -1; // Use our internally calculated limit.

// We save the current unique URIs limit to this pref, to make it available to
// other components without having to duplicate the full logic.
const PREF_READONLY_CALCULATED_MAX_URIS = "transient_current_max_pages";

// Seconds between each expiration step.
const PREF_INTERVAL_SECONDS = "interval_seconds";
const PREF_INTERVAL_SECONDS_NOTSET = 3 * 60;

// We calculate an optimal database size, based on hardware specs.
// This percentage of memory size is used to protect against calculating a too
// large database size on systems with small memory.
const DATABASE_TO_MEMORY_PERC = 4;
// This percentage of disk size is used to protect against calculating a too
// large database size on disks with tiny quota or available space.
const DATABASE_TO_DISK_PERC = 2;
// Maximum size of the optimal database.  High-end hardware has plenty of
// memory and disk space, but performances don't grow linearly.
const DATABASE_MAX_SIZE = 78643200; // 75 MiB
// If the physical memory size is bogus, fallback to this.
const MEMSIZE_FALLBACK_BYTES = 268435456; // 256 MiB
// If the disk available space is bogus, fallback to this.
const DISKSIZE_FALLBACK_BYTES = 268435456; // 256 MiB

// Max number of entries to expire at each expiration step.
// This value is globally used for different kind of data we expire, can be
// tweaked based on data type.  See below in getBoundStatement.
const EXPIRE_LIMIT_PER_STEP = 6;
// When we run a large expiration step, the above limit is multiplied by this.
const EXPIRE_LIMIT_PER_LARGE_STEP_MULTIPLIER = 10;

// When history is clean or dirty enough we will adapt the expiration algorithm
// to be more lazy or more aggressive.
// This is done acting on the interval between expiration steps and the number
// of expirable items.
// 1. Clean history:
//   We expire at (default interval * EXPIRE_AGGRESSIVITY_MULTIPLIER) the
//   default number of entries.
// 2. Dirty history:
//   We expire at the default interval, but a greater number of entries
//   (default number of entries * EXPIRE_AGGRESSIVITY_MULTIPLIER).
const EXPIRE_AGGRESSIVITY_MULTIPLIER = 3;

// This is the average size in bytes of an URI entry in the database.
// Magic numbers are determined through analysis of the distribution of a ratio
// between number of unique URIs and database size among our users.
// Used as a fall back value when it's not possible to calculate the real value.
const URIENTRY_AVG_SIZE = 700;

// Seconds of idle time before starting a larger expiration step.
// Notice during idle we stop the expiration timer since we don't want to hurt
// stand-by or mobile devices batteries.
const IDLE_TIMEOUT_SECONDS = 5 * 60;

// If the number of pages over history limit is greater than this threshold,
// expiration will be more aggressive, to bring back history to a saner size.
const OVERLIMIT_PAGES_THRESHOLD = 1000;

// Milliseconds in a day.
const MSECS_PER_DAY = 86400000;

// When we expire we can use these limits:
// - SMALL for usual partial expirations, will expire a small chunk.
// - LARGE for idle or shutdown expirations, will expire a large chunk.
// - UNLIMITED will expire all the orphans.
// - DEBUG will use a known limit, passed along with the debug notification.
const LIMIT = {
  SMALL: 0,
  LARGE: 1,
  UNLIMITED: 2,
  DEBUG: 3,
};

// Represents the status of history database.
const STATUS = {
  CLEAN: 0,
  DIRTY: 1,
  UNKNOWN: 2,
};

// Represents actions on which a query will run.
const ACTION = {
  TIMED: 1 << 0, // happens every this._interval
  TIMED_OVERLIMIT: 1 << 1, // like TIMED but only when history is over limits
  SHUTDOWN_DIRTY: 1 << 2, // happens at shutdown for DIRTY state
  IDLE_DIRTY: 1 << 3, // happens on idle for DIRTY state
  IDLE_DAILY: 1 << 4, // happens once a day on idle
  DEBUG: 1 << 5, // happens on TOPIC_DEBUG_START_EXPIRATION
};

// The queries we use to expire.
const EXPIRATION_QUERIES = {
  // Some visits can be expired more often than others, cause they are less
  // useful to the user and can pollute awesomebar results:
  // 1. urls over 255 chars
  // 2. redirect sources and downloads
  // Note: due to the REPLACE option, this should be executed before
  // QUERY_FIND_VISITS_TO_EXPIRE, that has a more complete result.
  QUERY_FIND_EXOTIC_VISITS_TO_EXPIRE: {
    sql: `INSERT INTO expiration_notify (v_id, url, guid, visit_date, reason)
          SELECT v.id, h.url, h.guid, v.visit_date, "exotic"
          FROM moz_historyvisits v
          JOIN moz_places h ON h.id = v.place_id
          WHERE visit_date < strftime('%s','now','localtime','start of day','-60 days','utc') * 1000000
          AND ( LENGTH(h.url) > 255 OR v.visit_type = 7 )
          ORDER BY v.visit_date ASC
          LIMIT :limit_visits`,
    actions:
      ACTION.TIMED_OVERLIMIT |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Finds visits to be expired when history is over the unique pages limit,
  // otherwise will return nothing.
  // This explicitly excludes any visits added in the last 7 days, to protect
  // users with thousands of bookmarks from constantly losing history.
  QUERY_FIND_VISITS_TO_EXPIRE: {
    sql: `INSERT INTO expiration_notify
            (v_id, url, guid, visit_date, expected_results)
          SELECT v.id, h.url, h.guid, v.visit_date, :limit_visits
          FROM moz_historyvisits v
          JOIN moz_places h ON h.id = v.place_id
          WHERE (SELECT COUNT(*) FROM moz_places) > :max_uris
          AND visit_date < strftime('%s','now','localtime','start of day','-7 days','utc') * 1000000
          ORDER BY v.visit_date ASC
          LIMIT :limit_visits`,
    actions:
      ACTION.TIMED_OVERLIMIT |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Removes the previously found visits.
  QUERY_EXPIRE_VISITS: {
    sql: `DELETE FROM moz_historyvisits WHERE id IN (
            SELECT v_id FROM expiration_notify WHERE v_id NOTNULL
          )`,
    actions:
      ACTION.TIMED_OVERLIMIT |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Finds orphan URIs in the database.
  // Notice we won't notify single removed URIs on History.clear(), so we don't
  // run this query in such a case, but just delete URIs.
  // This could run in the middle of adding a visit or bookmark to a new page.
  // In such a case since it is async, could end up expiring the orphan page
  // before it actually gets the new visit or bookmark.
  // Thus, since new pages get frecency -1, we filter on that.
  QUERY_FIND_URIS_TO_EXPIRE: {
    sql: `INSERT INTO expiration_notify (p_id, url, guid, visit_date)
          SELECT h.id, h.url, h.guid, h.last_visit_date
          FROM moz_places h
          LEFT JOIN moz_historyvisits v ON h.id = v.place_id
          WHERE h.last_visit_date IS NULL
            AND h.foreign_count = 0
            AND v.id IS NULL
            AND frecency <> -1
          LIMIT :limit_uris`,
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Expire found URIs from the database.
  QUERY_EXPIRE_URIS: {
    sql: `DELETE FROM moz_places WHERE id IN (
            SELECT p_id FROM expiration_notify WHERE p_id NOTNULL
          ) AND foreign_count = 0 AND last_visit_date ISNULL`,
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Hosts accumulated during the places delete are updated through a trigger
  // (see nsPlacesTriggers.h).
  QUERY_UPDATE_HOSTS: {
    sql: `DELETE FROM moz_updateoriginsdelete_temp`,
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Expire orphan pages from the icons database.
  QUERY_EXPIRE_FAVICONS_PAGES: {
    sql: `DELETE FROM moz_pages_w_icons
          WHERE page_url_hash NOT IN (
            SELECT url_hash FROM moz_places
          )`,
    actions:
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Expire orphan icons from the database.
  QUERY_EXPIRE_FAVICONS: {
    sql: `DELETE FROM moz_icons WHERE id IN (
            SELECT id FROM moz_icons WHERE root = 0
            EXCEPT
            SELECT icon_id FROM moz_icons_to_pages
          )`,
    actions:
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Expire orphan page annotations from the database.
  QUERY_EXPIRE_ANNOS: {
    sql: `DELETE FROM moz_annos WHERE id in (
            SELECT a.id FROM moz_annos a
            LEFT JOIN moz_places h ON a.place_id = h.id
            WHERE h.id IS NULL
            LIMIT :limit_annos
          )`,
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Expire item annos without a corresponding item id.
  QUERY_EXPIRE_ITEMS_ANNOS: {
    sql: `DELETE FROM moz_items_annos WHERE id IN (
            SELECT a.id FROM moz_items_annos a
            LEFT JOIN moz_bookmarks b ON a.item_id = b.id
            WHERE b.id IS NULL
            LIMIT :limit_annos
          )`,
    actions: ACTION.IDLE_DAILY | ACTION.DEBUG,
  },

  // Expire all annotation names without a corresponding annotation.
  QUERY_EXPIRE_ANNO_ATTRIBUTES: {
    sql: `DELETE FROM moz_anno_attributes WHERE id IN (
            SELECT n.id FROM moz_anno_attributes n
            LEFT JOIN moz_annos a ON n.id = a.anno_attribute_id
            LEFT JOIN moz_items_annos t ON n.id = t.anno_attribute_id
            WHERE a.anno_attribute_id IS NULL
              AND t.anno_attribute_id IS NULL
            LIMIT :limit_annos
          )`,
    actions:
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Expire orphan inputhistory.
  QUERY_EXPIRE_INPUTHISTORY: {
    sql: `DELETE FROM moz_inputhistory
          WHERE place_id IN (SELECT p_id FROM expiration_notify)
          AND place_id IN (
            SELECT i.place_id FROM moz_inputhistory i
            LEFT JOIN moz_places h ON h.id = i.place_id
            WHERE h.id IS NULL
            LIMIT :limit_inputhistory
          )`,
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Select entries for notifications.
  // If p_id is set whole_entry = 1, then we have expired the full page.
  // Either p_id or v_id are always set.
  QUERY_SELECT_NOTIFICATIONS: {
    sql: `SELECT url, guid, MAX(visit_date) AS visit_date,
                 MAX(IFNULL(MIN(p_id, 1), MIN(v_id, 0))) AS whole_entry,
                 MAX(expected_results) AS expected_results,
                 (SELECT MAX(visit_date) FROM expiration_notify
                  WHERE reason = "expired" AND url = n.url AND p_id ISNULL
                 ) AS most_recent_expired_visit
          FROM expiration_notify n
          GROUP BY url`,
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },

  // Empty the notifications table.
  QUERY_DELETE_NOTIFICATIONS: {
    sql: "DELETE FROM expiration_notify",
    actions:
      ACTION.TIMED |
      ACTION.TIMED_OVERLIMIT |
      ACTION.SHUTDOWN_DIRTY |
      ACTION.IDLE_DIRTY |
      ACTION.IDLE_DAILY |
      ACTION.DEBUG,
  },
};

/**
 * Sends a bookmarks notification through the given observers.
 *
 * @param observers
 *        array of nsINavBookmarkObserver objects.
 * @param notification
 *        the notification name.
 * @param args
 *        array of arguments to pass to the notification.
 */
function notify(observers, notification, args = []) {
  for (let observer of observers) {
    try {
      observer[notification](...args);
    } catch (ex) {}
  }
}

// nsPlacesExpiration definition

function nsPlacesExpiration() {
  // Smart Getters

  XPCOMUtils.defineLazyGetter(this, "_db", function() {
    let db = PlacesUtils.history.DBConnection;

    // Create the temporary notifications table.
    let stmt = db.createAsyncStatement(
      `CREATE TEMP TABLE expiration_notify (
         id INTEGER PRIMARY KEY
       , v_id INTEGER
       , p_id INTEGER
       , url TEXT NOT NULL
       , guid TEXT NOT NULL
       , visit_date INTEGER
       , expected_results INTEGER NOT NULL DEFAULT 0
       , reason TEXT NOT NULL DEFAULT "expired"
       )`
    );
    stmt.executeAsync();
    stmt.finalize();

    return db;
  });

  XPCOMUtils.defineLazyServiceGetter(
    this,
    "_idle",
    "@mozilla.org/widget/idleservice;1",
    "nsIIdleService"
  );

  this._prefBranch = Services.prefs.getBranch(PREF_BRANCH);

  this._loadPrefsPromise = this._loadPrefs().then(() => {
    // Observe our preferences branch for changes.
    this._prefBranch.addObserver("", this, true);

    // Create our expiration timer.
    this._newTimer();
  }, Cu.reportError);

  // Register topic observers.
  Services.obs.addObserver(this, TOPIC_DEBUG_START_EXPIRATION, true);
  Services.obs.addObserver(this, TOPIC_IDLE_DAILY, true);

  // Block shutdown.
  let shutdownClient = PlacesUtils.history.connectionShutdownClient.jsclient;
  shutdownClient.addBlocker("Places Expiration: shutdown", () => {
    if (this._shuttingDown) {
      return;
    }
    this._shuttingDown = true;
    this.expireOnIdle = false;
    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }
    // If the database is dirty, we want to expire some entries, to speed up
    // the expiration process.
    if (this.status == STATUS.DIRTY) {
      this._expireWithActionAndLimit(ACTION.SHUTDOWN_DIRTY, LIMIT.LARGE);
    }
    this._finalizeInternalStatements();
  });
}

nsPlacesExpiration.prototype = {
  observe: function PEX_observe(aSubject, aTopic, aData) {
    if (this._shuttingDown) {
      return;
    }

    if (aTopic == TOPIC_PREF_CHANGED) {
      this._loadPrefsPromise = this._loadPrefs().then(() => {
        if (aData == PREF_INTERVAL_SECONDS) {
          // Renew the timer with the new interval value.
          this._newTimer();
        }
      }, Cu.reportError);
    } else if (aTopic == TOPIC_DEBUG_START_EXPIRATION) {
      // The passed-in limit is the maximum number of visits to expire when
      // history is over capacity.  Mind to correctly handle the NaN value.
      let limit = parseInt(aData);
      if (limit == -1) {
        // Everything should be expired without any limit.  If history is over
        // capacity then all existing visits will be expired.
        // Should only be used in tests, since may cause dataloss.
        this._expireWithActionAndLimit(ACTION.DEBUG, LIMIT.UNLIMITED);
      } else if (limit > 0) {
        // The number of expired visits is limited by this amount.  It may be
        // used for testing purposes, like checking that limited queries work.
        this._debugLimit = limit;
        this._expireWithActionAndLimit(ACTION.DEBUG, LIMIT.DEBUG);
      } else {
        // Any other value is intended as a 0 limit, that means no visits
        // will be expired.  Even if this doesn't touch visits, it will remove
        // any orphan pages, icons, annotations and similar from the database,
        // so it may be used for cleanup purposes.
        this._debugLimit = -1;
        this._expireWithActionAndLimit(ACTION.DEBUG, LIMIT.DEBUG);
      }
    } else if (aTopic == TOPIC_IDLE_BEGIN) {
      // Stop the expiration timer.  We don't want to keep up expiring on idle
      // to preserve batteries on mobile devices and avoid killing stand-by.
      if (this._timer) {
        this._timer.cancel();
        this._timer = null;
      }
      if (this.expireOnIdle) {
        this._expireWithActionAndLimit(ACTION.IDLE_DIRTY, LIMIT.LARGE);
      }
    } else if (aTopic == TOPIC_IDLE_END) {
      // Restart the expiration timer.
      if (!this._timer) {
        this._newTimer();
      }
    } else if (aTopic == TOPIC_IDLE_DAILY) {
      this._expireWithActionAndLimit(ACTION.IDLE_DAILY, LIMIT.LARGE);
    } else if (aTopic == TOPIC_TESTING_MODE) {
      this._testingMode = true;
    } else if (aTopic == PlacesUtils.TOPIC_INIT_COMPLETE) {
      // Ideally we'd add this observer only when notifications start being
      // triggered. However, that's difficult to work out, so we do it on
      // TOPIC_INIT_COMPLETE which means we have to take the hit of initializing
      // this service slightly earlier.
      PlacesUtils.history.addObserver(this, true);
    }
  },

  // nsINavHistoryObserver

  _inBatchMode: false,
  onBeginUpdateBatch: function PEX_onBeginUpdateBatch() {
    this._inBatchMode = true;

    // We do not want to expire while we are doing batch work.
    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }
  },

  onEndUpdateBatch: function PEX_onEndUpdateBatch() {
    this._inBatchMode = false;

    // Restore timer.
    if (!this._timer) {
      this._newTimer();
    }
  },

  onClearHistory: function PEX_onClearHistory() {
    // History status is clean after a clear history.
    this.status = STATUS.CLEAN;
  },

  onTitleChanged() {},
  onDeleteURI() {},
  onPageChanged() {},
  onDeleteVisits() {},

  // nsITimerCallback

  notify: function PEX_timerCallback() {
    // Check if we are over history capacity, if so visits must be expired.
    this._getPagesStats(aPagesCount => {
      let overLimitPages = aPagesCount - this._urisLimit;
      this._overLimit = overLimitPages > 0;
      let action = this._overLimit ? ACTION.TIMED_OVERLIMIT : ACTION.TIMED;
      // Adapt expiration aggressivity to the number of pages over the limit.
      let limit =
        overLimitPages > OVERLIMIT_PAGES_THRESHOLD ? LIMIT.LARGE : LIMIT.SMALL;
      // Run at the first idle, or after a minute, whatever comes first.
      Services.tm.idleDispatchToMainThread(() => {
        this._expireWithActionAndLimit(action, limit);
      }, 60000);
    });
  },

  // mozIStorageStatementCallback

  handleResult: function PEX_handleResult(aResultSet) {
    // We don't want to notify after shutdown.
    if (this._shuttingDown) {
      return;
    }

    let row;
    while ((row = aResultSet.getNextRow())) {
      // expected_results is set to the number of expected visits by
      // QUERY_FIND_VISITS_TO_EXPIRE.  We decrease that counter for each found
      // visit and if it reaches zero we mark the database as dirty, since all
      // the expected visits were expired, so it's likely the next run will
      // find more.
      let expectedResults = row.getResultByName("expected_results");
      if (expectedResults > 0) {
        if (!("_expectedResultsCount" in this)) {
          this._expectedResultsCount = expectedResults;
        }
        if (this._expectedResultsCount > 0) {
          this._expectedResultsCount--;
        }
      }

      let uri = Services.io.newURI(row.getResultByName("url"));
      let guid = row.getResultByName("guid");
      let visitDate = row.getResultByName("visit_date");
      let wholeEntry = row.getResultByName("whole_entry");
      let mostRecentExpiredVisit = row.getResultByName(
        "most_recent_expired_visit"
      );
      let reason = Ci.nsINavHistoryObserver.REASON_EXPIRED;
      let observers = PlacesUtils.history.getObservers();

      if (mostRecentExpiredVisit) {
        let days = parseInt(
          (Date.now() - mostRecentExpiredVisit / 1000) / MSECS_PER_DAY
        );
        if (!this._mostRecentExpiredVisitDays) {
          this._mostRecentExpiredVisitDays = days;
        } else if (days < this._mostRecentExpiredVisitDays) {
          this._mostRecentExpiredVisitDays = days;
        }
      }

      // Dispatch expiration notifications to history.
      if (wholeEntry) {
        notify(observers, "onDeleteURI", [uri, guid, reason]);
      } else {
        notify(observers, "onDeleteVisits", [
          uri,
          visitDate > 0,
          guid,
          reason,
          0,
        ]);
      }
    }
  },

  handleError: function PEX_handleError(aError) {
    Cu.reportError(
      "Async statement execution returned with '" +
        aError.result +
        "', '" +
        aError.message +
        "'"
    );
  },

  // Number of expiration steps needed to reach a CLEAN status.
  _telemetrySteps: 1,
  handleCompletion: function PEX_handleCompletion(aReason) {
    if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
      if (this._mostRecentExpiredVisitDays) {
        try {
          Services.telemetry
            .getHistogramById("PLACES_MOST_RECENT_EXPIRED_VISIT_DAYS")
            .add(this._mostRecentExpiredVisitDays);
        } catch (ex) {
          Cu.reportError("Unable to report telemetry.");
        } finally {
          delete this._mostRecentExpiredVisitDays;
        }
      }

      if ("_expectedResultsCount" in this) {
        // Adapt the aggressivity of steps based on the status of history.
        // A dirty history will return all the entries we are expecting bringing
        // our countdown to zero, while a clean one will not.
        let oldStatus = this.status;
        this.status =
          this._expectedResultsCount == 0 ? STATUS.DIRTY : STATUS.CLEAN;

        // Collect or send telemetry data.
        if (this.status == STATUS.DIRTY) {
          this._telemetrySteps++;
        } else {
          // Avoid reporting the common cases where the database is clean, or
          // a single step is needed.
          if (oldStatus == STATUS.DIRTY) {
            try {
              Services.telemetry
                .getHistogramById("PLACES_EXPIRATION_STEPS_TO_CLEAN2")
                .add(this._telemetrySteps);
            } catch (ex) {
              Cu.reportError("Unable to report telemetry.");
            }
          }
          this._telemetrySteps = 1;
        }

        delete this._expectedResultsCount;
      }

      // Dispatch a notification that expiration has finished.
      Services.obs.notifyObservers(null, TOPIC_EXPIRATION_FINISHED);
    }
  },

  // nsPlacesExpiration

  _urisLimit: PREF_MAX_URIS_NOTSET,
  _interval: PREF_INTERVAL_SECONDS_NOTSET,
  _shuttingDown: false,

  _status: STATUS.UNKNOWN,
  set status(aNewStatus) {
    if (aNewStatus != this._status) {
      // If status changes we should restart the timer.
      this._status = aNewStatus;
      this._newTimer();
      // If needed add/remove the cleanup step on idle.  We want to expire on
      // idle only if history is dirty, to preserve mobile devices batteries.
      this.expireOnIdle = aNewStatus == STATUS.DIRTY;
    }
    return aNewStatus;
  },
  get status() {
    return this._status;
  },

  _isIdleObserver: false,
  _expireOnIdle: false,
  set expireOnIdle(aExpireOnIdle) {
    // Observe idle regardless aExpireOnIdle, since we always want to stop
    // timed expiration on idle, to preserve mobile battery life.
    if (!this._isIdleObserver && !this._shuttingDown) {
      this._idle.addIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._isIdleObserver = true;
    } else if (this._isIdleObserver && this._shuttingDown) {
      this._idle.removeIdleObserver(this, IDLE_TIMEOUT_SECONDS);
      this._isIdleObserver = false;
    }

    // If running a debug expiration we need full control of what happens
    // but idle cleanup could activate in the middle, since tinderboxes are
    // permanently idle.  That would cause unexpected oranges, so disable it.
    if (this._debugLimit !== undefined) {
      this._expireOnIdle = false;
    } else {
      this._expireOnIdle = aExpireOnIdle;
    }
    return this._expireOnIdle;
  },
  get expireOnIdle() {
    return this._expireOnIdle;
  },

  async _loadPrefs() {
    // Get the user's limit, if it was set.
    this._urisLimit = this._prefBranch.getIntPref(
      PREF_MAX_URIS,
      PREF_MAX_URIS_NOTSET
    );
    if (this._urisLimit < 0) {
      // Some testing code expects a pref change to be synchronous, so
      // temporarily set this to a large value, while we asynchronously update
      // to the correct value.
      this._urisLimit = 100000;

      // The user didn't specify a custom limit, so we calculate the number of
      // unique places that may fit an optimal database size on this hardware.
      // Oldest pages over this threshold will be expired.
      let memSizeBytes = MEMSIZE_FALLBACK_BYTES;
      try {
        // Limit the size on systems with small memory.
        memSizeBytes = Services.sysinfo.getProperty("memsize");
      } catch (ex) {}
      if (memSizeBytes <= 0) {
        memSizeBytes = MEMSIZE_FALLBACK_BYTES;
      }

      let diskAvailableBytes = DISKSIZE_FALLBACK_BYTES;
      try {
        // Protect against a full disk or tiny quota.
        let dbFile = this._db.databaseFile;
        dbFile.QueryInterface(Ci.nsIFile);
        diskAvailableBytes = dbFile.diskSpaceAvailable;
      } catch (ex) {}
      if (diskAvailableBytes <= 0) {
        diskAvailableBytes = DISKSIZE_FALLBACK_BYTES;
      }

      let optimalDatabaseSize = Math.min(
        (memSizeBytes * DATABASE_TO_MEMORY_PERC) / 100,
        (diskAvailableBytes * DATABASE_TO_DISK_PERC) / 100,
        DATABASE_MAX_SIZE
      );

      // Calculate avg size of a URI in the database.
      let db;
      try {
        db = await PlacesUtils.promiseDBConnection();
        if (db) {
          let row = (
            await db.execute(`SELECT * FROM pragma_page_size(),
                                                pragma_page_count(),
                                                pragma_freelist_count(),
                                                (SELECT count(*) FROM moz_places)`)
          )[0];
          let pageSize = row.getResultByIndex(0);
          let pageCount = row.getResultByIndex(1);
          let freelistCount = row.getResultByIndex(2);
          let uriCount = row.getResultByIndex(3);
          let dbSize = (pageCount - freelistCount) * pageSize;
          let avgURISize = Math.ceil(dbSize / uriCount);
          // For new profiles this value may be too large, due to the Sqlite header,
          // or Infinity when there are no pages.  Thus we must limit it.
          if (avgURISize > URIENTRY_AVG_SIZE * 3) {
            avgURISize = URIENTRY_AVG_SIZE;
          }
          this._urisLimit = Math.ceil(optimalDatabaseSize / avgURISize);
        }
      } catch (ex) {
        // We may have been initialized late in the shutdown process, maybe
        // by a call to clear history on shutdown.
        // If we're unable to get a connection clone, we'll just proceed with
        // the default value, it should not be critical at this point in the
        // application life-cycle.
      }
    }

    // Expose the calculated limit to other components.
    this._prefBranch.setIntPref(
      PREF_READONLY_CALCULATED_MAX_URIS,
      this._urisLimit
    );

    // Get the expiration interval value.
    this._interval = this._prefBranch.getIntPref(
      PREF_INTERVAL_SECONDS,
      PREF_INTERVAL_SECONDS_NOTSET
    );
    if (this._interval <= 0) {
      this._interval = PREF_INTERVAL_SECONDS_NOTSET;
    }
  },

  /**
   * Evaluates the real number of pages in the database and the value currently
   * used by the SQLite query planner.
   *
   * @param aCallback
   *        invoked on success, function (aPagesCount).
   */
  _getPagesStats: function PEX__getPagesStats(aCallback) {
    if (!this._cachedStatements.LIMIT_COUNT) {
      this._cachedStatements.LIMIT_COUNT = this._db.createAsyncStatement(
        `SELECT COUNT(*) FROM moz_places`
      );
    }
    this._cachedStatements.LIMIT_COUNT.executeAsync({
      _pagesCount: 0,
      handleResult(aResults) {
        let row = aResults.getNextRow();
        this._pagesCount = row.getResultByIndex(0);
      },
      handleCompletion(aReason) {
        if (aReason == Ci.mozIStorageStatementCallback.REASON_FINISHED) {
          aCallback(this._pagesCount);
        }
      },
      handleError(aError) {
        Cu.reportError(
          "Async statement execution returned with '" +
            aError.result +
            "', '" +
            aError.message +
            "'"
        );
      },
    });
  },

  /**
   * Execute async statements to expire with the specified queries.
   *
   * @param aAction
   *        The ACTION we are expiring for.  See the ACTION const for values.
   * @param aLimit
   *        Whether to use small, large or no limits when expiring.  See the
   *        LIMIT const for values.
   */
  _expireWithActionAndLimit: function PEX__expireWithActionAndLimit(
    aAction,
    aLimit
  ) {
    (async () => {
      // Ensure that we'll run statements with the most up-to-date pref values.
      // On shutdown we cannot do this, since we must enqueue the expiration
      // statements synchronously before the connection goes away.
      // TODO (Bug 1275878): handle this properly through a shutdown blocker.
      if (!this._shuttingDown) {
        await this._loadPrefsPromise;
      }

      // Skip expiration during batch mode.
      if (this._inBatchMode) {
        return;
      }
      // Don't try to further expire after shutdown.
      if (this._shuttingDown && aAction != ACTION.SHUTDOWN_DIRTY) {
        return;
      }

      let boundStatements = [];
      for (let queryType in EXPIRATION_QUERIES) {
        if (EXPIRATION_QUERIES[queryType].actions & aAction) {
          boundStatements.push(
            this._getBoundStatement(queryType, aLimit, aAction)
          );
        }
      }

      // Execute statements asynchronously in a transaction.
      this._db.executeAsync(boundStatements, this);
    })().catch(Cu.reportError);
  },

  /**
   * Finalizes all of our mozIStorageStatements so we can properly close the
   * database.
   */
  _finalizeInternalStatements: function PEX__finalizeInternalStatements() {
    for (let queryType in this._cachedStatements) {
      let stmt = this._cachedStatements[queryType];
      stmt.finalize();
    }
  },

  /**
   * Generate the statement used for expiration.
   *
   * @param aQueryType
   *        Type of the query to build statement for.
   * @param aLimit
   *        Whether to use small, large or no limits when expiring.  See the
   *        LIMIT const for values.
   * @param aAction
   *        Current action causing the expiration.  See the ACTION const.
   */
  _cachedStatements: {},
  _getBoundStatement: function PEX__getBoundStatement(
    aQueryType,
    aLimit,
    aAction
  ) {
    // Statements creation can be expensive, so we want to cache them.
    let stmt = this._cachedStatements[aQueryType];
    if (stmt === undefined) {
      stmt = this._cachedStatements[aQueryType] = this._db.createAsyncStatement(
        EXPIRATION_QUERIES[aQueryType].sql
      );
    }

    let baseLimit;
    switch (aLimit) {
      case LIMIT.UNLIMITED:
        baseLimit = -1;
        break;
      case LIMIT.SMALL:
        baseLimit = EXPIRE_LIMIT_PER_STEP;
        break;
      case LIMIT.LARGE:
        baseLimit =
          EXPIRE_LIMIT_PER_STEP * EXPIRE_LIMIT_PER_LARGE_STEP_MULTIPLIER;
        break;
      case LIMIT.DEBUG:
        baseLimit = this._debugLimit;
        break;
    }
    if (
      this.status == STATUS.DIRTY &&
      aAction != ACTION.DEBUG &&
      baseLimit > 0
    ) {
      baseLimit *= EXPIRE_AGGRESSIVITY_MULTIPLIER;
    }

    // Bind the appropriate parameters.
    let params = stmt.params;
    switch (aQueryType) {
      case "QUERY_FIND_EXOTIC_VISITS_TO_EXPIRE":
        // Avoid expiring all visits in case of an unlimited debug expiration,
        // just remove orphans instead.
        params.limit_visits =
          aLimit == LIMIT.DEBUG && baseLimit == -1 ? 0 : baseLimit;
        break;
      case "QUERY_FIND_VISITS_TO_EXPIRE":
        params.max_uris = this._urisLimit;
        // Avoid expiring all visits in case of an unlimited debug expiration,
        // just remove orphans instead.
        params.limit_visits =
          aLimit == LIMIT.DEBUG && baseLimit == -1 ? 0 : baseLimit;
        break;
      case "QUERY_FIND_URIS_TO_EXPIRE":
        params.limit_uris = baseLimit;
        break;
      case "QUERY_EXPIRE_ANNOS":
        // Each page may have multiple annos.
        params.limit_annos = baseLimit * EXPIRE_AGGRESSIVITY_MULTIPLIER;
        break;
      case "QUERY_EXPIRE_ITEMS_ANNOS":
        params.limit_annos = baseLimit;
        break;
      case "QUERY_EXPIRE_ANNO_ATTRIBUTES":
        params.limit_annos = baseLimit;
        break;
      case "QUERY_EXPIRE_INPUTHISTORY":
        params.limit_inputhistory = baseLimit;
        break;
    }

    return stmt;
  },

  /**
   * Creates a new timer based on this._interval.
   *
   * @return a REPEATING_SLACK nsITimer that runs every this._interval.
   */
  _newTimer: function PEX__newTimer() {
    if (this._timer) {
      this._timer.cancel();
    }
    if (this._shuttingDown) {
      return undefined;
    }
    let interval =
      this.status != STATUS.DIRTY
        ? this._interval * EXPIRE_AGGRESSIVITY_MULTIPLIER
        : this._interval;

    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(
      this,
      interval * 1000,
      Ci.nsITimer.TYPE_REPEATING_SLACK_LOW_PRIORITY
    );
    if (this._testingMode) {
      Services.obs.notifyObservers(null, TOPIC_TEST_INTERVAL_CHANGED, interval);
    }
    return (this._timer = timer);
  },

  // nsISupports

  classID: Components.ID("705a423f-2f69-42f3-b9fe-1517e0dee56f"),

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsIObserver,
    Ci.nsINavHistoryObserver,
    Ci.nsITimerCallback,
    Ci.mozIStorageStatementCallback,
    Ci.nsISupportsWeakReference,
  ]),
};

// Module Registration

var EXPORTED_SYMBOLS = ["nsPlacesExpiration"];
