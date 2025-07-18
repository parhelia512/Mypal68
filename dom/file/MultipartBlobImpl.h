/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MultipartBlobImpl_h
#define mozilla_dom_MultipartBlobImpl_h

#include <utility>

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BaseBlobImpl.h"

namespace mozilla {
namespace dom {

class MultipartBlobImpl final : public BaseBlobImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(MultipartBlobImpl, BaseBlobImpl)

  // Create as a file
  static already_AddRefed<MultipartBlobImpl> Create(
      nsTArray<RefPtr<BlobImpl>>&& aBlobImpls, const nsAString& aName,
      const nsAString& aContentType, ErrorResult& aRv);

  // Create as a blob
  static already_AddRefed<MultipartBlobImpl> Create(
      nsTArray<RefPtr<BlobImpl>>&& aBlobImpls, const nsAString& aContentType,
      ErrorResult& aRv);

  // Create as a file to be later initialized
  explicit MultipartBlobImpl(const nsAString& aName)
      : BaseBlobImpl(NS_LITERAL_STRING("MultipartBlobImpl"), aName,
                     EmptyString(), UINT64_MAX) {}

  // Create as a blob to be later initialized
  MultipartBlobImpl()
      : BaseBlobImpl(NS_LITERAL_STRING("MultipartBlobImpl"), EmptyString(),
                     UINT64_MAX) {}

  void InitializeBlob(ErrorResult& aRv);

  void InitializeBlob(const Sequence<Blob::BlobPart>& aData,
                      const nsAString& aContentType, bool aNativeEOL,
                      ErrorResult& aRv);

  virtual already_AddRefed<BlobImpl> CreateSlice(uint64_t aStart,
                                                 uint64_t aLength,
                                                 const nsAString& aContentType,
                                                 ErrorResult& aRv) override;

  virtual uint64_t GetSize(ErrorResult& aRv) override { return mLength; }

  virtual void CreateInputStream(nsIInputStream** aInputStream,
                                 ErrorResult& aRv) override;

  virtual const nsTArray<RefPtr<BlobImpl>>* GetSubBlobImpls() const override {
    return mBlobImpls.Length() ? &mBlobImpls : nullptr;
  }

  void SetName(const nsAString& aName) { mName = aName; }

  size_t GetAllocationSize() const override;
  size_t GetAllocationSize(
      FallibleTArray<BlobImpl*>& aVisitedBlobImpls) const override;

  void GetBlobImplType(nsAString& aBlobImplType) const override;

 protected:
  MultipartBlobImpl(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                    const nsAString& aName, const nsAString& aContentType)
      : BaseBlobImpl(NS_LITERAL_STRING("MultipartBlobImpl"), aName,
                     aContentType, UINT64_MAX),
        mBlobImpls(std::move(aBlobImpls)) {}

  MultipartBlobImpl(nsTArray<RefPtr<BlobImpl>>&& aBlobImpls,
                    const nsAString& aContentType)
      : BaseBlobImpl(NS_LITERAL_STRING("MultipartBlobImpl"), aContentType,
                     UINT64_MAX),
        mBlobImpls(std::move(aBlobImpls)) {}

  virtual ~MultipartBlobImpl() = default;

  void SetLengthAndModifiedDate(ErrorResult& aRv);

  nsTArray<RefPtr<BlobImpl>> mBlobImpls;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MultipartBlobImpl_h
