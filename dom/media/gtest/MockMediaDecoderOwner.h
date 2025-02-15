/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOCK_MEDIA_DECODER_OWNER_H_
#define MOCK_MEDIA_DECODER_OWNER_H_

#include "MediaDecoderOwner.h"
#include "mozilla/AbstractThread.h"

namespace mozilla {

class MockMediaDecoderOwner : public MediaDecoderOwner {
 public:
  void DispatchAsyncEvent(const nsAString& aName) override {}
  void FireTimeUpdate(bool aPeriodic) override {}
  bool GetPaused() override { return false; }
  void MetadataLoaded(const MediaInfo* aInfo,
                      UniquePtr<const MetadataTags> aTags) override {}
  void NetworkError() override {}
  void DecodeError(const MediaResult& aError) override {}
  bool HasError() const override { return false; }
  void LoadAborted() override {}
  void PlaybackEnded() override {}
  void SeekStarted() override {}
  void SeekCompleted() override {}
  void DownloadProgressed() override {}
  void UpdateReadyState() override {}
  void FirstFrameLoaded() override {}
  void DownloadSuspended() override {}
  void DownloadResumed(bool aForceNetworkLoading) override {}
  void NotifySuspendedByCache(bool aIsSuspended) override {}
  void NotifyDecoderPrincipalChanged() override {}
  void SetAudibleState(bool aAudible) override {}
  void NotifyXPCOMShutdown() override {}
  AbstractThread* AbstractMainThread() const override {
    // Non-DocGroup version for Mock.
    return AbstractThread::MainThread();
  }
  void ConstructMediaTracks(const MediaInfo* aInfo) {}
  void RemoveMediaTracks() {}
  void AsyncResolveSeekDOMPromiseIfExists() override {}
  void AsyncRejectSeekDOMPromiseIfExists() override {}
};
}  // namespace mozilla

#endif
