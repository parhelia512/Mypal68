# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

addons-window =
    .title = Add-ons Manager
addons-page-title = Add-ons Manager

search-header =
    .placeholder = Search addons.mozilla.org
    .searchbuttonlabel = Search

search-header-shortcut =
    .key = f

loading-label =
    .value = Loading…

list-empty-installed =
    .value = You don’t have any add-ons of this type installed

list-empty-available-updates =
    .value = No updates found

list-empty-recent-updates =
    .value = You haven’t recently updated any add-ons

list-empty-find-updates =
    .label = Check For Updates

list-empty-button =
    .label = Learn more about add-ons

help-button = Add-ons Support

preferences =
    { PLATFORM() ->
        [windows] { -brand-short-name } Options
       *[other] { -brand-short-name } Preferences
    }

show-all-extensions-button =
    .label = Show all extensions

cmd-show-details =
    .label = Show More Information
    .accesskey = S

cmd-find-updates =
    .label = Find Updates
    .accesskey = F

cmd-preferences =
    .label =
        { PLATFORM() ->
            [windows] Options
           *[other] Preferences
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] P
        }

cmd-enable-theme =
    .label = Wear Theme
    .accesskey = W

cmd-disable-theme =
    .label = Stop Wearing Theme
    .accesskey = W

cmd-install-addon =
    .label = Install
    .accesskey = I

cmd-contribute =
    .label = Contribute
    .accesskey = C
    .tooltiptext = Contribute to the development of this add-on

discover-title = What are Add-ons?

discover-description =
    Add-ons are applications that let you personalize { -brand-short-name } with
    extra functionality or style. Try a time-saving sidebar, a weather notifier, or a themed look to make { -brand-short-name }
    your own.

discover-footer =
    When you’re connected to the internet, this pane will feature
    some of the best and most popular add-ons for you to try out.

detail-version =
    .label = Version

detail-last-updated =
    .label = Last Updated

detail-contributions-description = The developer of this add-on asks that you help support its continued development by making a small contribution.

detail-contributions-button = Contribute
    .title = Contribute to the development of this add-on
    .accesskey = C

detail-update-type =
    .value = Automatic Updates

detail-update-default =
    .label = Default
    .tooltiptext = Automatically install updates only if that’s the default

detail-update-automatic =
    .label = On
    .tooltiptext = Automatically install updates

detail-update-manual =
    .label = Off
    .tooltiptext = Don’t automatically install updates

# Used as a description for the option to allow or block an add-on in private windows.
detail-private-browsing-label = Run in Private Windows

detail-private-browsing-description2 = When allowed, the extension will have access to your online activities while private browsing. <label data-l10n-name="detail-private-browsing-learn-more">Learn more</label>

# Some add-ons may elect to not run in private windows by setting incognito: not_allowed in the manifest.  This
# cannot be overriden by the user.
detail-private-disallowed-label = Not Allowed in Private Windows
detail-private-disallowed-description = This extension does not run while private browsing. <label data-l10n-name="detail-private-browsing-learn-more">Learn more</label>

# Some special add-ons are privileged, run in private windows automatically, and this permission can't be revoked
detail-private-required-label = Requires Access to Private Windows
detail-private-required-description = This extension has access to your online activities while private browsing. <label data-l10n-name="detail-private-browsing-learn-more">Learn more</label>

detail-private-browsing-on =
    .label = Allow
    .tooltiptext = Enable in Private Browsing

detail-private-browsing-off =
    .label = Don’t Allow
    .tooltiptext = Disable in Private Browsing

detail-home =
    .label = Homepage

detail-home-value =
    .value = { detail-home.label }

detail-repository =
    .label = Add-on Profile

detail-repository-value =
    .value = { detail-repository.label }

detail-check-for-updates =
    .label = Check for Updates
    .accesskey = U
    .tooltiptext = Check for updates for this add-on

detail-show-preferences =
    .label =
        { PLATFORM() ->
            [windows] Options
           *[other] Preferences
        }
    .accesskey =
        { PLATFORM() ->
            [windows] O
           *[other] P
        }
    .tooltiptext =
        { PLATFORM() ->
            [windows] Change this add-on’s options
           *[other] Change this add-on’s preferences
        }

