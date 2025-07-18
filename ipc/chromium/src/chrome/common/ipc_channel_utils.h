/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CHROME_COMMON_IPC_CHANNEL_UTILS_H_
#define CHROME_COMMON_IPC_CHANNEL_UTILS_H_

#include "chrome/common/ipc_message.h"
#include "MessageChannel.h"

namespace IPC {

void AddIPCProfilerMarker(const Message& aMessage, int32_t aOtherPid,
                          mozilla::ipc::MessageDirection aDirection,
                          mozilla::ipc::MessagePhase aPhase);

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_UTILS_H_
