/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Exposed=Window]
interface XULMenuElement : XULElement {
  [HTMLConstructor] constructor();

  attribute Element? activeChild;

  boolean handleKeyPress(KeyboardEvent keyEvent);

  readonly attribute boolean openedWithKey;

};
