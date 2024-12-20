/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[LegacyUnenumerableNamedProperties,
 Exposed=Window]
interface Plugin {
  readonly attribute DOMString description;
  readonly attribute DOMString filename;
  readonly attribute DOMString version;
  readonly attribute DOMString name;

  readonly attribute unsigned long length;
  getter MimeType? item(unsigned long index);
  getter MimeType? namedItem(DOMString name);
};
