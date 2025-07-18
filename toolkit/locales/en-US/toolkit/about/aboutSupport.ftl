# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

page-title = Troubleshooting Information
page-subtitle =
    This page contains technical information that might be useful when you’re
    trying to solve a problem. If you are looking for answers to common questions
    about { -brand-short-name }, check out our <a data-l10n-name="support-link">support website</a>.

crashes-title = Crash Reports
crashes-id = Report ID
crashes-send-date = Submitted
crashes-all-reports = All Crash Reports
crashes-no-config = This application has not been configured to display crash reports.
extensions-title = Extensions
extensions-name = Name
extensions-enabled = Enabled
extensions-version = Version
extensions-id = ID
security-software-title = Security Software
security-software-type = Type
security-software-name = Name
security-software-antivirus = Antivirus
security-software-antispyware = Antispyware
security-software-firewall = Firewall
features-title = { -brand-short-name } Features
features-name = Name
features-version = Version
features-id = ID
processes-title = Remote Processes
processes-type = Type
processes-count = Count
app-basics-title = Application Basics
app-basics-name = Name
app-basics-version = Version
app-basics-build-id = Build ID
app-basics-update-channel = Update Channel
app-basics-update-history = Update History
app-basics-show-update-history = Show Update History
app-basics-profile-dir =
    { PLATFORM() ->
        [linux] Profile Directory
       *[other] Profile Folder
    }
app-basics-enabled-plugins = Enabled Plugins
app-basics-build-config = Build Configuration
app-basics-user-agent = User Agent
app-basics-os = OS
app-basics-memory-use = Memory Use
app-basics-performance = Performance
app-basics-service-workers = Registered Service Workers
app-basics-profiles = Profiles
app-basics-launcher-process-status = Launcher Process
app-basics-multi-process-support = Multiprocess Windows
app-basics-remote-processes-count = Remote Processes
app-basics-enterprise-policies = Enterprise Policies
app-basics-location-service-key-google = Google Location Service Key
app-basics-safebrowsing-key-google = Google Safebrowsing Key
app-basics-key-mozilla = Mozilla Location Service Key
app-basics-safe-mode = Safe Mode
show-dir-label =
    { PLATFORM() ->
        [macos] Show in Finder
        [windows] Open Folder
       *[other] Open Directory
    }
modified-key-prefs-title = Important Modified Preferences
modified-prefs-name = Name
modified-prefs-value = Value
user-js-title = user.js Preferences
user-js-description = Your profile folder contains a <a data-l10n-name="user-js-link">user.js file</a>, which includes preferences that were not created by { -brand-short-name }.
locked-key-prefs-title = Important Locked Preferences
locked-prefs-name = Name
locked-prefs-value = Value
graphics-title = Graphics
graphics-features-title = Features
graphics-diagnostics-title = Diagnostics
graphics-failure-log-title = Failure Log
graphics-gpu1-title = GPU #1
graphics-gpu2-title = GPU #2
graphics-decision-log-title = Decision Log
graphics-crash-guards-title = Crash Guard Disabled Features
graphics-workarounds-title = Workarounds
# Windowing system in use on Linux (e.g. X11, Wayland).
graphics-window-protocol = Window Protocol
place-database-title = Places Database
place-database-integrity = Integrity
place-database-verify-integrity = Verify Integrity
a11y-title = Accessibility
a11y-activated = Activated
a11y-force-disabled = Prevent Accessibility
a11y-handler-used = Accessible Handler Used
a11y-instantiator = Accessibility Instantiator
library-version-title = Library Versions
copy-text-to-clipboard-label = Copy text to clipboard
copy-raw-data-to-clipboard-label = Copy raw data to clipboard
sandbox-title = Sandbox
sandbox-sys-call-log-title = Rejected System Calls
sandbox-sys-call-index = #
sandbox-sys-call-age = Seconds Ago
sandbox-sys-call-pid = PID
sandbox-sys-call-tid = TID
sandbox-sys-call-proc-type = Process Type
sandbox-sys-call-number = Syscall
sandbox-sys-call-args = Arguments
safe-mode-title = Try Safe Mode
restart-in-safe-mode-label = Restart with Add-ons Disabled…
media-title = Media
media-output-devices-title = Output Devices
media-input-devices-title = Input Devices
media-device-name = Name
media-device-group = Group
media-device-vendor = Vendor
media-device-state = State
media-device-preferred = Preferred
media-device-format = Format
media-device-channels = Channels
media-device-rate = Rate
media-device-latency = Latency
intl-title = Internationalization & Localization
intl-app-title = Application Settings
intl-locales-requested = Requested Locales
intl-locales-available = Available Locales
intl-locales-supported = App Locales
intl-locales-default = Default Locale
intl-os-title = Operating System
intl-os-prefs-system-locales = System Locales
intl-regional-prefs = Regional Preferences

# Variables
# $days (Integer) - Number of days of crashes to log
report-crash-for-days =
    { $days ->
        [one] Crash Reports for the Last { $days } Day
       *[other] Crash Reports for the Last { $days } Days
    }

# Variables
# $minutes (integer) - Number of minutes since crash
crashes-time-minutes =
    { $minutes ->
        [one] { $minutes } minute ago
       *[other] { $minutes } minutes ago
    }

# Variables
# $hours (integer) - Number of hours since crash
crashes-time-hours =
    { $hours ->
        [one] { $hours } hour ago
       *[other] { $hours } hours ago
    }

# Variables
# $days (integer) - Number of days since crash
crashes-time-days =
    { $days ->
        [one] { $days } day ago
       *[other] { $days } days ago
    }