detail-rating =
    .value = Rating

addon-restart-now =
    .label = Restart now

plugin-deprecation-description =
    Missing something? Some plugins are no longer supported by { -brand-short-name }. <label data-l10n-name="learn-more">Learn More.</label>

private-browsing-description2 =
    { -brand-short-name } is changing how extensions work in private browsing. Any new extensions you add to
    { -brand-short-name } won’t run by default in Private Windows. Unless you allow it in settings, the
    extension won’t work while private browsing, and won’t have access to your online activities
    there. We’ve made this change to keep your private browsing private.
    <label data-l10n-name="private-browsing-learn-more">Learn how to manage extension settings</label>

extensions-view-recent-updates =
    .name = Recent Updates
    .tooltiptext = { extensions-view-recent-updates.name }

extensions-view-available-updates =
    .name = Available Updates
    .tooltiptext = { extensions-view-available-updates.name }

## These are global warnings

extensions-warning-safe-mode = All add-ons have been disabled by safe mode.
extensions-warning-check-compatibility = Add-on compatibility checking is disabled. You may have incompatible add-ons.
extensions-warning-check-compatibility-button = Enable
    .title = Enable add-on compatibility checking
extensions-warning-update-security = Add-on update security checking is disabled. You may be compromised by updates.
extensions-warning-update-security-button = Enable
    .title = Enable add-on update security checking


## Strings connected to add-on updates

addon-updates-check-for-updates = Check for Updates
    .accesskey = C
addon-updates-view-updates = View Recent Updates
    .accesskey = V

# This menu item is a checkbox that toggles the default global behavior for
# add-on update checking.

addon-updates-update-addons-automatically = Update Add-ons Automatically
    .accesskey = A

## Specific add-ons can have custom update checking behaviors ("Manually",
## "Automatically", "Use default global behavior"). These menu items reset the
## update checking behavior for all add-ons to the default global behavior
## (which itself is either "Automatically" or "Manually", controlled by the
## extensions-updates-update-addons-automatically.label menu item).

addon-updates-reset-updates-to-automatic = Reset All Add-ons to Update Automatically
    .accesskey = R
addon-updates-reset-updates-to-manual = Reset All Add-ons to Update Manually
    .accesskey = R

## Status messages displayed when updating add-ons

addon-updates-updating = Updating add-ons
addon-updates-installed = Your add-ons have been updated.
addon-updates-none-found = No updates found
addon-updates-manual-updates-found = View Available Updates

## Add-on install/debug strings for page options menu

addon-install-from-file = Install Add-on From File…
    .accesskey = I
addon-install-from-file-dialog-title = Select add-on to install
addon-install-from-file-filter-name = Add-ons
addon-open-about-debugging = Debug Add-ons
    .accesskey = b

## Extension shortcut management

# This is displayed in the page options menu
addon-manage-extensions-shortcuts = Manage Extension Shortcuts
    .accesskey = S

shortcuts-no-addons = You don’t have any extensions enabled.
shortcuts-no-commands = The following extensions do not have shortcuts:
shortcuts-input =
  .placeholder = Type a shortcut

shortcuts-browserAction = Activate extension
shortcuts-pageAction = Activate page action
shortcuts-sidebarAction = Toggle the sidebar

shortcuts-modifier-mac = Include Ctrl, Alt, or ⌘
shortcuts-modifier-other = Include Ctrl or Alt
shortcuts-invalid = Invalid combination
shortcuts-letter = Type a letter
shortcuts-system = Can’t override a { -brand-short-name } shortcut

# String displayed in warning label when there is a duplicate shortcut
shortcuts-duplicate = Duplicate shortcut

# String displayed when a keyboard shortcut is already assigned to more than one add-on
# Variables:
#   $shortcut (string) - Shortcut string for the add-on
shortcuts-duplicate-warning-message = { $shortcut } is being used as a shortcut in more than one case. Duplicate shortcuts may cause unexpected behavior.

# String displayed when a keyboard shortcut is already used by another add-on
# Variables:
#   $addon (string) - Name of the add-on
shortcuts-exists = Already in use by { $addon }

