/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientHandleOpParent.h"

#include "ClientHandleParent.h"
#include "ClientSourceParent.h"
#include "mozilla/dom/PClientManagerParent.h"

namespace mozilla {
namespace dom {

ClientSourceParent* ClientHandleOpParent::GetSource() const {
  auto handle = static_cast<ClientHandleParent*>(Manager());
  return handle->GetSource();
}

void ClientHandleOpParent::ActorDestroy(ActorDestroyReason aReason) {
  mPromiseRequestHolder.DisconnectIfExists();
}

void ClientHandleOpParent::Init(ClientOpConstructorArgs&& aArgs) {
  ClientSourceParent* source = GetSource();
  CopyableErrorResult rv;
  if (!source) {
    rv.Throw(NS_ERROR_DOM_ABORT_ERR);
    Unused << PClientHandleOpParent::Send__delete__(this, rv);
    return;
  }

  RefPtr<ClientOpPromise> p;

  // ClientPostMessageArgs can contain PBlob actors.  This means we
  // can't just forward the args from one PBackground manager to
  // another.  Instead, unpack the structured clone data and repack
  // it into a new set of arguments.
  if (aArgs.type() == ClientOpConstructorArgs::TClientPostMessageArgs) {
    const ClientPostMessageArgs& orig = aArgs.get_ClientPostMessageArgs();

    ClientPostMessageArgs rebuild;
    rebuild.serviceWorker() = orig.serviceWorker();

    StructuredCloneData data;
    data.BorrowFromClonedMessageDataForBackgroundParent(orig.clonedData());
    if (!data.BuildClonedMessageDataForBackgroundParent(
            source->Manager()->Manager(), rebuild.clonedData())) {
      rv.Throw(NS_ERROR_DOM_ABORT_ERR);
      Unused << PClientHandleOpParent::Send__delete__(this, rv);
      return;
    }

    p = source->StartOp(std::move(rebuild));
  }

  // Other argument types can just be forwarded straight through.
  else {
    p = source->StartOp(std::move(aArgs));
  }

  // Capturing 'this' is safe here because we disconnect the promise in
  // ActorDestroy() which ensures neither lambda is called if the actor
  // is destroyed before the source operation completes.
  p->Then(
       GetCurrentSerialEventTarget(), __func__,
       [this](const ClientOpResult& aResult) {
         mPromiseRequestHolder.Complete();
         Unused << PClientHandleOpParent::Send__delete__(this, aResult);
       },
       [this](const CopyableErrorResult& aRv) {
         mPromiseRequestHolder.Complete();
         Unused << PClientHandleOpParent::Send__delete__(this, aRv);
       })
      ->Track(mPromiseRequestHolder);
}

}  // namespace dom
}  // namespace mozilla
