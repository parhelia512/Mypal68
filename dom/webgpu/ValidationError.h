/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ValidationError_H_
#define GPU_ValidationError_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
namespace webgpu {
class Device;

class ValidationError final : public nsWrapperCache, public ChildOf<Device> {
 public:
  GPU_DECL_CYCLE_COLLECTION(ValidationError)
  GPU_DECL_JS_WRAP(ValidationError)
  ValidationError() = delete;

 private:
  virtual ~ValidationError();
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ValidationError_H_
