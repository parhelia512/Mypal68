/* eslint max-len: ["error", 80] */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

let promptService;

const SUPPORT_URL = Services.urlFormatter.formatURL(
  Services.prefs.getStringPref("app.support.baseURL")
);
const REMOVE_SUMO_URL = SUPPORT_URL + "cant-remove-addon";

const SECTION_INDEXES = {
  enabled: 0,
  disabled: 1,
};
function getSection(doc, type) {
  return doc.querySelector(`section[section="${SECTION_INDEXES[type]}"]`);
}

function getTestCards(root) {
  return root.querySelectorAll('addon-card[addon-id$="@mochi.test"]');
}

function getCardByAddonId(root, id) {
  return root.querySelector(`addon-card[addon-id="${id}"]`);
}

function isEmpty(el) {
  return !el.children.length;
}

function waitForThemeChange(list) {
  // Wait for two move events. One theme will be enabled and another disabled.
  let moveCount = 0;
  return BrowserTestUtils.waitForEvent(list, "move", () => ++moveCount == 2);
}

let mockProvider;

add_task(async function setup() {
  mockProvider = new MockProvider();
  promptService = mockPromptService();
});

let extensionsCreated = 0;

function createExtensions(manifestExtras) {
  return manifestExtras.map(extra =>
    ExtensionTestUtils.loadExtension({
      manifest: {
        name: "Test extension",
        applications: {
          gecko: { id: `test-${extensionsCreated++}@mochi.test` },
        },
        icons: {
          32: "test-icon.png",
        },
        ...extra,
      },
      useAddonManager: "temporary",
    })
  );
}

