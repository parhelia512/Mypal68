/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebGPUBinding.h"
#include "CommandEncoder.h"

#include "Device.h"

namespace mozilla {
namespace webgpu {

CommandEncoder::~CommandEncoder() = default;

GPU_IMPL_CYCLE_COLLECTION(CommandEncoder, mParent)
GPU_IMPL_JS_WRAP(CommandEncoder)

}  // namespace webgpu
}  // namespace mozilla
