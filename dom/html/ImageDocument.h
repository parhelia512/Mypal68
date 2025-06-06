/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_ImageDocument_h
#define mozilla_dom_ImageDocument_h

#include "mozilla/Attributes.h"
#include "imgINotificationObserver.h"
#include "MediaDocument.h"
#include "nsIDOMEventListener.h"

namespace mozilla {
namespace dom {

class ImageDocument final : public MediaDocument,
                            public imgINotificationObserver,
                            public nsIDOMEventListener {
 public:
  ImageDocument();

  NS_DECL_ISUPPORTS_INHERITED

  enum MediaDocumentKind MediaDocumentKind() const override {
    return MediaDocumentKind::Image;
  }

  nsresult Init() override;

  nsresult StartDocumentLoad(const char* aCommand, nsIChannel* aChannel,
                             nsILoadGroup* aLoadGroup,
                             nsISupports* aContainer,
                             nsIStreamListener** aDocListener,
                             bool aReset = true,
                             nsIContentSink* aSink = nullptr) override;

  void SetScriptGlobalObject(nsIScriptGlobalObject*) override;
  void Destroy() override;
  void OnPageShow(bool aPersisted, EventTarget* aDispatchStartTarget,
                  bool aOnlySystemGroup = false) override;

  NS_DECL_IMGINOTIFICATIONOBSERVER

  // nsIDOMEventListener
  NS_DECL_NSIDOMEVENTLISTENER

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ImageDocument, MediaDocument)

  friend class ImageListener;

  void DefaultCheckOverflowing() { CheckOverflowing(mResizeImageByDefault); }

  // WebIDL API
  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  bool ImageIsOverflowing() const {
    return mImageIsOverflowingHorizontally || mImageIsOverflowingVertically;
  }
  bool ImageIsResized() const { return mImageIsResized; }
  // ShrinkToFit is called from xpidl methods and we don't have a good
  // way to mark those MOZ_CAN_RUN_SCRIPT yet.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void ShrinkToFit();
  void RestoreImage();
  void RestoreImageTo(int32_t aX, int32_t aY) { ScrollImageTo(aX, aY, true); }

  void NotifyPossibleTitleChange(bool aBoundTitleElement) override;

 protected:
  virtual ~ImageDocument();

  nsresult CreateSyntheticDocument() override;

  nsresult CheckOverflowing(bool changeState);

  void UpdateTitleAndCharset();

  void ScrollImageTo(int32_t aX, int32_t aY, bool restoreImage);

  float GetRatio() {
    return std::min(mVisibleWidth / mImageWidth, mVisibleHeight / mImageHeight);
  }

  void ResetZoomLevel();
  float GetZoomLevel();
#if defined(MOZ_WIDGET_ANDROID)
  float GetResolution();
#endif

  void UpdateSizeFromLayout();

  enum eModeClasses {
    eNone,
    eShrinkToFit,
    eOverflowingVertical,  // And maybe horizontal too.
    eOverflowingHorizontalOnly
  };
  void SetModeClass(eModeClasses mode);

  void OnSizeAvailable(imgIRequest* aRequest, imgIContainer* aImage);
  void OnLoadComplete(imgIRequest* aRequest, nsresult aStatus);
  void OnHasTransparency();

  nsCOMPtr<Element> mImageContent;

  float mVisibleWidth;
  float mVisibleHeight;
  int32_t mImageWidth;
  int32_t mImageHeight;

  bool mResizeImageByDefault;
  bool mClickResizingEnabled;
  bool mImageIsOverflowingHorizontally;
  bool mImageIsOverflowingVertically;
  // mImageIsResized is true if the image is currently resized
  bool mImageIsResized;
  // mShouldResize is true if the image should be resized when it doesn't fit
  // mImageIsResized cannot be true when this is false, but mImageIsResized
  // can be false when this is true
  bool mShouldResize;
  bool mFirstResize;
  // mObservingImageLoader is true while the observer is set.
  bool mObservingImageLoader;
  bool mTitleUpdateInProgress;
  bool mHasCustomTitle;

  float mOriginalZoomLevel;
#if defined(MOZ_WIDGET_ANDROID)
  float mOriginalResolution;
#endif
};

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_ImageDocument_h */
