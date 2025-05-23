/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary PerformanceEntryEventInit : EventInit
{
  DOMString name = "";
  DOMString entryType = "";
  DOMHighResTimeStamp startTime = 0;
  DOMHighResTimeStamp duration = 0;
  double epoch = 0;
  DOMString origin = "";
};

[ChromeOnly,
 Exposed=Window]
interface PerformanceEntryEvent : Event
{
  constructor(DOMString type,
              optional PerformanceEntryEventInit eventInitDict = {});

  readonly attribute DOMString name;
  readonly attribute DOMString entryType;
  readonly attribute DOMHighResTimeStamp startTime;
  readonly attribute DOMHighResTimeStamp duration;
  readonly attribute double epoch;
  readonly attribute DOMString origin;
};
