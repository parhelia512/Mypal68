"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionSettingsStore",
  "resource://gre/modules/ExtensionSettingsStore.jsm"
);

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

const ITEMS = {
  key1: [
    { key: "key1", value: "val1", id: "@first" },
    { key: "key1", value: "val2", id: "@second" },
    { key: "key1", value: "val3", id: "@third" },
  ],
  key2: [
    { key: "key2", value: "val1-2", id: "@first" },
    { key: "key2", value: "val2-2", id: "@second" },
    { key: "key2", value: "val3-2", id: "@third" },
  ],
};
const KEY_LIST = Object.keys(ITEMS);
const TEST_TYPE = "myType";

let callbackCount = 0;

function initialValue(key) {
  callbackCount++;
  return `key:${key}`;
}

add_task(async function test_settings_store() {
  await promiseStartupManager();

  // Create an array of test framework extension wrappers to install.
  let testExtensions = [
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        applications: { gecko: { id: "@first" } },
      },
    }),
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        applications: { gecko: { id: "@second" } },
      },
    }),
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        applications: { gecko: { id: "@third" } },
      },
    }),
  ];

  for (let extension of testExtensions) {
    await extension.startup();
  }

  // Create an array actual Extension objects which correspond to the
  // test framework extension wrappers.
  let extensions = testExtensions.map(extension => extension.extension);

  let expectedCallbackCount = 0;

  await Assert.rejects(
    ExtensionSettingsStore.getLevelOfControl(1, TEST_TYPE, "key"),
    /The ExtensionSettingsStore was accessed before the initialize promise resolved/,
    "Accessing the SettingsStore before it is initialized throws an error."
  );

  // Initialize the SettingsStore.
  await ExtensionSettingsStore.initialize();

  // Add a setting for the second oldest extension, where it is the only setting for a key.
  for (let key of KEY_LIST) {
    let extensionIndex = 1;
    let itemToAdd = ITEMS[key][extensionIndex];
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl with no settings set for a key."
    );
    let item = await ExtensionSettingsStore.addSetting(
      extensions[extensionIndex].id,
      TEST_TYPE,
      itemToAdd.key,
      itemToAdd.value,
      initialValue
    );
    expectedCallbackCount++;
    equal(
      callbackCount,
      expectedCallbackCount,
      "initialValueCallback called the expected number of times."
    );
    deepEqual(
      item,
      itemToAdd,
      "Adding initial item for a key returns that item."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      itemToAdd,
      "getSetting returns correct item with only one item in the list."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl with only one item in the list."
    );
    ok(
      ExtensionSettingsStore.hasSetting(
        extensions[extensionIndex].id,
        TEST_TYPE,
        key
      ),
      "hasSetting returns the correct value when an extension has a setting set."
    );
    item = await ExtensionSettingsStore.getSetting(
      TEST_TYPE,
      key,
      extensions[extensionIndex].id
    );
    deepEqual(
      item,
      itemToAdd,
      "getSetting with id returns correct item with only one item in the list."
    );
  }

  // Add a setting for the oldest extension.
  for (let key of KEY_LIST) {
    let extensionIndex = 0;
    let itemToAdd = ITEMS[key][extensionIndex];
    let item = await ExtensionSettingsStore.addSetting(
      extensions[extensionIndex].id,
      TEST_TYPE,
      itemToAdd.key,
      itemToAdd.value,
      initialValue
    );
    equal(
      callbackCount,
      expectedCallbackCount,
      "initialValueCallback called the expected number of times."
    );
    equal(
      item,
      null,
      "An older extension adding a setting for a key returns null"
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][1],
      "getSetting returns correct item with more than one item in the list."
    );
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_other_extensions",
      "getLevelOfControl returns correct levelOfControl when another extension is in control."
    );
    item = await ExtensionSettingsStore.getSetting(
      TEST_TYPE,
      key,
      extensions[extensionIndex].id
    );
    deepEqual(
      item,
      itemToAdd,
      "getSetting with id returns correct item with more than one item in the list."
    );
  }

  // Reload the settings store to emulate a browser restart.
  await ExtensionSettingsStore._reloadFile();

  // Add a setting for the newest extension.
  for (let key of KEY_LIST) {
    let extensionIndex = 2;
    let itemToAdd = ITEMS[key][extensionIndex];
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl for a more recent extension."
    );
    let item = await ExtensionSettingsStore.addSetting(
      extensions[extensionIndex].id,
      TEST_TYPE,
      itemToAdd.key,
      itemToAdd.value,
      initialValue
    );
    equal(
      callbackCount,
      expectedCallbackCount,
      "initialValueCallback called the expected number of times."
    );
    deepEqual(
      item,
      itemToAdd,
      "Adding item for most recent extension returns that item."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      itemToAdd,
      "getSetting returns correct item with more than one item in the list."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl when this extension is in control."
    );
    item = await ExtensionSettingsStore.getSetting(
      TEST_TYPE,
      key,
      extensions[extensionIndex].id
    );
    deepEqual(
      item,
      itemToAdd,
      "getSetting with id returns correct item with more than one item in the list."
    );
  }

  for (let extension of extensions) {
    let items = await ExtensionSettingsStore.getAllForExtension(
      extension.id,
      TEST_TYPE
    );
    deepEqual(items, KEY_LIST, "getAllForExtension returns expected keys.");
  }

  // Attempting to remove a setting that has not been set should *not* throw an exception.
  let removeResult = await ExtensionSettingsStore.removeSetting(
    extensions[0].id,
    "myType",
    "unset_key"
  );
  equal(
    removeResult,
    null,
    "Removing a setting that was not previously set returns null."
  );

  // Attempting to disable a setting that has not been set should throw an exception.
  Assert.throws(
    () =>
      ExtensionSettingsStore.disable(extensions[0].id, "myType", "unset_key"),
    /Cannot alter the setting for myType:unset_key as it does not exist/,
    "disable rejects with an unset key."
  );

  // Attempting to enable a setting that has not been set should throw an exception.
  Assert.throws(
    () =>
      ExtensionSettingsStore.enable(extensions[0].id, "myType", "unset_key"),
    /Cannot alter the setting for myType:unset_key as it does not exist/,
    "enable rejects with an unset key."
  );

  let expectedKeys = KEY_LIST;
  // Disable the non-top item for a key.
  for (let key of KEY_LIST) {
    let extensionIndex = 0;
    let item = await ExtensionSettingsStore.addSetting(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key,
      "new value",
      initialValue
    );
    equal(
      callbackCount,
      expectedCallbackCount,
      "initialValueCallback called the expected number of times."
    );
    equal(item, null, "Updating non-top item for a key returns null");
    item = await ExtensionSettingsStore.disable(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(item, null, "Disabling non-top item for a key returns null.");
    let allForExtension = await ExtensionSettingsStore.getAllForExtension(
      extensions[extensionIndex].id,
      TEST_TYPE
    );
    deepEqual(
      allForExtension,
      expectedKeys,
      "getAllForExtension returns expected keys after a disable."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][2],
      "getSetting returns correct item after a disable."
    );
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_other_extensions",
      "getLevelOfControl returns correct levelOfControl after disabling of non-top item."
    );
  }

  // Re-enable the non-top item for a key.
  for (let key of KEY_LIST) {
    let extensionIndex = 0;
    let item = await ExtensionSettingsStore.enable(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(item, null, "Enabling non-top item for a key returns null.");
    let allForExtension = await ExtensionSettingsStore.getAllForExtension(
      extensions[extensionIndex].id,
      TEST_TYPE
    );
    deepEqual(
      allForExtension,
      expectedKeys,
      "getAllForExtension returns expected keys after an enable."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][2],
      "getSetting returns correct item after an enable."
    );
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_other_extensions",
      "getLevelOfControl returns correct levelOfControl after enabling of non-top item."
    );
  }

  // Remove the non-top item for a key.
  for (let key of KEY_LIST) {
    let extensionIndex = 0;
    let item = await ExtensionSettingsStore.removeSetting(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(item, null, "Removing non-top item for a key returns null.");
    expectedKeys = expectedKeys.filter(expectedKey => expectedKey != key);
    let allForExtension = await ExtensionSettingsStore.getAllForExtension(
      extensions[extensionIndex].id,
      TEST_TYPE
    );
    deepEqual(
      allForExtension,
      expectedKeys,
      "getAllForExtension returns expected keys after a removal."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][2],
      "getSetting returns correct item after a removal."
    );
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[extensionIndex].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_other_extensions",
      "getLevelOfControl returns correct levelOfControl after removal of non-top item."
    );
    ok(
      !ExtensionSettingsStore.hasSetting(
        extensions[extensionIndex].id,
        TEST_TYPE,
        key
      ),
      "hasSetting returns the correct value when an extension does not have a setting set."
    );
  }

  for (let key of KEY_LIST) {
    // Disable the top item for a key.
    let item = await ExtensionSettingsStore.disable(
      extensions[2].id,
      TEST_TYPE,
      key
    );
    deepEqual(
      item,
      ITEMS[key][1],
      "Disabling top item for a key returns the new top item."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][1],
      "getSetting returns correct item after a disable."
    );
    let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[2].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after disabling of top item."
    );

    // Re-enable the top item for a key.
    item = await ExtensionSettingsStore.enable(
      extensions[2].id,
      TEST_TYPE,
      key
    );
    deepEqual(
      item,
      ITEMS[key][2],
      "Re-enabling top item for a key returns the old top item."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][2],
      "getSetting returns correct item after an enable."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[2].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after re-enabling top item."
    );

    // Remove the top item for a key.
    item = await ExtensionSettingsStore.removeSetting(
      extensions[2].id,
      TEST_TYPE,
      key
    );
    deepEqual(
      item,
      ITEMS[key][1],
      "Removing top item for a key returns the new top item."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      ITEMS[key][1],
      "getSetting returns correct item after a removal."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[2].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after removal of top item."
    );

    // Add a setting for the current top item.
    let itemToAdd = { key, value: `new-${key}`, id: "@second" };
    item = await ExtensionSettingsStore.addSetting(
      extensions[1].id,
      TEST_TYPE,
      itemToAdd.key,
      itemToAdd.value,
      initialValue
    );
    equal(
      callbackCount,
      expectedCallbackCount,
      "initialValueCallback called the expected number of times."
    );
    deepEqual(
      item,
      itemToAdd,
      "Updating top item for a key returns that item."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      itemToAdd,
      "getSetting returns correct item after updating."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after updating."
    );

    // Disable the last remaining item for a key.
    let expectedItem = { key, initialValue: initialValue(key) };
    // We're using the callback to set the expected value, so we need to increment the
    // expectedCallbackCount.
    expectedCallbackCount++;
    item = await ExtensionSettingsStore.disable(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    deepEqual(
      item,
      expectedItem,
      "Disabling last item for a key returns the initial value."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      expectedItem,
      "getSetting returns the initial value after all are disabled."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after all are disabled."
    );

    // Re-enable the last remaining item for a key.
    item = await ExtensionSettingsStore.enable(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    deepEqual(
      item,
      itemToAdd,
      "Re-enabling last item for a key returns the old value."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(
      item,
      itemToAdd,
      "getSetting returns expected value after re-enabling."
    );
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controlled_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after re-enabling."
    );

    // Remove the last remaining item for a key.
    item = await ExtensionSettingsStore.removeSetting(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    deepEqual(
      item,
      expectedItem,
      "Removing last item for a key returns the initial value."
    );
    item = await ExtensionSettingsStore.getSetting(TEST_TYPE, key);
    deepEqual(item, null, "getSetting returns null after all are removed.");
    levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    equal(
      levelOfControl,
      "controllable_by_this_extension",
      "getLevelOfControl returns correct levelOfControl after all are removed."
    );

    // Attempting to remove a setting that has had all extensions removed should *not* throw an exception.
    removeResult = await ExtensionSettingsStore.removeSetting(
      extensions[1].id,
      TEST_TYPE,
      key
    );
    equal(
      removeResult,
      null,
      "Removing a setting that has had all extensions removed returns null."
    );
  }

  // Test adding a setting with a value in callbackArgument.
  let extensionIndex = 0;
  let testKey = "callbackArgumentKey";
  let callbackArgumentValue = Date.now();
  // Add the setting.
  let item = await ExtensionSettingsStore.addSetting(
    extensions[extensionIndex].id,
    TEST_TYPE,
    testKey,
    1,
    initialValue,
    callbackArgumentValue
  );
  expectedCallbackCount++;
  equal(
    callbackCount,
    expectedCallbackCount,
    "initialValueCallback called the expected number of times."
  );
  // Remove the setting which should return the initial value.
  let expectedItem = {
    key: testKey,
    initialValue: initialValue(callbackArgumentValue),
  };
  // We're using the callback to set the expected value, so we need to increment the
  // expectedCallbackCount.
  expectedCallbackCount++;
  item = await ExtensionSettingsStore.removeSetting(
    extensions[extensionIndex].id,
    TEST_TYPE,
    testKey
  );
  deepEqual(
    item,
    expectedItem,
    "Removing last item for a key returns the initial value."
  );
  item = await ExtensionSettingsStore.getSetting(TEST_TYPE, testKey);
  deepEqual(item, null, "getSetting returns null after all are removed.");

  item = await ExtensionSettingsStore.getSetting(TEST_TYPE, "not a key");
  equal(
    item,
    null,
    "getSetting returns a null item if the setting does not have any records."
  );
  let levelOfControl = await ExtensionSettingsStore.getLevelOfControl(
    extensions[1].id,
    TEST_TYPE,
    "not a key"
  );
  equal(
    levelOfControl,
    "controllable_by_this_extension",
    "getLevelOfControl returns correct levelOfControl if the setting does not have any records."
  );

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});

