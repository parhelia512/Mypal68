/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG11/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=Window]
interface SVGPathSegList {
  readonly attribute unsigned long numberOfItems;
  [Throws]
  getter SVGPathSeg getItem(unsigned long index);

  // Mozilla-specific stuff
  readonly attribute unsigned long length; // synonym for numberOfItems
};
