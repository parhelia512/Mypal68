/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/gamepad/#gamepadevent-interface
 */

[Pref="dom.gamepad.enabled",
 Exposed=Window]
interface GamepadEvent : Event
{
  constructor(DOMString type, optional GamepadEventInit eventInitDict = {});

  readonly attribute Gamepad? gamepad;
};

dictionary GamepadEventInit : EventInit
{
  Gamepad? gamepad = null;
};
