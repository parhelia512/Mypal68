/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// By defualt, synced fields may only be set by the currently active process,
// however a field can be marked as `MOZ_BC_FIELD_RACY` to relax this
// restriction, and allow it to be set from any process.
//
// Process restrictions on racy fields may be added in `WillSet{name}`
// validators.
#ifndef MOZ_BC_FIELD_RACY
#  define MOZ_BC_FIELD_RACY MOZ_BC_FIELD
#endif

MOZ_BC_FIELD_RACY(Name, nsString)
MOZ_BC_FIELD_RACY(Closed, bool)
MOZ_BC_FIELD(CrossOriginPolicy, nsILoadInfo::CrossOriginPolicy)
MOZ_BC_FIELD(OpenerPolicy, nsILoadInfo::CrossOriginOpenerPolicy)

// The current opener for this BrowsingContext. This is a weak reference, and
// stored as the opener ID.
MOZ_BC_FIELD(OpenerId, uint64_t)

// Toplevel browsing contexts only. This field controls whether the browsing
// context is currently considered to be activated by a gesture.
MOZ_BC_FIELD_RACY(IsActivatedByUserGesture, bool)

// ScreenOrientation-related APIs
MOZ_BC_FIELD(CurrentOrientationAngle, float)

MOZ_BC_FIELD(CurrentOrientationType, mozilla::dom::OrientationType)

MOZ_BC_FIELD(InRDMPane, bool)

#undef MOZ_BC_FIELD
#undef MOZ_BC_FIELD_RACY
