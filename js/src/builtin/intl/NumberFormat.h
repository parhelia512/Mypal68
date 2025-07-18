/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_intl_NumberFormat_h
#define builtin_intl_NumberFormat_h

#include "mozilla/Attributes.h"

#include <stdint.h>

#include "builtin/SelfHostingDefines.h"
#include "js/Class.h"
#include "vm/NativeObject.h"

namespace mozilla::intl {
class NumberFormat;
}

namespace js {

class NumberFormatObject : public NativeObject {
 public:
  static const JSClass class_;
  static const JSClass& protoClass_;

  static constexpr uint32_t INTERNALS_SLOT = 0;
  static constexpr uint32_t UNUMBER_FORMATTER_SLOT = 1;
  static constexpr uint32_t SLOT_COUNT = 2;

  static_assert(INTERNALS_SLOT == INTL_INTERNALS_OBJECT_SLOT,
                "INTERNALS_SLOT must match self-hosting define for internals "
                "object slot");

  // Estimated memory use for UNumberFormatter and UFormattedNumber
  // (see IcuMemoryUsage).
  static constexpr size_t EstimatedMemoryUse = 750;

  mozilla::intl::NumberFormat* getNumberFormatter() const {
    const auto& slot = getFixedSlot(UNUMBER_FORMATTER_SLOT);
    if (slot.isUndefined()) {
      return nullptr;
    }
    return static_cast<mozilla::intl::NumberFormat*>(slot.toPrivate());
  }

  void setNumberFormatter(mozilla::intl::NumberFormat* formatter) {
    setFixedSlot(UNUMBER_FORMATTER_SLOT, PrivateValue(formatter));
  }

 private:
  static const JSClassOps classOps_;
  static const ClassSpec classSpec_;

  static void finalize(JSFreeOp* fop, JSObject* obj);
};

/**
 * Returns a new instance of the standard built-in NumberFormat constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: numberFormat = intl_NumberFormat(locales, options)
 */
[[nodiscard]] extern bool intl_NumberFormat(JSContext* cx, unsigned argc,
                                            Value* vp);

/**
 * Returns the numbering system type identifier per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * default numbering system for the given locale.
 *
 * Usage: defaultNumberingSystem = intl_numberingSystem(locale)
 */
[[nodiscard]] extern bool intl_numberingSystem(JSContext* cx, unsigned argc,
                                               Value* vp);

/**
 * Returns a string representing the number x according to the effective
 * locale and the formatting options of the given NumberFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.2.
 *
 * Usage: formatted = intl_FormatNumber(numberFormat, x, formatToParts)
 */
[[nodiscard]] extern bool intl_FormatNumber(JSContext* cx, unsigned argc,
                                            Value* vp);

#if DEBUG || MOZ_SYSTEM_ICU
/**
 * Returns an object with all available measurement units.
 *
 * Usage: units = intl_availableMeasurementUnits()
 */
[[nodiscard]] extern bool intl_availableMeasurementUnits(JSContext* cx,
                                                         unsigned argc,
                                                         Value* vp);
#endif

}  // namespace js

#endif /* builtin_intl_NumberFormat_h */