# Variables
# $reports (integer) - Number of pending reports
pending-reports =
    { $reports ->
        [one] All Crash Reports (including { $reports } pending crash in the given time range)
       *[other] All Crash Reports (including { $reports } pending crashes in the given time range)
    }

raw-data-copied = Raw data copied to clipboard
text-copied = Text copied to clipboard

## The verb "blocked" here refers to a graphics feature such as "Direct2D" or "OpenGL layers".
blocked-driver = Blocked for your graphics driver version.
blocked-gfx-card = Blocked for your graphics card because of unresolved driver issues.
blocked-os-version = Blocked for your operating system version.
blocked-mismatched-version = Blocked for your graphics driver version mismatch between registry and DLL.
# Variables
# $driverVersion - The graphics driver version string
try-newer-driver = Blocked for your graphics driver version. Try updating your graphics driver to version { $driverVersion } or newer.

# "ClearType" is a proper noun and should not be translated. Feel free to leave English strings if
# there are no good translations, these are only used in about:support
clear-type-parameters = ClearType Parameters

compositing = Compositing
hardware-h264 = Hardware H264 Decoding
main-thread-no-omtc = main thread, no OMTC
yes = Yes
no = No
unknown = Unknown
virtual-monitor-disp = Virtual Monitor Display

## The following strings indicate if an API key has been found.
## In some development versions, it's expected for some API keys that they are
## not found.
found = Found
missing = Missing

gpu-process-pid = GPUProcessPid
gpu-process = GPUProcess
gpu-description = Description
gpu-vendor-id = Vendor ID
gpu-device-id = Device ID
gpu-subsys-id = Subsys ID
gpu-drivers = Drivers
gpu-ram = RAM
gpu-driver-vendor = Driver Vendor
gpu-driver-version = Driver Version
gpu-driver-date = Driver Date
gpu-active = Active
webgl1-wsiinfo = WebGL 1 Driver WSI Info
webgl1-renderer = WebGL 1 Driver Renderer
webgl1-version = WebGL 1 Driver Version
webgl1-driver-extensions = WebGL 1 Driver Extensions
webgl1-extensions = WebGL 1 Extensions
webgl2-wsiinfo = WebGL 2 Driver WSI Info
webgl2-renderer = WebGL 2 Driver Renderer
webgl2-version = WebGL 2 Driver Version
webgl2-driver-extensions = WebGL 2 Driver Extensions
webgl2-extensions = WebGL 2 Extensions
blocklisted-bug = Blocklisted due to known issues

# Variables
# $bugNumber (string) - String of bug number from Bugzilla
bug-link = bug { $bugNumber }

# Variables
# $failureCode (string) - String that can be searched in the source tree.
unknown-failure = Blocklisted; failure code { $failureCode }

d3d11layers-crash-guard = D3D11 Compositor
d3d11video-crash-guard = D3D11 Video Decoder
d3d9video-crash-buard = D3D9 Video Decoder
glcontext-crash-guard = OpenGL

reset-on-next-restart = Reset on Next Restart
gpu-process-kill-button = Terminate GPU Process
gpu-device-reset = Device Reset
gpu-device-reset-button = Trigger Device Reset
uses-tiling = Uses Tiling
content-uses-tiling = Uses Tiling (Content)
off-main-thread-paint-enabled = Off Main Thread Painting Enabled
off-main-thread-paint-worker-count = Off Main Thread Painting Worker Count
target-frame-rate = Target Frame Rate

audio-backend = Audio Backend
max-audio-channels = Max Channels
channel-layout = Preferred Channel Layout
sample-rate = Preferred Sample Rate

min-lib-versions = Expected minimum version
loaded-lib-versions = Version in use

has-seccomp-bpf = Seccomp-BPF (System Call Filtering)
has-seccomp-tsync = Seccomp Thread Synchronization
has-user-namespaces = User Namespaces
has-privileged-user-namespaces = User Namespaces for privileged processes
can-sandbox-content = Content Process Sandboxing
can-sandbox-media = Media Plugin Sandboxing
content-sandbox-level = Content Process Sandbox Level
effective-content-sandbox-level = Effective Content Process Sandbox Level
sandbox-proc-type-content = content
sandbox-proc-type-file = file content
sandbox-proc-type-media-plugin = media plugin
sandbox-proc-type-data-decoder = data decoder

launcher-process-status-0 = Enabled
launcher-process-status-1 = Disabled due to failure
launcher-process-status-2 = Disabled forcibly
launcher-process-status-unknown = Unknown status

# Variables
# $remoteWindows (integer) - Number of remote windows
# $totalWindows (integer) - Number of total windows
multi-process-windows = { $remoteWindows }/{ $totalWindows }
multi-process-status-0 = Enabled by user
multi-process-status-1 = Enabled by default
multi-process-status-2 = Disabled
multi-process-status-4 = Disabled by accessibility tools
multi-process-status-6 = Disabled by unsupported text input
multi-process-status-7 = Disabled by add-ons
multi-process-status-8 = Disabled forcibly
multi-process-status-unknown = Unknown status

async-pan-zoom = Asynchronous Pan/Zoom
apz-none = none
wheel-enabled = wheel input enabled
touch-enabled = touch input enabled
drag-enabled = scrollbar drag enabled
keyboard-enabled = keyboard enabled
autoscroll-enabled = autoscroll enabled

## Variables
## $preferenceKey (string) - String ID of preference
wheel-warning = async wheel input disabled due to unsupported pref: { $preferenceKey }
touch-warning = async touch input disabled due to unsupported pref: { $preferenceKey }

## Strings representing the status of the Enterprise Policies engine.
policies-inactive = Inactive
policies-active = Active
policies-error = Error