add_task(async function testExtensionList() {
  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id } },
      icons: {
        32: "test-icon.png",
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addon = await AddonManager.getAddonByID(id);
  ok(addon, "The add-on can be found");

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");

  // There shouldn't be any disabled extensions.
  let disabledSection = getSection(doc, "disabled");
  ok(isEmpty(disabledSection), "The disabled section is empty");

  // The loaded extension should be in the enabled list.
  let enabledSection = getSection(doc, "enabled");
  ok(!isEmpty(enabledSection), "The enabled section isn't empty");
  let card = getCardByAddonId(enabledSection, id);
  ok(card, "The card is in the enabled section");

  // Check the properties of the card.
  is(card.addonNameEl.textContent, "Test extension", "The name is set");
  let icon = card.querySelector(".addon-icon");
  ok(icon.src.endsWith("/test-icon.png"), "The icon is set");

  // Disable the extension.
  let disableToggle = card.querySelector('[action="toggle-disabled"]');
  ok(disableToggle.checked, "The disable toggle is checked");
  is(
    doc.l10n.getAttributes(disableToggle).id,
    "extension-enable-addon-button-label",
    "The toggle has the enable label"
  );
  ok(disableToggle.getAttribute("aria-label"), "There's an aria-label");
  ok(!disableToggle.hidden, "The toggle is visible");

  let disabled = BrowserTestUtils.waitForEvent(list, "move");
  disableToggle.click();
  await disabled;
  is(
    card.parentNode,
    disabledSection,
    "The card is now in the disabled section"
  );

  // The disable button is now enable.
  ok(!disableToggle.checked, "The disable toggle is unchecked");
  is(
    doc.l10n.getAttributes(disableToggle).id,
    "extension-enable-addon-button-label",
    "The button has the same enable label"
  );
  ok(disableToggle.getAttribute("aria-label"), "There's an aria-label");

  // Remove the add-on.
  let removeButton = card.querySelector('[action="remove"]');
  is(
    doc.l10n.getAttributes(removeButton).id,
    "remove-addon-button",
    "The button has the remove label"
  );
  // There is a support link when the add-on isn't removeable, verify we don't
  // always include one.
  ok(!removeButton.querySelector("a"), "There isn't a link in the item");

  // Remove but cancel.
  let cancelled = BrowserTestUtils.waitForEvent(card, "remove-cancelled");
  removeButton.click();
  await cancelled;

  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  // Tell the mock prompt service that the prompt was accepted.
  promptService._response = 0;
  removeButton.click();
  await removed;

  addon = await AddonManager.getAddonByID(id);
  ok(
    addon && !!(addon.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "The addon is pending uninstall"
  );

  // Ensure that a pending uninstall bar has been created for the
  // pending uninstall extension, and pressing the undo button will
  // refresh the list and render a card to the re-enabled extension.
  assertHasPendingUninstalls(list, 1);
  assertHasPendingUninstallAddon(list, addon);

  // Add a second pending uninstall extension.
  info("Install a second test extension and wait for addon card rendered");
  let added = BrowserTestUtils.waitForEvent(list, "add");
  const extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension 2",
      applications: { gecko: { id: "test-2@mochi.test" } },
      icons: {
        32: "test-icon.png",
      },
    },
    useAddonManager: "temporary",
  });
  await extension2.startup();

  await added;
  ok(
    getCardByAddonId(list, extension2.id),
    "Got a card added for the second extension"
  );

  info("Uninstall the second test extension and wait for addon card removed");
  removed = BrowserTestUtils.waitForEvent(list, "remove");
  const addon2 = await AddonManager.getAddonByID(extension2.id);
  addon2.uninstall(true);
  await removed;

  ok(
    !getCardByAddonId(list, extension2.id),
    "Addon card for the second extension removed"
  );

  assertHasPendingUninstalls(list, 2);
  assertHasPendingUninstallAddon(list, addon2);

  // Addon2 was enabled before entering the pending uninstall state,
  // wait for its startup after pressing undo.
  let addon2Started = AddonTestUtils.promiseWebExtensionStartup(addon2.id);
  await testUndoPendingUninstall(list, addon);
  await testUndoPendingUninstall(list, addon2);
  info("Wait for the second pending uninstal add-ons startup");
  await addon2Started;

  ok(
    getCardByAddonId(disabledSection, addon.id),
    "The card for the first extension is in the disabled section"
  );
  ok(
    getCardByAddonId(enabledSection, addon2.id),
    "The card for the second extension is in the enabled section"
  );

  await extension2.unload();
  await extension.unload();

  // Install a theme and verify that it is not listed in the pending
  // uninstall message bars while the list extensions view is loaded.
  const themeXpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "My theme",
      applications: { gecko: { id: "theme@mochi.test" } },
      theme: {},
    },
  });
  const themeAddon = await AddonManager.installTemporaryAddon(themeXpi);
  // Leave it pending uninstall, the following assertions related to
  // the pending uninstall message bars will fail if the theme is listed.
  await themeAddon.uninstall(true);

  // Install a third addon to verify that is being fully removed once the
  // about:addons page is closed.
  const xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Test extension 3",
      applications: { gecko: { id: "test-3@mochi.test" } },
      icons: {
        32: "test-icon.png",
      },
    },
  });

  added = BrowserTestUtils.waitForEvent(list, "add");
  const addon3 = await AddonManager.installTemporaryAddon(xpi);
  await added;
  ok(
    getCardByAddonId(list, addon3.id),
    "Addon card for the third extension added"
  );

  removed = BrowserTestUtils.waitForEvent(list, "remove");
  addon3.uninstall(true);
  await removed;
  ok(
    !getCardByAddonId(list, addon3.id),
    "Addon card for the third extension removed"
  );

  assertHasPendingUninstalls(list, 1);
  ok(
    addon3 && !!(addon3.pendingOperations & AddonManager.PENDING_UNINSTALL),
    "The third addon is pending uninstall"
  );

  await closeView(win);

  ok(
    !(await AddonManager.getAddonByID(addon3.id)),
    "The third addon has been fully uninstalled"
  );

  ok(
    themeAddon.pendingOperations & AddonManager.PENDING_UNINSTALL,
    "The theme addon is pending after the list extension view is closed"
  );

  await themeAddon.uninstall();

  ok(
    !(await AddonManager.getAddonByID(themeAddon.id)),
    "The theme addon is fully uninstalled"
  );
});

