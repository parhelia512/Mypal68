/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_base_MessageManagerCallback_h__
#define dom_base_MessageManagerCallback_h__

#include "nsStringFwd.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace dom {

class ProcessMessageManager;

namespace ipc {

class StructuredCloneData;

class MessageManagerCallback {
 public:
  virtual ~MessageManagerCallback() = default;

  virtual bool DoLoadMessageManagerScript(const nsAString& aURL,
                                          bool aRunInGlobalScope) {
    return true;
  }

  virtual bool DoSendBlockingMessage(const nsAString& aMessage,
                                     StructuredCloneData& aData,
                                     nsTArray<StructuredCloneData>* aRetVal) {
    return true;
  }

  virtual nsresult DoSendAsyncMessage(const nsAString& aMessage,
                                      StructuredCloneData& aData) {
    return NS_OK;
  }

  virtual mozilla::dom::ProcessMessageManager* GetProcessMessageManager()
      const {
    return nullptr;
  }

  virtual void DoGetRemoteType(nsAString& aRemoteType,
                               ErrorResult& aError) const;

 protected:
  bool BuildClonedMessageDataForParent(ContentParent* aParent,
                                       StructuredCloneData& aData,
                                       ClonedMessageData& aClonedData);
  bool BuildClonedMessageDataForChild(ContentChild* aChild,
                                      StructuredCloneData& aData,
                                      ClonedMessageData& aClonedData);
};

void UnpackClonedMessageDataForParent(const ClonedMessageData& aClonedData,
                                      StructuredCloneData& aData);

void UnpackClonedMessageDataForChild(const ClonedMessageData& aClonedData,
                                     StructuredCloneData& aData);

}  // namespace ipc
}  // namespace dom
}  // namespace mozilla

#endif
