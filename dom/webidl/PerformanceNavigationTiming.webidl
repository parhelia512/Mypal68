/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/navigation-timing/#sec-PerformanceNavigationTiming
 *
 * Copyright © 2016 W3C® (MIT, ERCIM, Keio, Beihang).
 * W3C liability, trademark and document use rules apply.
 */

enum NavigationType {
  "navigate",
  "reload",
  "back_forward",
  "prerender"
};

[Exposed=Window,
 Func="mozilla::dom::PerformanceNavigationTiming::Enabled"]
interface PerformanceNavigationTiming : PerformanceResourceTiming {
  readonly        attribute DOMHighResTimeStamp unloadEventStart;
  readonly        attribute DOMHighResTimeStamp unloadEventEnd;
  readonly        attribute DOMHighResTimeStamp domInteractive;
  readonly        attribute DOMHighResTimeStamp domContentLoadedEventStart;
  readonly        attribute DOMHighResTimeStamp domContentLoadedEventEnd;
  readonly        attribute DOMHighResTimeStamp domComplete;
  readonly        attribute DOMHighResTimeStamp loadEventStart;
  readonly        attribute DOMHighResTimeStamp loadEventEnd;
  readonly        attribute NavigationType      type;
  readonly        attribute unsigned short      redirectCount;

  [Default] object toJSON();
};