add_task(async function testMouseSupport() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id: "test@mochi.test" } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let [card] = getTestCards(doc);
  is(card.addon.id, "test@mochi.test", "The right card is found");

  let menuButton = card.querySelector('[action="more-options"]');
  let panel = card.querySelector("panel-list");

  ok(!panel.open, "The panel is initially closed");
  await BrowserTestUtils.synthesizeMouseAtCenter(
    menuButton,
    { type: "mousedown" },
    gBrowser.selectedBrowser
  );
  ok(panel.open, "The panel is now open");

  await closeView(win);
  await extension.unload();
});

add_task(async function testKeyboardSupport() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id: "test@mochi.test" } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Some helpers.
  let tab = event => EventUtils.synthesizeKey("VK_TAB", event);
  let space = () => EventUtils.synthesizeKey(" ", {});
  let isFocused = (el, msg) => is(doc.activeElement, el, msg);

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");
  let enabledSection = getSection(doc, "enabled");
  let disabledSection = getSection(doc, "disabled");

  // Find the card.
  let [card] = getTestCards(list);
  is(card.addon.id, "test@mochi.test", "The right card is found");

  // Focus the more options menu button.
  let moreOptionsButton = card.querySelector('[action="more-options"]');
  moreOptionsButton.focus();
  isFocused(moreOptionsButton, "The more options button is focused");

  // Test opening and closing the menu.
  let moreOptionsMenu = card.querySelector("panel-list");
  let expandButton = moreOptionsMenu.querySelector('[action="expand"]');
  let removeButton = card.querySelector('[action="remove"]');
  is(moreOptionsMenu.open, false, "The menu is closed");
  let shown = BrowserTestUtils.waitForEvent(moreOptionsMenu, "shown");
  space();
  await shown;
  is(moreOptionsMenu.open, true, "The menu is open");
  isFocused(removeButton, "The remove button is now focused");
  tab({ shiftKey: true });
  is(moreOptionsMenu.open, true, "The menu stays open");
  isFocused(expandButton, "The focus has looped to the bottom");
  tab();
  is(moreOptionsMenu.open, true, "The menu stays open");
  isFocused(removeButton, "The focus has looped to the top");

  let hidden = BrowserTestUtils.waitForEvent(moreOptionsMenu, "hidden");
  EventUtils.synthesizeKey("Escape", {});
  await hidden;
  isFocused(moreOptionsButton, "Escape closed the menu");

  // Disable the add-on.
  let disableButton = card.querySelector('[action="toggle-disabled"]');
  tab({ shiftKey: true });
  isFocused(disableButton, "The disable toggle is focused");
  is(card.parentNode, enabledSection, "The card is in the enabled section");
  let disabled = BrowserTestUtils.waitForEvent(list, "move");
  space();
  await disabled;
  is(
    card.parentNode,
    disabledSection,
    "The card is now in the disabled section"
  );
  isFocused(disableButton, "The disable button is still focused");

  // Remove the add-on.
  tab();
  isFocused(moreOptionsButton, "The more options button is focused again");
  shown = BrowserTestUtils.waitForEvent(moreOptionsMenu, "shown");
  space();
  is(moreOptionsMenu.open, true, "The menu is open");
  await shown;
  isFocused(removeButton, "The remove button is focused");
  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  space();
  await removed;
  is(card.parentNode, null, "The card is no longer on the page");

  await extension.unload();
  await closeView(win);
});

add_task(async function testOpenDetailFromNameKeyboard() {
  let id = "details@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Detail extension",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");

  let card = getCardByAddonId(win.document, id);

  info("focus the add-on's name, which should be an <a>");
  card.addonNameEl.focus();

  let detailsLoaded = waitForViewLoad(win);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await detailsLoaded;

  card = getCardByAddonId(win.document, id);
  is(
    card.addonNameEl.textContent,
    "Detail extension",
    "The right detail view is laoded"
  );

  await extension.unload();
  await closeView(win);
});

