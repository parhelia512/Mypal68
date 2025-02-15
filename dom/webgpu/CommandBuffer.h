/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CommandBuffer_H_
#define GPU_CommandBuffer_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class CommandBuffer final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CommandBuffer)
  GPU_DECL_JS_WRAP(CommandBuffer)

 private:
  CommandBuffer() = delete;
  virtual ~CommandBuffer();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_CommandBuffer_H_
