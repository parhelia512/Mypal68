/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dom.spec.whatwg.org/#interface-parentnode
 */

interface mixin ParentNode {
  [Constant]
  readonly attribute HTMLCollection children;
  [Pure]
  readonly attribute Element? firstElementChild;
  [Pure]
  readonly attribute Element? lastElementChild;
  [Pure]
  readonly attribute unsigned long childElementCount;

  [ChromeOnly]
  HTMLCollection getElementsByAttribute(DOMString name,
                                        [TreatNullAs=EmptyString] DOMString value);
  [ChromeOnly, Throws]
  HTMLCollection getElementsByAttributeNS(DOMString? namespaceURI, DOMString name,
                                          [TreatNullAs=EmptyString] DOMString value);

  [CEReactions, Throws, Unscopable]
  void prepend((Node or DOMString)... nodes);
  [CEReactions, Throws, Unscopable]
  void append((Node or DOMString)... nodes);
  [CEReactions, Throws, Unscopable]
  void replaceChildren((Node or DOMString)... nodes);
};
