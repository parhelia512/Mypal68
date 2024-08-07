/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileDescriptorSetParent.h"

namespace mozilla {
namespace ipc {

FileDescriptorSetParent::FileDescriptorSetParent(
    const FileDescriptor& aFileDescriptor) {
  mFileDescriptors.AppendElement(aFileDescriptor);
}

FileDescriptorSetParent::~FileDescriptorSetParent() = default;

void FileDescriptorSetParent::ForgetFileDescriptors(
    nsTArray<FileDescriptor>& aFileDescriptors) {
  aFileDescriptors = std::move(mFileDescriptors);
}

void FileDescriptorSetParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Implement me! Bug 1005157
}

mozilla::ipc::IPCResult FileDescriptorSetParent::RecvAddFileDescriptor(
    const FileDescriptor& aFileDescriptor) {
  mFileDescriptors.AppendElement(aFileDescriptor);
  return IPC_OK();
}

}  // namespace ipc
}  // namespace mozilla
