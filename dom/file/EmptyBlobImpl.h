/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_EmptyBlobImpl_h
#define mozilla_dom_EmptyBlobImpl_h

#include "BaseBlobImpl.h"

namespace mozilla {
namespace dom {

class EmptyBlobImpl final : public BaseBlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(EmptyBlobImpl, BaseBlobImpl)

  explicit EmptyBlobImpl(const nsAString& aContentType)
      : BaseBlobImpl(NS_LITERAL_STRING("EmptyBlobImpl"), aContentType,
                     0 /* aLength */) {}

  EmptyBlobImpl(const nsAString& aName, const nsAString& aContentType,
                int64_t aLastModifiedDate)
      : BaseBlobImpl(NS_LITERAL_STRING("EmptyBlobImpl"), aName, aContentType, 0,
                     aLastModifiedDate) {}

  virtual void CreateInputStream(nsIInputStream** aStream,
                                 ErrorResult& aRv) override;

  virtual already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart,
                                                 uint64_t aLength,
                                                 const nsAString& aContentType,
                                                 ErrorResult& aRv) override;

  virtual bool IsMemoryFile() const override { return true; }

 private:
  ~EmptyBlobImpl() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_EmptyBlobImpl_h
