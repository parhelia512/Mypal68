/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=(Window,Worker)]
interface PromiseRejectionEvent : Event
{
  constructor(DOMString type, PromiseRejectionEventInit eventInitDict);

  [BinaryName="rejectedPromise"]
  readonly attribute Promise<any> promise;
  readonly attribute any reason;
};

dictionary PromiseRejectionEventInit : EventInit {
  required Promise<any> promise;
           any reason;
};
