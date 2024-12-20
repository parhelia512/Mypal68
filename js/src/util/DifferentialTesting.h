/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for differential testing.
 */

#ifndef util_DifferentialTesting_h
#define util_DifferentialTesting_h

namespace js {

inline bool SupportDifferentialTesting() {
#ifdef DEBUG
  extern bool gSupportDifferentialTesting;
  return gSupportDifferentialTesting;
#else
  return false;
#endif
}

}  // namespace js

#endif /* util_DifferentialTesting_h */
