/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-midi-api/
 */

[SecureContext, Pref="dom.webmidi.enabled",
 Exposed=Window]
interface MIDIAccess : EventTarget {
  readonly attribute MIDIInputMap  inputs;
  readonly attribute MIDIOutputMap outputs;
           attribute EventHandler  onstatechange;
  readonly attribute boolean       sysexEnabled;
};