add_task(async function test_settings_store_setByUser() {
  await promiseStartupManager();

  // Create an array of test framework extension wrappers to install.
  let testExtensions = [
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        applications: { gecko: { id: "@first" } },
      },
    }),
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        applications: { gecko: { id: "@second" } },
      },
    }),
    ExtensionTestUtils.loadExtension({
      useAddonManager: "temporary",
      manifest: {
        applications: { gecko: { id: "@third" } },
      },
    }),
  ];

  let type = "some_type";
  let key = "some_key";

  for (let extension of testExtensions) {
    await extension.startup();
  }

  // Create an array actual Extension objects which correspond to the
  // test framework extension wrappers.
  let [one, two, three] = testExtensions.map(extension => extension.extension);
  let initialCallback = () => "initial";

  // Initialize the SettingsStore.
  await ExtensionSettingsStore.initialize();

  equal(
    null,
    ExtensionSettingsStore.getSetting(type, key),
    "getSetting is initially null"
  );

  let item = await ExtensionSettingsStore.addSetting(
    one.id,
    type,
    key,
    "one",
    initialCallback
  );
  deepEqual(
    { key, value: "one", id: one.id },
    item,
    "addSetting returns the first set item"
  );

  item = await ExtensionSettingsStore.addSetting(
    two.id,
    type,
    key,
    "two",
    initialCallback
  );
  deepEqual(
    { key, value: "two", id: two.id },
    item,
    "addSetting returns the second set item"
  );

  item = await ExtensionSettingsStore.addSetting(
    three.id,
    type,
    key,
    "three",
    initialCallback
  );
  deepEqual(
    { key, value: "three", id: three.id },
    item,
    "addSetting returns the third set item"
  );

  deepEqual(
    item,
    ExtensionSettingsStore.getSetting(type, key),
    "getSetting returns the third set item"
  );

  ExtensionSettingsStore.setByUser(type, key);
  deepEqual(
    { key, initialValue: "initial" },
    ExtensionSettingsStore.getSetting(type, key),
    "getSetting returns the initial value after being set by user"
  );

  item = ExtensionSettingsStore.enable(one.id, type, key);
  deepEqual(
    { key, value: "one", id: one.id },
    item,
    "enable returns the first set item after enable"
  );

  item = ExtensionSettingsStore.enable(three.id, type, key);
  deepEqual(
    { key, value: "three", id: three.id },
    item,
    "enable returns the third set item after enable"
  );

  item = ExtensionSettingsStore.enable(two.id, type, key);
  deepEqual(
    undefined,
    item,
    "enable returns undefined after enabling the second item"
  );

  item = ExtensionSettingsStore.getSetting(type, key);
  deepEqual(
    { key, value: "three", id: three.id },
    item,
    "getSetting returns the third set item after enabling the second item"
  );

  ExtensionSettingsStore.removeSetting(three.id, type, key);
  ExtensionSettingsStore.removeSetting(two.id, type, key);
  ExtensionSettingsStore.removeSetting(one.id, type, key);

  equal(
    null,
    ExtensionSettingsStore.getSetting(type, key),
    "getSetting returns null after removing all settings"
  );

  for (let extension of testExtensions) {
    await extension.unload();
  }

  await promiseShutdownManager();
});

