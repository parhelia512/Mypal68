/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

namespace {  // NOLINT(google-build-namespaces)

inline Sk4px::Wide Sk4px::widen() const {
    return Sk16h(vmovl_u8(vget_low_u8 (this->fVec)),
                 vmovl_u8(vget_high_u8(this->fVec)));
}

inline Sk4px::Wide Sk4px::mulWiden(const Sk16b& other) const {
    return Sk16h(vmull_u8(vget_low_u8 (this->fVec), vget_low_u8 (other.fVec)),
                 vmull_u8(vget_high_u8(this->fVec), vget_high_u8(other.fVec)));
}

inline Sk4px Sk4px::Wide::addNarrowHi(const Sk16h& other) const {
    const Sk4px::Wide o(other);  // Should be no code, but allows us to access fLo, fHi.
    return Sk16b(vcombine_u8(vaddhn_u16(this->fLo.fVec, o.fLo.fVec),
                             vaddhn_u16(this->fHi.fVec, o.fHi.fVec)));
}

inline Sk4px Sk4px::Wide::div255() const {
    // Calculated as (x + (x+128)>>8 +128) >> 8.  The 'r' in each instruction provides each +128.
    return Sk16b(vcombine_u8(vraddhn_u16(this->fLo.fVec, vrshrq_n_u16(this->fLo.fVec, 8)),
                             vraddhn_u16(this->fHi.fVec, vrshrq_n_u16(this->fHi.fVec, 8))));
}

inline Sk4px Sk4px::alphas() const {
    auto as = vshrq_n_u32((uint32x4_t)fVec, SK_A32_SHIFT);  // ___3 ___2 ___1 ___0
    return Sk16b((uint8x16_t)vmulq_n_u32(as, 0x01010101));  // 3333 2222 1111 0000
}

inline Sk4px Sk4px::Load4Alphas(const SkAlpha a[4]) {
    uint8x16_t a8 = vdupq_n_u8(0);                           // ____ ____ ____ ____
    a8 = vld1q_lane_u8(a+0, a8,  0);                         // ____ ____ ____ ___0
    a8 = vld1q_lane_u8(a+1, a8,  4);                         // ____ ____ ___1 ___0
    a8 = vld1q_lane_u8(a+2, a8,  8);                         // ____ ___2 ___1 ___0
    a8 = vld1q_lane_u8(a+3, a8, 12);                         // ___3 ___2 ___1 ___0
    auto a32 = (uint32x4_t)a8;                               //
    return Sk16b((uint8x16_t)vmulq_n_u32(a32, 0x01010101));  // 3333 2222 1111 0000
}

inline Sk4px Sk4px::Load2Alphas(const SkAlpha a[2]) {
    uint8x16_t a8 = vdupq_n_u8(0);                           // ____ ____ ____ ____
    a8 = vld1q_lane_u8(a+0, a8,  0);                         // ____ ____ ____ ___0
    a8 = vld1q_lane_u8(a+1, a8,  4);                         // ____ ____ ___1 ___0
    auto a32 = (uint32x4_t)a8;                               //
    return Sk16b((uint8x16_t)vmulq_n_u32(a32, 0x01010101));  // ____ ____ 1111 0000
}

} // namespace
