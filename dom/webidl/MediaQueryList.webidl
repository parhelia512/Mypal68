/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://drafts.csswg.org/cssom-view/#mediaquerylist
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[ProbablyShortLivingWrapper,
 Exposed=Window]
interface MediaQueryList : EventTarget {
  readonly attribute UTF8String media;
  readonly attribute boolean matches;

  [Throws]
  void addListener(EventListener? listener);

  [Throws]
  void removeListener(EventListener? listener);

           attribute EventHandler onchange;
};
