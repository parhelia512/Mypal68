/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/mediacapture-record/
 *
 * Copyright © 2017 W3C® (MIT, ERCIM, Keio, Beihang). W3C liability, trademark
 * and document use rules apply.
 */

dictionary MediaRecorderErrorEventInit : EventInit {
  required DOMException error;
};

[Exposed=Window]
interface MediaRecorderErrorEvent : Event {
  constructor(DOMString type, MediaRecorderErrorEventInit eventInitDict);

  [SameObject] readonly attribute DOMException error;
};