add_task(async function testExtensionReordering() {
  let extensions = createExtensions([
    { name: "Extension One" },
    { name: "This is last" },
    { name: "An extension, is first" },
  ]);

  await Promise.all(extensions.map(extension => extension.startup()));

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Get a reference to the addon-list for events.
  let list = doc.querySelector("addon-list");

  // Find the related cards, they should all have @mochi.test ids.
  let enabledSection = getSection(doc, "enabled");
  let cards = getTestCards(enabledSection);

  is(cards.length, 3, "Each extension has an addon-card");

  let order = Array.from(cards).map(card => card.addon.name);
  Assert.deepEqual(
    order,
    ["An extension, is first", "Extension One", "This is last"],
    "The add-ons are sorted by name"
  );

  // Disable the second extension.
  let disabledSection = getSection(doc, "disabled");
  ok(isEmpty(disabledSection), "The disabled section is initially empty");

  // Disable the add-ons in a different order.
  let reorderedCards = [cards[1], cards[0], cards[2]];
  for (let { addon } of reorderedCards) {
    let moved = BrowserTestUtils.waitForEvent(list, "move");
    await addon.disable();
    await moved;
  }

  order = Array.from(getTestCards(disabledSection)).map(
    card => card.addon.name
  );
  Assert.deepEqual(
    order,
    ["An extension, is first", "Extension One", "This is last"],
    "The add-ons are sorted by name"
  );

  // All of our installed add-ons are disabled, install a new one.
  let [newExtension] = createExtensions([{ name: "Extension New" }]);
  let added = BrowserTestUtils.waitForEvent(list, "add");
  await newExtension.startup();
  await added;

  let [newCard] = getTestCards(enabledSection);
  is(
    newCard.addon.name,
    "Extension New",
    "The new add-on is in the enabled list"
  );

  // Enable everything again.
  for (let { addon } of cards) {
    let moved = BrowserTestUtils.waitForEvent(list, "move");
    await addon.enable();
    await moved;
  }

  order = Array.from(getTestCards(enabledSection)).map(card => card.addon.name);
  Assert.deepEqual(
    order,
    [
      "An extension, is first",
      "Extension New",
      "Extension One",
      "This is last",
    ],
    "The add-ons are sorted by name"
  );

  // Remove the new extension.
  let removed = BrowserTestUtils.waitForEvent(list, "remove");
  await newExtension.unload();
  await removed;
  is(newCard.parentNode, null, "The new card has been removed");

  await Promise.all(extensions.map(extension => extension.unload()));
  await closeView(win);
});

add_task(async function testThemeList() {
  let theme = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id: "theme@mochi.test" } },
      name: "My theme",
      theme: {},
    },
    useAddonManager: "temporary",
  });

  let win = await loadInitialView("theme");
  let doc = win.document;

  let list = doc.querySelector("addon-list");

  let cards = getTestCards(list);
  is(cards.length, 0, "There are no test themes to start");

  let added = BrowserTestUtils.waitForEvent(list, "add");
  await theme.startup();
  await added;

  cards = getTestCards(list);
  is(cards.length, 1, "There is now one custom theme");

  let [card] = cards;
  is(card.addon.name, "My theme", "The card is for the test theme");

  let enabledSection = getSection(doc, "enabled");
  let disabledSection = getSection(doc, "disabled");

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  is(
    card.parentNode,
    enabledSection,
    "The new theme card is in the enabled section"
  );
  is(
    enabledSection.querySelectorAll("addon-card").length,
    1,
    "There is one enabled theme"
  );

  let themesChanged = waitForThemeChange(list);
  card.querySelector('[action="toggle-disabled"]').click();
  await themesChanged;

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  is(
    card.parentNode,
    disabledSection,
    "The card is now in the disabled section"
  );
  is(
    enabledSection.querySelectorAll("addon-card").length,
    1,
    "There is one enabled theme"
  );

  await theme.unload();
  await closeView(win);
});

