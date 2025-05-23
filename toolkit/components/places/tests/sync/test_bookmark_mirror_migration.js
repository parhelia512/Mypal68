/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Keep in sync with `SyncedBookmarksMirror.jsm`.
const CURRENT_MIRROR_SCHEMA_VERSION = 7;

// The oldest schema version that we support. Any databases with schemas older
// than this will be dropped and recreated.
const OLDEST_SUPPORTED_MIRROR_SCHEMA_VERSION = 5;

async function getIndexNames(db, table, schema = "mirror") {
  let rows = await db.execute(`PRAGMA ${schema}.index_list(${table})`);
  let names = [];
  for (let row of rows) {
    // Column 4 is `c` if the index was created via `CREATE INDEX`, `u` if
    // via `UNIQUE`, and `pk` if via `PRIMARY KEY`.
    let wasCreated = row.getResultByIndex(3) == "c";
    if (wasCreated) {
      // Column 2 is the name of the index.
      names.push(row.getResultByIndex(1));
    }
  }
  return names.sort();
}

add_task(async function test_migrate_after_downgrade() {
  await PlacesTestUtils.markBookmarksAsSynced();

  let dbFile = await setupFixtureFile("mirror_v5.sqlite");
  let oldBuf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
  });

  info("Downgrade schema version to oldest supported");
  await oldBuf.db.setSchemaVersion(
    OLDEST_SUPPORTED_MIRROR_SCHEMA_VERSION,
    "mirror"
  );
  await oldBuf.finalize();

  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
  });

  // All migrations between `OLDEST_SUPPORTED_MIRROR_SCHEMA_VERSION` should
  // be idempotent. When we downgrade, we roll back the schema version, but
  // leave the schema changes in place, since we can't anticipate what a
  // future version will change.
  let schemaVersion = await buf.db.getSchemaVersion("mirror");
  equal(
    schemaVersion,
    CURRENT_MIRROR_SCHEMA_VERSION,
    "Should upgrade downgraded mirror schema"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Migrations between 5 and 7 add three indexes.
add_task(async function test_migrate_from_5_to_current() {
  await PlacesTestUtils.markBookmarksAsSynced();

  let dbFile = await setupFixtureFile("mirror_v5.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
  });

  let schemaVersion = await buf.db.getSchemaVersion("mirror");
  equal(
    schemaVersion,
    CURRENT_MIRROR_SCHEMA_VERSION,
    "Should upgrade mirror schema to current version"
  );

  let itemsIndexNames = await getIndexNames(buf.db, "items");
  deepEqual(
    itemsIndexNames,
    ["itemKeywords", "itemURLs"],
    "Should add two indexes on items"
  );

  let structureIndexNames = await getIndexNames(buf.db, "structure");
  deepEqual(
    structureIndexNames,
    ["structurePositions"],
    "Should add an index on structure"
  );

  let changesToUpload = await buf.apply();
  deepEqual(changesToUpload, {}, "Shouldn't flag any items for reupload");

  await assertLocalTree(
    PlacesUtils.bookmarks.menuGuid,
    {
      guid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      index: 0,
      title: BookmarksMenuTitle,
      children: [
        {
          guid: "bookmarkAAAA",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 0,
          title: "A",
          url: "http://example.com/a",
        },
        {
          guid: "bookmarkBBBB",
          type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
          index: 1,
          title: "B",
          url: "http://example.com/b",
          keyword: "hi",
        },
      ],
    },
    "Should apply mirror tree after migrating"
  );

  let keywordEntry = await PlacesUtils.keywords.fetch("hi");
  equal(
    keywordEntry.url.href,
    "http://example.com/b",
    "Should apply keyword from migrated mirror"
  );

  await buf.finalize();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesSyncUtils.bookmarks.reset();
});

// Migrations between 1 and 2 discard the entire database.
add_task(async function test_migrate_from_1_to_2() {
  let dbFile = await setupFixtureFile("mirror_v1.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: dbFile.path,
    },
  });
  ok(
    buf.wasCorrupt,
    "Migrating from unsupported version should mark database as corrupt"
  );
  await buf.finalize();
});

add_task(async function test_database_corrupt() {
  let corruptFile = await setupFixtureFile("mirror_corrupt.sqlite");
  let buf = await SyncedBookmarksMirror.open({
    path: corruptFile.path,
    },
  });
  ok(buf.wasCorrupt, "Opening corrupt database should mark it as such");
  await buf.finalize();
});
