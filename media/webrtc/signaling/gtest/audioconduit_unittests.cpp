/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "AudioConduit.h"

#include "MockCall.h"

using namespace mozilla;

namespace test {

class MockChannelProxy : public webrtc::voe::ChannelProxy {
 public:
  void SetSendAudioLevelIndicationStatus(bool enable, int id) override {
    mSetSendAudioLevelIndicationStatusEnabled = enable;
  }

  void SetReceiveAudioLevelIndicationStatus(bool enable, int id,
                                            bool isLevelSsrc = true) override {
    mSetReceiveAudioLevelIndicationStatusEnabled = enable;
  }

  void SetReceiveCsrcAudioLevelIndicationStatus(bool enable, int id) override {
    mSetReceiveCsrcAudioLevelIndicationStatusEnabled = enable;
  }

  void SetSendMIDStatus(bool enable, int id) override {
    mSetSendMIDStatusEnabled = enable;
  }

  void RegisterTransport(Transport* transport) override {}

  void SetRtcpEventObserver(webrtc::RtcpEventObserver* observer) override {}

  bool mSetSendAudioLevelIndicationStatusEnabled;
  bool mSetReceiveAudioLevelIndicationStatusEnabled;
  bool mSetReceiveCsrcAudioLevelIndicationStatusEnabled;
  bool mSetSendMIDStatusEnabled;
};

class AudioConduitWithMockChannelProxy : public WebrtcAudioConduit {
 public:
  AudioConduitWithMockChannelProxy(RefPtr<WebRtcCallWrapper> aCall,
                                   nsCOMPtr<nsISerialEventTarget> aStsThread)
      : WebrtcAudioConduit(aCall, aStsThread) {}

  MediaConduitErrorCode Init() override {
    mRecvChannelProxy.reset(new MockChannelProxy);
    mSendChannelProxy.reset(new MockChannelProxy);

    return kMediaConduitNoError;
  }

  void DeleteChannels() override {
    mRecvChannelProxy = nullptr;
    mSendChannelProxy = nullptr;
  }

  const MockChannelProxy* GetRecvChannelProxy() {
    return static_cast<MockChannelProxy*>(mRecvChannelProxy.get());
  }

  const MockChannelProxy* GetSendChannelProxy() {
    return static_cast<MockChannelProxy*>(mSendChannelProxy.get());
  }
};

class AudioConduitTest : public ::testing::Test {
 public:
  AudioConduitTest() : mCall(new MockCall()) {
    mAudioConduit = new AudioConduitWithMockChannelProxy(
        WebRtcCallWrapper::Create(UniquePtr<MockCall>(mCall)),
        GetCurrentSerialEventTarget());
    mAudioConduit->Init();
  }

