/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_hooks_h
#define memory_hooks_h

#if defined(MOZ_REPLACE_MALLOC) && defined(MOZ_PROFILER_MEMORY)
namespace mozilla {
namespace profiler {

void install_memory_hooks();
void remove_memory_hooks();
void enable_native_allocations(int aMainThreadId);
void disable_native_allocations();

}  // namespace profiler
}  // namespace mozilla
#endif

#endif
