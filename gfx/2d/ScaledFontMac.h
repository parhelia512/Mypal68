/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTMAC_H_
#define MOZILLA_GFX_SCALEDFONTMAC_H_

#ifdef MOZ_WIDGET_COCOA
#  include <ApplicationServices/ApplicationServices.h>
#else
#  include <CoreGraphics/CoreGraphics.h>
#  include <CoreText/CoreText.h>
#endif

#include "2D.h"

#include "ScaledFontBase.h"

namespace mozilla {
namespace gfx {

class ScaledFontMac : public ScaledFontBase {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontMac, override)
  ScaledFontMac(CGFontRef aFont, const RefPtr<UnscaledFont>& aUnscaledFont, Float aSize,
                bool aOwnsFont = false, const Color& aFontSmoothingBackgroundColor = Color(),
                bool aUseFontSmoothing = true, bool aApplySyntheticBold = false);
  ~ScaledFontMac();

  FontType GetType() const override { return FontType::MAC; }
#ifdef USE_SKIA
  SkTypeface* CreateSkTypeface() override;
#endif
  already_AddRefed<Path> GetPathForGlyphs(const GlyphBuffer& aBuffer,
                                          const DrawTarget* aTarget) override;

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

#ifdef MOZ_BUILD_WEBRENDER
  bool GetWRFontInstanceOptions(Maybe<wr::FontInstanceOptions>* aOutOptions,
                                Maybe<wr::FontInstancePlatformOptions>* aOutPlatformOptions,
                                std::vector<FontVariation>* aOutVariations) override;
#endif

  bool CanSerialize() override { return true; }

  Color FontSmoothingBackgroundColor() { return mFontSmoothingBackgroundColor; }

#ifdef USE_CAIRO_SCALED_FONT
  cairo_font_face_t* GetCairoFontFace() override;
#endif

 private:
  friend class DrawTargetSkia;
  CGFontRef mFont;
  CTFontRef mCTFont;  // only created if CTFontDrawGlyphs is available, otherwise null
  Color mFontSmoothingBackgroundColor;
  bool mUseFontSmoothing;
  bool mApplySyntheticBold;

  typedef void(CTFontDrawGlyphsFuncT)(CTFontRef, const CGGlyph[], const CGPoint[], size_t,
                                      CGContextRef);

  static bool sSymbolLookupDone;

 public:
  // function pointer for CTFontDrawGlyphs, if available;
  // initialized the first time a ScaledFontMac is created,
  // so it will be valid by the time DrawTargetCG wants to use it
  static CTFontDrawGlyphsFuncT* CTFontDrawGlyphsPtr;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTMAC_H_ */
