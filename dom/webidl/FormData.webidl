/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://xhr.spec.whatwg.org
 */

typedef (Blob or Directory or USVString) FormDataEntryValue;

[Exposed=(Window,Worker)]
interface FormData {
  [Throws]
  constructor(optional HTMLFormElement form);

  [Throws]
  void append(USVString name, Blob value, optional USVString filename);
  [Throws]
  void append(USVString name, USVString value);
  void delete(USVString name);
  FormDataEntryValue? get(USVString name);
  sequence<FormDataEntryValue> getAll(USVString name);
  boolean has(USVString name);
  [Throws]
  void set(USVString name, Blob value, optional USVString filename);
  [Throws]
  void set(USVString name, USVString value);
  iterable<USVString, FormDataEntryValue>;
};
