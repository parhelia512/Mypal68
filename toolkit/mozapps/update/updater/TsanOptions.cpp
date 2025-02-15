/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/TsanOptions.h"

#ifndef _MSC_VER  // Not supported by clang-cl yet

// See also mozglue/build/TsanOptions.cpp before modifying this
extern "C" const char* __tsan_default_suppressions() {
  return "# Add your suppressions below\n"

         // External uninstrumented libraries
         MOZ_TSAN_DEFAULT_EXTLIB_SUPPRESSIONS

         // Bug 1623034
         "race:QuitProgressUI\n"
         "race:ShowProgressUI\n"

      // End of suppressions.
      ;  // Please keep this semicolon.
}
#endif  // _MSC_VER
