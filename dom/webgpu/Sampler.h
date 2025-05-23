/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_SAMPLER_H_
#define GPU_SAMPLER_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {

class Device;

class Sampler final : public ObjectBase, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(Sampler)
  GPU_DECL_JS_WRAP(Sampler)

 private:
  Sampler() = delete;
  virtual ~Sampler();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_SAMPLER_H_