add_task(async function test_settings_store_add_disabled() {
  await promiseStartupManager();

  let id = "@add-on-disable";
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      applications: { gecko: { id } },
    },
  });

  await extension.startup();
  await ExtensionSettingsStore.initialize();

  await ExtensionSettingsStore.addSetting(
    id,
    "foo",
    "bar",
    "set",
    () => "not set"
  );

  let item = ExtensionSettingsStore.getSetting("foo", "bar");
  equal(item.id, id, "The add-on is in control");
  equal(item.value, "set", "The value is set");

  ExtensionSettingsStore.disable(id, "foo", "bar");
  item = ExtensionSettingsStore.getSetting("foo", "bar");
  equal(item.id, undefined, "The add-on is not in control");
  equal(item.initialValue, "not set", "The value is not set");

  await ExtensionSettingsStore.addSetting(
    id,
    "foo",
    "bar",
    "set",
    () => "not set"
  );
  item = ExtensionSettingsStore.getSetting("foo", "bar");
  equal(item.id, id, "The add-on is in control");
  equal(item.value, "set", "The value is set");

  await extension.unload();

  await promiseShutdownManager();
});

add_task(async function test_exceptions() {
  await ExtensionSettingsStore.initialize();

  await Assert.rejects(
    ExtensionSettingsStore.addSetting(
      1,
      TEST_TYPE,
      "key_not_a_function",
      "val1",
      "not a function"
    ),
    /initialValueCallback must be a function/,
    "addSetting rejects with a callback that is not a function."
  );
});
