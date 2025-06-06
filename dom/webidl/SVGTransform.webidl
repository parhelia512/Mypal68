/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.w3.org/TR/SVG2/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

[Exposed=Window]
interface SVGTransform {

  // Transform Types
  const unsigned short SVG_TRANSFORM_UNKNOWN = 0;
  const unsigned short SVG_TRANSFORM_MATRIX = 1;
  const unsigned short SVG_TRANSFORM_TRANSLATE = 2;
  const unsigned short SVG_TRANSFORM_SCALE = 3;
  const unsigned short SVG_TRANSFORM_ROTATE = 4;
  const unsigned short SVG_TRANSFORM_SKEWX = 5;
  const unsigned short SVG_TRANSFORM_SKEWY = 6;

  readonly attribute unsigned short type;
  [BinaryName="getMatrix"]
  readonly attribute SVGMatrix matrix;
  readonly attribute float angle;

  [Throws]
  void setMatrix(optional DOMMatrix2DInit matrix = {});
  [Throws]
  void setTranslate(float tx, float ty);
  [Throws]
  void setScale(float sx, float sy);
  [Throws]
  void setRotate(float angle, float cx, float cy);
  [Throws]
  void setSkewX(float angle);
  [Throws]
  void setSkewY(float angle);
};

