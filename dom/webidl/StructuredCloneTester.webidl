/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=(Window,Worker),
 Pref="dom.testing.structuredclonetester.enabled",
 Serializable]
interface StructuredCloneTester {
  constructor(boolean serializable, boolean deserializable);

  readonly attribute boolean serializable;
  readonly attribute boolean deserializable;
};