add_task(async function testBuiltInThemeButtons() {
  let win = await loadInitialView("theme");
  let doc = win.document;

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");
  let enabledSection = getSection(doc, "enabled");
  let disabledSection = getSection(doc, "disabled");

  let defaultTheme = getCardByAddonId(doc, "default-theme@mozilla.org");
  let darkTheme = getCardByAddonId(doc, "firefox-compact-dark@mozilla.org");

  // Check that themes are in the expected spots.
  is(defaultTheme.parentNode, enabledSection, "The default theme is enabled");
  is(darkTheme.parentNode, disabledSection, "The dark theme is disabled");

  // The default theme shouldn't have remove or disable options.
  let defaultButtons = {
    toggleDisabled: defaultTheme.querySelector('[action="toggle-disabled"]'),
    remove: defaultTheme.querySelector('[action="remove"]'),
  };
  is(defaultButtons.toggleDisabled.hidden, true, "Disable is hidden");
  is(defaultButtons.remove.hidden, true, "Remove is hidden");

  // The dark theme should have an enable button, but not remove.
  let darkButtons = {
    toggleDisabled: darkTheme.querySelector('[action="toggle-disabled"]'),
    remove: darkTheme.querySelector('[action="remove"]'),
  };
  is(darkButtons.toggleDisabled.hidden, false, "Enable is visible");
  is(darkButtons.remove.hidden, true, "Remove is hidden");

  // Enable the dark theme and check the buttons again.
  let themesChanged = waitForThemeChange(list);
  darkButtons.toggleDisabled.click();
  await themesChanged;

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  // Check the buttons.
  is(defaultButtons.toggleDisabled.hidden, false, "Enable is visible");
  is(defaultButtons.remove.hidden, true, "Remove is hidden");
  is(darkButtons.toggleDisabled.hidden, false, "Disable is visible");
  is(darkButtons.remove.hidden, true, "Remove is hidden");

  // Disable the dark theme.
  themesChanged = waitForThemeChange(list);
  darkButtons.toggleDisabled.click();
  await themesChanged;

  await TestUtils.waitForCondition(
    () => enabledSection.querySelectorAll("addon-card").length == 1
  );

  // The themes are back to their starting posititons.
  is(defaultTheme.parentNode, enabledSection, "Default is enabled");
  is(darkTheme.parentNode, disabledSection, "Dark is disabled");

  await closeView(win);
});

add_task(async function testSideloadRemoveButton() {
  const id = "sideload@mochi.test";
  mockProvider.createAddons([
    {
      id,
      name: "Sideloaded",
      permissions: 0,
    },
  ]);

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getCardByAddonId(doc, id);

  let moreOptionsPanel = card.querySelector("panel-list");
  let moreOptionsButton = card.querySelector('[action="more-options"]');
  let panelOpened = BrowserTestUtils.waitForEvent(moreOptionsPanel, "shown");
  EventUtils.synthesizeMouseAtCenter(moreOptionsButton, {}, win);
  await panelOpened;

  // Verify the remove button is visible with a SUMO link.
  let removeButton = card.querySelector('[action="remove"]');
  ok(removeButton.disabled, "Remove is disabled");
  ok(!removeButton.hidden, "Remove is visible");

  let sumoLink = removeButton.querySelector("a");
  ok(sumoLink, "There's a link");
  is(
    doc.l10n.getAttributes(removeButton).id,
    "remove-addon-disabled-button",
    "The can't remove text is shown"
  );
  sumoLink.focus();
  is(doc.activeElement, sumoLink, "The link can be focused");

  let newTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, REMOVE_SUMO_URL);
  sumoLink.click();
  BrowserTestUtils.removeTab(await newTabOpened);

  await closeView(win);
});

add_task(async function testOnlyTypeIsShown() {
  let win = await loadInitialView("theme");
  let doc = win.document;

  // Find the addon-list to listen for events.
  let list = doc.querySelector("addon-list");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id: "test@mochi.test" } },
    },
    useAddonManager: "temporary",
  });

  let skipped = BrowserTestUtils.waitForEvent(
    list,
    "skip-add",
    e => e.detail == "type-mismatch"
  );
  await extension.startup();
  await skipped;

  let cards = getTestCards(list);
  is(cards.length, 0, "There are no test extension cards");

  await extension.unload();
  await closeView(win);
});