  MockCall* mCall;
  RefPtr<AudioConduitWithMockChannelProxy> mAudioConduit;
};

TEST_F(AudioConduitTest, TestConfigureSendMediaCodec) {
  MediaConduitErrorCode ec;

  // defaults
  AudioCodecConfig codecConfig(114, "opus", 48000, 2, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
  }

  // null codec
  ec = mAudioConduit->ConfigureSendMediaCodec(nullptr);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // empty codec name
  codecConfig = AudioCodecConfig(114, "", 48000, 2, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // long codec name
  size_t longNameLength = WebrtcAudioConduit::CODEC_PLNAME_SIZE + 2;
  char* longName = new char[longNameLength];
  memset(longName, 'A', longNameLength - 2);
  longName[longNameLength - 1] = 0;
  codecConfig = AudioCodecConfig(114, longName, 48000, 2, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
  delete[] longName;
}

TEST_F(AudioConduitTest, TestConfigureSendOpusMono) {
  MediaConduitErrorCode ec;

  // opus mono
  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 1, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 1UL);
    ASSERT_EQ(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusFEC) {
  MediaConduitErrorCode ec;

  // opus with inband Forward Error Correction
  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, true);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_NE(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("useinbandfec"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendMaxPlaybackRate) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mMaxPlaybackRate = 1234;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_NE(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxplaybackrate"), "1234");
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveMediaCodecs) {
  MediaConduitErrorCode ec;

  // just default opus stereo
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(mCall->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
  }

  // multiple codecs
  codecs.clear();
  codecs.emplace_back(new AudioCodecConfig(9, "g722", 16000, 2, false));
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(mCall->mAudioReceiveConfig.decoder_map.size(), 2U);
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(9);
    ASSERT_EQ(f.name, "g722");
    ASSERT_EQ(f.clockrate_hz, 16000);
    ASSERT_EQ(f.num_channels, 2U);
    ASSERT_EQ(f.parameters.size(), 0U);
  }
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2U);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
  }

  // no codecs
  codecs.clear();
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // null codec
  codecs.clear();
  codecs.push_back(nullptr);
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // invalid codec name
  codecs.clear();
  codecs.emplace_back(new AudioCodecConfig(114, "", 48000, 2, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // invalid number of channels
  codecs.clear();
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 42, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // long codec name
  codecs.clear();
  size_t longNameLength = WebrtcAudioConduit::CODEC_PLNAME_SIZE + 2;
  char* longName = new char[longNameLength];
  memset(longName, 'A', longNameLength - 2);
  longName[longNameLength - 1] = 0;
  codecs.emplace_back(new AudioCodecConfig(100, longName, 48000, 42, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
  delete[] longName;
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusMono) {
  MediaConduitErrorCode ec;

  // opus mono
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 1, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(mCall->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 1UL);
    ASSERT_EQ(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusFEC) {
  MediaConduitErrorCode ec;

  // opus with inband Forward Error Correction
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, true));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(mCall->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_NE(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("useinbandfec"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveMaxPlaybackRate) {
  MediaConduitErrorCode ec;

  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));

  codecs[0]->mMaxPlaybackRate = 0;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.count("maxplaybackrate"), 0U);
  }

  codecs[0]->mMaxPlaybackRate = 8000;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        mCall->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.at("maxplaybackrate"), "8000");
  }
}

TEST_F(AudioConduitTest, TestSetLocalRTPExtensions) {
  MediaConduitErrorCode ec;

  using LocalDirection = MediaSessionConduitLocalDirection;

  RtpExtList extensions;

  // Empty extensions
  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);

  // Audio level
  webrtc::RtpExtension extension;
  extension.uri = webrtc::RtpExtension::kAudioLevelUri;
  extensions.emplace_back(extension);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kAudioLevelUri);
  ASSERT_TRUE(mAudioConduit->GetRecvChannelProxy()
                  ->mSetReceiveAudioLevelIndicationStatusEnabled);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioSendConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kAudioLevelUri);
  ASSERT_TRUE(mAudioConduit->GetSendChannelProxy()
                  ->mSetSendAudioLevelIndicationStatusEnabled);

  // Contributing sources audio level
  extensions.clear();
  extension.uri = webrtc::RtpExtension::kCsrcAudioLevelUri;
  extensions.emplace_back(extension);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);

  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kCsrcAudioLevelUri);
  ASSERT_TRUE(mAudioConduit->GetRecvChannelProxy()
                  ->mSetReceiveCsrcAudioLevelIndicationStatusEnabled);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // MId
  extensions.clear();
  extension.uri = webrtc::RtpExtension::kMIdUri;
  extensions.emplace_back(extension);

  // We do not support configuring receiving MId, but do not return an error
  // in this case.
  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioSendConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kMIdUri);
  ASSERT_TRUE(mAudioConduit->GetSendChannelProxy()->mSetSendMIDStatusEnabled);
}

TEST_F(AudioConduitTest, TestSetSyncGroup) {
  MediaConduitErrorCode ec;

  mAudioConduit->SetSyncGroup("test");
  ec = mAudioConduit->StartReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(mCall->mAudioReceiveConfig.sync_group, "test");
}

}  // End namespace test.
