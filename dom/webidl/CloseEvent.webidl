/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The nsIDOMCloseEvent interface is the interface to the event
 * close on a WebSocket object.
 *
 * For more information on this interface, please see
 * http://www.whatwg.org/specs/web-apps/current-work/multipage/network.html#closeevent
 */

[LegacyEventInit,
 Exposed=(Window,Worker)]
interface CloseEvent : Event
{
  constructor(DOMString type, optional CloseEventInit eventInitDict = {});

  readonly attribute boolean wasClean;
  readonly attribute unsigned short code;
  readonly attribute DOMString reason;
};

dictionary CloseEventInit : EventInit
{
  boolean wasClean = false;
  unsigned short code = 0;
  DOMString reason = "";
};