add_task(async function testPluginIcons() {
  const pluginIconUrl = "chrome://global/skin/plugins/pluginGeneric.svg";

  let win = await loadInitialView("plugin");
  let doc = win.document;

  // Check that the icons are set to the plugin icon.
  let icons = doc.querySelectorAll(".card-heading-icon");
  ok(!!icons.length, "There are some plugins listed");

  for (let icon of icons) {
    is(icon.src, pluginIconUrl, "Plugins use the plugin icon");
  }

  await closeView(win);
});

add_task(async function testExtensionGenericIcon() {
  const extensionIconUrl =
    "chrome://mozapps/skin/extensions/extensionGeneric.svg";

  let id = "test@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test extension",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getCardByAddonId(doc, id);
  let icon = card.querySelector(".addon-icon");
  is(icon.src, extensionIconUrl, "Extensions without icon use the generic one");

  await extension.unload();
  await closeView(win);
});

add_task(async function testSectionHeadingKeys() {
  mockProvider.createAddons([
    {
      id: "test-theme",
      name: "Test Theme",
      type: "theme",
    },
    {
      id: "test-extension-disabled",
      name: "Test Disabled Extension",
      type: "extension",
      userDisabled: true,
    },
    {
      id: "test-plugin-disabled",
      name: "Test Disabled Plugin",
      type: "plugin",
      userDisabled: true,
    },
    {
      id: "test-locale",
      name: "Test Enabled Locale",
      type: "locale",
    },
    {
      id: "test-locale-disabled",
      name: "Test Disabled Locale",
      type: "locale",
      userDisabled: true,
    },
    {
      id: "test-dictionary",
      name: "Test Enabled Dictionary",
      type: "dictionary",
    },
    {
      id: "test-dictionary-disabled",
      name: "Test Disabled Dictionary",
      type: "dictionary",
      userDisabled: true,
    },
  ]);

  for (let type of ["extension", "theme", "plugin", "locale", "dictionary"]) {
    let win = await loadInitialView(type);
    let doc = win.document;

    for (let status of ["enabled", "disabled"]) {
      let section = getSection(doc, status);
      let el = section.querySelector(".list-section-heading");
      isnot(el, null, "Should have heading present");
      is(
        doc.l10n.getAttributes(el).id,
        `${type}-${status}-heading`,
        `Should have correct ${status} heading for ${type} section`
      );
    }

    await closeView(win);
  }
});

add_task(async function testDisabledDimming() {
  const id = "disabled@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Disable me",
      applications: { gecko: { id } },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let addon = await AddonManager.getAddonByID(id);

  let win = await loadInitialView("extension");
  let doc = win.document;

  const checkOpacity = (card, expected, msg) => {
    let { opacity } = card.ownerGlobal.getComputedStyle(card.firstElementChild);
    is(opacity, expected, msg);
  };
  const waitForTransition = card =>
    BrowserTestUtils.waitForEvent(
      card.firstElementChild,
      "transitionend",
      e => e.propertyName === "opacity"
    );

  let card = getCardByAddonId(doc, id);
  checkOpacity(card, "1", "The opacity is 1 when enabled");

  // Disable the add-on, check again.
  await addon.disable();
  checkOpacity(card, "0.6", "The opacity is dimmed when disabled");

  // Click on the menu button, this should un-dim the card.
  let transitionEnded = waitForTransition(card);
  card.panel.open = true;
  await transitionEnded;
  checkOpacity(card, "1", "The opacity is 1 when the menu is open");

  // Close the menu, opacity should return.
  transitionEnded = waitForTransition(card);
  card.panel.open = false;
  await transitionEnded;
  checkOpacity(card, "0.6", "The card is dimmed again");

  await closeView(win);
  await extension.unload();
});
