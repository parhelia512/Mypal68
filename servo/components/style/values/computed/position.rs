/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! CSS handling for the computed value of
//! [`position`][position] values.
//!
//! [position]: https://drafts.csswg.org/css-backgrounds-3/#position

use crate::values::computed::{Integer, LengthPercentage, NonNegativeNumber, Percentage};
use crate::values::generics::position::AspectRatio as GenericAspectRatio;
use crate::values::generics::position::Position as GenericPosition;
use crate::values::generics::position::PositionOrAuto as GenericPositionOrAuto;
use crate::values::generics::position::Ratio as GenericRatio;
use crate::values::generics::position::ZIndex as GenericZIndex;
pub use crate::values::specified::position::{GridAutoFlow, GridTemplateAreas, MasonryAutoFlow};
use crate::{One, Zero};
use std::cmp::{Ordering, PartialOrd};
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

/// The computed value of a CSS `<position>`
pub type Position = GenericPosition<HorizontalPosition, VerticalPosition>;

/// The computed value of an `auto | <position>`
pub type PositionOrAuto = GenericPositionOrAuto<Position>;

/// The computed value of a CSS horizontal position.
pub type HorizontalPosition = LengthPercentage;

/// The computed value of a CSS vertical position.
pub type VerticalPosition = LengthPercentage;

impl Position {
    /// `50% 50%`
    #[inline]
    pub fn center() -> Self {
        Self::new(
            LengthPercentage::new_percent(Percentage(0.5)),
            LengthPercentage::new_percent(Percentage(0.5)),
        )
    }

    /// `0% 0%`
    #[inline]
    pub fn zero() -> Self {
        Self::new(LengthPercentage::zero(), LengthPercentage::zero())
    }
}

impl ToCss for Position {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        self.horizontal.to_css(dest)?;
        dest.write_str(" ")?;
        self.vertical.to_css(dest)
    }
}

/// A computed value for the `z-index` property.
pub type ZIndex = GenericZIndex<Integer>;

/// A computed <ratio> value.
pub type Ratio = GenericRatio<NonNegativeNumber>;

impl PartialOrd for Ratio {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        f64::partial_cmp(
            &((self.0).0 as f64 * (other.1).0 as f64),
            &((self.1).0 as f64 * (other.0).0 as f64),
        )
    }
}

impl Ratio {
    /// Returns a new Ratio.
    pub fn new(a: f32, b: f32) -> Self {
        GenericRatio(a.into(), b.into())
    }

    /// Returns the used value. A ratio of 0/0 behaves as the ratio 1/0.
    /// https://drafts.csswg.org/css-values-4/#ratios
    pub fn used_value(self) -> Self {
        if self.0.is_zero() && self.1.is_zero() {
            Ratio::new(One::one(), Zero::zero())
        } else {
            self
        }
    }
}

/// A computed value for the `aspect-ratio` property.
pub type AspectRatio = GenericAspectRatio<NonNegativeNumber>;