shortcuts-card-expand-button =
    { $numberToShow ->
        *[other] Show { $numberToShow } More
    }

shortcuts-card-collapse-button = Show Less

go-back-button =
    .tooltiptext = Go back

privacy-policy = Privacy Policy

# Refers to the author of an add-on, shown below the name of the add-on.
# Variables:
#   $author (string) - The name of the add-on developer.
created-by-author = by <a data-l10n-name="author">{ $author }</a>
# Shows the number of daily users of the add-on.
# Variables:
#   $dailyUsers (number) - The number of daily users.
user-count = Users: { $dailyUsers }
install-extension-button = Add to { -brand-product-name }
install-theme-button = Install Theme
# The label of the button that appears after installing an add-on. Upon click,
# the detailed add-on view is opened, from where the add-on can be managed.
manage-addon-button = Manage
find-more-addons = Find more add-ons

# This is a label for the button to open the "more options" menu, it is only
# used for screen readers.
addon-options-button =
    .aria-label = More Options

## Add-on actions
remove-addon-button = Remove
# The link will always be shown after the other text.
remove-addon-disabled-button = Can’t Be Removed <a data-l10n-name="link">Why?</a>
disable-addon-button = Disable
enable-addon-button = Enable
# This is used for the toggle on the extension card, it's a checkbox and this
# is always its label.
extension-enable-addon-button-label =
    .aria-label = Enable
preferences-addon-button =
    { PLATFORM() ->
        [windows] Options
       *[other] Preferences
    }
details-addon-button = Details
release-notes-addon-button = Release Notes
permissions-addon-button = Permissions

extension-enabled-heading = Enabled
extension-disabled-heading = Disabled

theme-enabled-heading = Enabled
theme-disabled-heading = Disabled

plugin-enabled-heading = Enabled
plugin-disabled-heading = Disabled

dictionary-enabled-heading = Enabled
dictionary-disabled-heading = Disabled

locale-enabled-heading = Enabled
locale-disabled-heading = Disabled

ask-to-activate-button = Ask to Activate
always-activate-button = Always Activate
never-activate-button = Never Activate

addon-detail-author-label = Author
addon-detail-version-label = Version
addon-detail-last-updated-label = Last Updated
addon-detail-homepage-label = Homepage

# The average rating that the add-on has received.
# Variables:
#   $rating (number) - A number between 0 and 5. The translation should show at most one digit after the comma.
five-star-rating =
  .title = Rated { NUMBER($rating, maximumFractionDigits: 1) } out of 5

# This string is used to show that an add-on is disabled.
# Variables:
#   $name (string) - The name of the add-on
addon-name-disabled = { $name } (disabled)

## Pending uninstall message bar

# Variables:
#   $addon (string) - Name of the add-on
pending-uninstall-description = <span data-l10n-name="addon-name">{ $addon }</span> has been removed.
pending-uninstall-undo-button = Undo

addon-detail-updates-label = Allow automatic updates
addon-detail-updates-radio-default = Default
addon-detail-updates-radio-on = On
addon-detail-updates-radio-off = Off
addon-detail-update-check-label = Check for Updates
install-update-button = Update

# This is the tooltip text for the private browsing badge in about:addons. The
# badge is the private browsing icon included next to the extension's name.
addon-badge-private-browsing-allowed =
    .title = Allowed in private windows
addon-detail-private-browsing-help = When allowed, the extension will have access to your online activities while private browsing. <a data-l10n-name="learn-more">Learn more</a>
addon-detail-private-browsing-allow = Allow
addon-detail-private-browsing-disallow = Don’t Allow

available-updates-heading = Available Updates
recent-updates-heading = Recent Updates

release-notes-loading = Loading…
release-notes-error = Sorry, but there was an error loading the release notes.

addon-permissions-empty = This extension doesn’t require any permissions

## Page headings

extension-heading = Manage Your Extensions
theme-heading = Manage Your Themes
plugin-heading = Manage Your Plugins
dictionary-heading = Manage Your Dictionaries
locale-heading = Manage Your Languages
updates-heading = Manage Your Updates
shortcuts-heading = Manage Extension Shortcuts

addon-page-options-button =
    .title = Tools for all add-ons
