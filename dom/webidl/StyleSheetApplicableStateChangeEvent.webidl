/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Exposed=Window]
interface StyleSheetApplicableStateChangeEvent : Event
{
  constructor(DOMString type,
              optional StyleSheetApplicableStateChangeEventInit eventInitDict = {});

  readonly attribute CSSStyleSheet? stylesheet;
  readonly attribute boolean applicable;
};

dictionary StyleSheetApplicableStateChangeEventInit : EventInit
{
  CSSStyleSheet? stylesheet = null;
  boolean applicable = false;
};
