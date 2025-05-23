/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaRecorder.h"

#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "DOMMediaStream.h"
#include "GeckoProfiler.h"
#include "MediaDecoder.h"
#include "MediaEncoder.h"
#include "MediaTrackGraphImpl.h"
#include "VideoUtils.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/AudioStreamTrack.h"
#include "mozilla/dom/BlobEvent.h"
#include "mozilla/dom/EmptyBlobImpl.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/MediaRecorderErrorEvent.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "mozilla/dom/VideoStreamTrack.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TaskQueue.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentTypeParser.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "mozilla/dom/Document.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsMimeTypes.h"
#include "nsProxyRelease.h"
#include "nsTArray.h"

mozilla::LazyLogModule gMediaRecorderLog("MediaRecorder");
#define LOG(type, msg) MOZ_LOG(gMediaRecorderLog, type, msg)

#define MIN_VIDEO_BITRATE_BPS 10e3        // 10kbps
#define DEFAULT_VIDEO_BITRATE_BPS 2500e3  // 2.5Mbps
#define MAX_VIDEO_BITRATE_BPS 100e6       // 100Mbps

#define MIN_AUDIO_BITRATE_BPS 500        // 500bps
#define DEFAULT_AUDIO_BITRATE_BPS 128e3  // 128kbps
#define MAX_AUDIO_BITRATE_BPS 512e3      // 512kbps

namespace mozilla {

namespace dom {

using namespace mozilla::media;

/**
 * MediaRecorderReporter measures memory being used by the Media Recorder.
 *
 * It is a singleton reporter and the single class object lives as long as at
 * least one Recorder is registered. In MediaRecorder, the reporter is
 * unregistered when it is destroyed.
 */
class MediaRecorderReporter final : public nsIMemoryReporter {
 public:
  static void AddMediaRecorder(MediaRecorder* aRecorder) {
    if (!sUniqueInstance) {
      sUniqueInstance = MakeAndAddRef<MediaRecorderReporter>();
      RegisterWeakAsyncMemoryReporter(sUniqueInstance);
    }
    sUniqueInstance->mRecorders.AppendElement(aRecorder);
  }

  static void RemoveMediaRecorder(MediaRecorder* aRecorder) {
    if (!sUniqueInstance) {
      return;
    }

    sUniqueInstance->mRecorders.RemoveElement(aRecorder);
    if (sUniqueInstance->mRecorders.IsEmpty()) {
      UnregisterWeakMemoryReporter(sUniqueInstance);
      sUniqueInstance = nullptr;
    }
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  MediaRecorderReporter() = default;

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override {
    nsTArray<RefPtr<MediaRecorder::SizeOfPromise>> promises;
    for (const RefPtr<MediaRecorder>& recorder : mRecorders) {
      promises.AppendElement(recorder->SizeOfExcludingThis(MallocSizeOf));
    }

    nsCOMPtr<nsIHandleReportCallback> handleReport = aHandleReport;
    nsCOMPtr<nsISupports> data = aData;
    MediaRecorder::SizeOfPromise::All(GetCurrentSerialEventTarget(),
                                      promises)
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [handleReport, data](const nsTArray<size_t>& sizes) {
              nsCOMPtr<nsIMemoryReporterManager> manager =
                  do_GetService("@mozilla.org/memory-reporter-manager;1");
              if (!manager) {
                return;
              }

              size_t sum = 0;
              for (const size_t& size : sizes) {
                sum += size;
              }

              handleReport->Callback(
                  EmptyCString(), NS_LITERAL_CSTRING("explicit/media/recorder"),
                  KIND_HEAP, UNITS_BYTES, sum,
                  NS_LITERAL_CSTRING("Memory used by media recorder."), data);

              manager->EndReport();
            },
            [](size_t) { MOZ_CRASH("Unexpected reject"); });

    return NS_OK;
  }

 private:
  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf)

  virtual ~MediaRecorderReporter() {
    MOZ_ASSERT(mRecorders.IsEmpty(), "All recorders must have been removed");
  }

  static StaticRefPtr<MediaRecorderReporter> sUniqueInstance;

  nsTArray<RefPtr<MediaRecorder>> mRecorders;
};
NS_IMPL_ISUPPORTS(MediaRecorderReporter, nsIMemoryReporter);

NS_IMPL_CYCLE_COLLECTION_CLASS(MediaRecorder)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MediaRecorder,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStream)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAudioNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSecurityDomException)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mUnknownDomException)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MediaRecorder,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStream)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAudioNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSecurityDomException)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mUnknownDomException)
  tmp->UnRegisterActivityObserver();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MediaRecorder)
  NS_INTERFACE_MAP_ENTRY(nsIDocumentActivity)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(MediaRecorder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaRecorder, DOMEventTargetHelper)

namespace {
bool PrincipalSubsumes(MediaRecorder* aRecorder, nsIPrincipal* aPrincipal) {
  if (!aRecorder->GetOwner()) {
    return false;
  }
  nsCOMPtr<Document> doc = aRecorder->GetOwner()->GetExtantDoc();
  if (!doc) {
    return false;
  }
  if (!aPrincipal) {
    return false;
  }
  bool subsumes;
  if (NS_FAILED(doc->NodePrincipal()->Subsumes(aPrincipal, &subsumes))) {
    return false;
  }
  return subsumes;
}

bool MediaStreamTracksPrincipalSubsumes(
    MediaRecorder* aRecorder,
    const nsTArray<RefPtr<MediaStreamTrack>>& aTracks) {
  nsCOMPtr<nsIPrincipal> principal = nullptr;
  for (const auto& track : aTracks) {
    nsContentUtils::CombineResourcePrincipals(&principal,
                                              track->GetPrincipal());
  }
  return PrincipalSubsumes(aRecorder, principal);
}

bool AudioNodePrincipalSubsumes(MediaRecorder* aRecorder,
                                AudioNode* aAudioNode) {
  MOZ_ASSERT(aAudioNode);
  Document* doc =
      aAudioNode->GetOwner() ? aAudioNode->GetOwner()->GetExtantDoc() : nullptr;
  nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;
  return PrincipalSubsumes(aRecorder, principal);
}

enum class TypeSupport {
  Supported,
  MediaTypeInvalid,
  NoVideoWithAudioType,
  ContainersDisabled,
  CodecsDisabled,
  ContainerUnsupported,
  CodecUnsupported,
  CodecDuplicated,
};

nsCString TypeSupportToCString(TypeSupport aSupport,
                               const nsAString& aMimeType) {
  nsAutoCString mime = NS_ConvertUTF16toUTF8(aMimeType);
  switch (aSupport) {
    case TypeSupport::Supported:
      return nsPrintfCString("%s is supported", mime.get());
    case TypeSupport::MediaTypeInvalid:
      return nsPrintfCString("%s is not a valid media type", mime.get());
    case TypeSupport::NoVideoWithAudioType:
      return nsPrintfCString(
          "Video cannot be recorded with %s as it is an audio type",
          mime.get());
    case TypeSupport::ContainersDisabled:
      return NS_LITERAL_CSTRING("All containers are disabled");
    case TypeSupport::CodecsDisabled:
      return NS_LITERAL_CSTRING("All codecs are disabled");
    case TypeSupport::ContainerUnsupported:
      return nsPrintfCString("%s indicates an unsupported container",
                             mime.get());
    case TypeSupport::CodecUnsupported:
      return nsPrintfCString("%s indicates an unsupported codec", mime.get());
    case TypeSupport::CodecDuplicated:
      return nsPrintfCString("%s contains the same codec multiple times",
                             mime.get());
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown TypeSupport");
      return NS_LITERAL_CSTRING("Unknown error");
  }
}

TypeSupport CanRecordAudioTrackWith(const Maybe<MediaContainerType>& aMimeType,
                                    const nsAString& aMimeTypeString) {
  if (aMimeTypeString.IsEmpty()) {
    // For the empty string we just need to check whether we have support for an
    // audio container and an audio codec.
    if (!MediaEncoder::IsWebMEncoderEnabled() &&
        !MediaDecoder::IsOggEnabled()) {
      // No container support for audio.
      return TypeSupport::ContainersDisabled;
    }

    if (!MediaDecoder::IsOpusEnabled()) {
      // No codec support for audio.
      return TypeSupport::CodecsDisabled;
    }

    return TypeSupport::Supported;
  }

  if (!aMimeType) {
    // A mime type string was set, but it couldn't be parsed to a valid
    // MediaContainerType.
    return TypeSupport::MediaTypeInvalid;
  }

  if (aMimeType->Type() != MEDIAMIMETYPE(VIDEO_WEBM) &&
      aMimeType->Type() != MEDIAMIMETYPE(AUDIO_WEBM) &&
      aMimeType->Type() != MEDIAMIMETYPE(AUDIO_OGG)) {
    // Any currently supported container can record audio.
    return TypeSupport::ContainerUnsupported;
  }

  if (aMimeType->Type() == MEDIAMIMETYPE(VIDEO_WEBM) &&
      !MediaEncoder::IsWebMEncoderEnabled()) {
    return TypeSupport::ContainerUnsupported;
  }

  if (aMimeType->Type() == MEDIAMIMETYPE(AUDIO_WEBM) &&
      !MediaEncoder::IsWebMEncoderEnabled()) {
    return TypeSupport::ContainerUnsupported;
  }

  if (aMimeType->Type() == MEDIAMIMETYPE(AUDIO_OGG) &&
      !MediaDecoder::IsOggEnabled()) {
    return TypeSupport::ContainerUnsupported;
  }

  if (!MediaDecoder::IsOpusEnabled()) {
    return TypeSupport::CodecUnsupported;
  }

  if (!aMimeType->ExtendedType().HaveCodecs()) {
    // No codecs constrained, we can pick opus.
    return TypeSupport::Supported;
  }

  size_t opus = 0;
  size_t unknown = 0;
  for (const auto& codec : aMimeType->ExtendedType().Codecs().Range()) {
    // Ignore video codecs.
    if (codec.EqualsLiteral("vp8")) {
      continue;
    }
    if (codec.EqualsLiteral("vp8.0")) {
      continue;
    }
    if (codec.EqualsLiteral("opus")) {
      // All containers support opus
      opus++;
      continue;
    }
    unknown++;
  }

  if (unknown > 0) {
    // Unsupported codec.
    return TypeSupport::CodecUnsupported;
  }

  if (opus == 0) {
    // Codecs specified but not opus. Unsupported for audio.
    return TypeSupport::CodecUnsupported;
  }

  if (opus > 1) {
    // Opus specified more than once. Bad form.
    return TypeSupport::CodecDuplicated;
  }

  return TypeSupport::Supported;
}

TypeSupport CanRecordVideoTrackWith(const Maybe<MediaContainerType>& aMimeType,
                                    const nsAString& aMimeTypeString) {
  if (aMimeTypeString.IsEmpty()) {
    // For the empty string we just need to check whether we have support for a
    // video container and a video codec. The VP8 encoder is always available.
    if (!MediaEncoder::IsWebMEncoderEnabled()) {
      // No container support for video.
      return TypeSupport::ContainersDisabled;
    }

    return TypeSupport::Supported;
  }

  if (!aMimeType) {
    // A mime type string was set, but it couldn't be parsed to a valid
    // MediaContainerType.
    return TypeSupport::MediaTypeInvalid;
  }

  if (!aMimeType->Type().HasVideoMajorType()) {
    return TypeSupport::NoVideoWithAudioType;
  }

  if (aMimeType->Type() != MEDIAMIMETYPE(VIDEO_WEBM)) {
    return TypeSupport::ContainerUnsupported;
  }

  if (!MediaEncoder::IsWebMEncoderEnabled()) {
    return TypeSupport::ContainerUnsupported;
  }

  if (!aMimeType->ExtendedType().HaveCodecs()) {
    // No codecs constrained, we can pick vp8.
    return TypeSupport::Supported;
  }

  size_t vp8 = 0;
  size_t unknown = 0;
  for (const auto& codec : aMimeType->ExtendedType().Codecs().Range()) {
    if (codec.EqualsLiteral("opus")) {
      // Ignore audio codecs.
      continue;
    }
    if (codec.EqualsLiteral("vp8")) {
      vp8++;
      continue;
    }
    if (codec.EqualsLiteral("vp8.0")) {
      vp8++;
      continue;
    }
    unknown++;
  }

  if (unknown > 0) {
    // Unsupported codec.
    return TypeSupport::CodecUnsupported;
  }

  if (vp8 == 0) {
    // Codecs specified but not vp8. Unsupported for video.
    return TypeSupport::CodecUnsupported;
  }

  if (vp8 > 1) {
    // Vp8 specified more than once. Bad form.
    return TypeSupport::CodecDuplicated;
  }

  return TypeSupport::Supported;
}

TypeSupport CanRecordWith(MediaStreamTrack* aTrack,
                          const Maybe<MediaContainerType>& aMimeType,
                          const nsAString& aMimeTypeString) {
  if (aTrack->AsAudioStreamTrack()) {
    return CanRecordAudioTrackWith(aMimeType, aMimeTypeString);
  }

  if (aTrack->AsVideoStreamTrack()) {
    return CanRecordVideoTrackWith(aMimeType, aMimeTypeString);
  }

  MOZ_CRASH("Unexpected track type");
}

TypeSupport IsTypeSupportedImpl(const nsAString& aMIMEType) {
  if (aMIMEType.IsEmpty()) {
    // Lie and return true even if no container/codec support is enabled,
    // because the spec mandates it.
    return TypeSupport::Supported;
  }
  Maybe<MediaContainerType> mime = MakeMediaContainerType(aMIMEType);
  TypeSupport rv = CanRecordAudioTrackWith(mime, aMIMEType);
  if (rv == TypeSupport::Supported) {
    return rv;
  }
  return CanRecordVideoTrackWith(mime, aMIMEType);
}

nsString SelectMimeType(bool aHasVideo, bool aHasAudio,
                        const nsString& aConstrainedMimeType) {
  MOZ_ASSERT(aHasVideo || aHasAudio);

  Maybe<MediaContainerType> constrainedType =
      MakeMediaContainerType(aConstrainedMimeType);

  // If we are recording video, Start() should have rejected any non-video mime
  // types.
  MOZ_ASSERT_IF(constrainedType && aHasVideo,
                constrainedType->Type().HasVideoMajorType());
  // IsTypeSupported() rejects application mime types.
  MOZ_ASSERT_IF(constrainedType,
                !constrainedType->Type().HasApplicationMajorType());

  nsString result;
  if (constrainedType && constrainedType->ExtendedType().HaveCodecs()) {
    // The constrained mime type is fully defined (it has codecs!). No need to
    // select anything.
    CopyUTF8toUTF16(constrainedType->OriginalString(), result);
  } else {
    // There is no constrained mime type, or there is and it is not fully
    // defined but still valid. Select what's missing, so that we have major
    // type, container and codecs.

    // If there is a constrained mime type it should not have codecs defined,
    // because then it is fully defined and used unchanged (covered earlier).
    MOZ_ASSERT_IF(constrainedType,
                  !constrainedType->ExtendedType().HaveCodecs());

    nsCString majorType;
    {
      if (constrainedType) {
        // There is a constrained type. It has both major type and container in
        // order to be valid. Use them as is.
        majorType = constrainedType->Type().AsString();
      } else if (aHasVideo) {
        majorType = NS_LITERAL_CSTRING(VIDEO_WEBM);
      } else {
        majorType = NS_LITERAL_CSTRING(AUDIO_OGG);
      }
    }

    nsCString codecs;
    {
      if (aHasVideo && aHasAudio) {
        codecs = NS_LITERAL_CSTRING("\"vp8, opus\"");
      } else if (aHasVideo) {
        codecs = NS_LITERAL_CSTRING("vp8");
      } else {
        codecs = NS_LITERAL_CSTRING("opus");
      }
    }
    result = NS_ConvertUTF8toUTF16(
        nsPrintfCString("%s; codecs=%s", majorType.get(), codecs.get()));
  }

  MOZ_ASSERT_IF(aHasAudio,
                CanRecordAudioTrackWith(MakeMediaContainerType(result),
                                        result) == TypeSupport::Supported);
  MOZ_ASSERT_IF(aHasVideo,
                CanRecordVideoTrackWith(MakeMediaContainerType(result),
                                        result) == TypeSupport::Supported);
  return result;
}

void SelectBitrates(uint32_t aBitsPerSecond, uint8_t aNumVideoTracks,
                    uint32_t* aOutVideoBps, uint8_t aNumAudioTracks,
                    uint32_t* aOutAudioBps) {
  uint32_t vbps = 0;
  uint32_t abps = 0;

  const uint32_t minVideoBps = MIN_VIDEO_BITRATE_BPS * aNumVideoTracks;
  const uint32_t maxVideoBps = MAX_VIDEO_BITRATE_BPS * aNumVideoTracks;

  const uint32_t minAudioBps = MIN_AUDIO_BITRATE_BPS * aNumAudioTracks;
  const uint32_t maxAudioBps = MAX_AUDIO_BITRATE_BPS * aNumAudioTracks;

  if (aNumVideoTracks == 0) {
    MOZ_DIAGNOSTIC_ASSERT(aNumAudioTracks > 0);
    abps = std::min(maxAudioBps, std::max(minAudioBps, aBitsPerSecond));
  } else if (aNumAudioTracks == 0) {
    vbps = std::min(maxVideoBps, std::max(minVideoBps, aBitsPerSecond));
  } else {
    // Scale the bits so that video gets 20 times the bits of audio.
    // Since we must account for varying number of tracks of each type we weight
    // them by type; video = weight 20, audio = weight 1.
    const uint32_t videoWeight = aNumVideoTracks * 20;
    const uint32_t audioWeight = aNumAudioTracks;
    const uint32_t totalWeights = audioWeight + videoWeight;
    const uint32_t videoBitrate =
        uint64_t(aBitsPerSecond) * videoWeight / totalWeights;
    const uint32_t audioBitrate =
        uint64_t(aBitsPerSecond) * audioWeight / totalWeights;
    vbps = std::min(maxVideoBps, std::max(minVideoBps, videoBitrate));
    abps = std::min(maxAudioBps, std::max(minAudioBps, audioBitrate));
  }

  *aOutVideoBps = vbps;
  *aOutAudioBps = abps;
}
}  // namespace

/**
 * Session is an object to represent a single recording event.
 * In original design, all recording context is stored in MediaRecorder, which
 * causes a problem if someone calls MediaRecorder::Stop and
 * MediaRecorder::Start quickly. To prevent blocking main thread, media encoding
 * is executed in a second thread, named as Read Thread. For the same reason, we
 * do not wait Read Thread shutdown in MediaRecorder::Stop. If someone call
 * MediaRecorder::Start before Read Thread shutdown, the same recording context
 * in MediaRecorder might be access by two Reading Threads, which cause a
 * problem. In the new design, we put recording context into Session object,
 * including Read Thread.  Each Session has its own recording context and Read
 * Thread, problem is been resolved.
 *
 * Life cycle of a Session object.
 * 1) Initialization Stage (in main thread)
 *    Setup media tracks in MTG, and bind MediaEncoder with Source Stream when
 * mStream is available. Resource allocation, such as encoded data cache buffer
 * and MediaEncoder. Create read thread. Automatically switch to Extract stage
 * in the end of this stage. 2) Extract Stage (in Read Thread) Pull encoded A/V
 * frames from MediaEncoder, dispatch to OnDataAvailable handler. Unless a
 * client calls Session::Stop, Session object keeps stay in this stage. 3)
 * Destroy Stage (in main thread) Switch from Extract stage to Destroy stage by
 * calling Session::Stop. Release session resource and remove associated tracks
 * from MTG.
 *
 * Lifetime of MediaRecorder and Session objects.
 * 1) MediaRecorder creates a Session in MediaRecorder::Start function and holds
 *    a reference to Session. Then the Session registers itself to a
 *    ShutdownBlocker and also holds a reference to MediaRecorder.
 *    Therefore, the reference dependency in gecko is:
 *    ShutdownBlocker -> Session <-> MediaRecorder, note that there is a cycle
 *    reference between Session and MediaRecorder.
 * 2) A Session is destroyed after MediaRecorder::Stop has been called _and_ all
 * encoded media data has been passed to OnDataAvailable handler. 3)
 * MediaRecorder::Stop is called by user or the document is going to inactive or
 * invisible.
 */
class MediaRecorder::Session : public PrincipalChangeObserver<MediaStreamTrack>,
                               public DOMMediaStream::TrackListener {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Session)

  class StoreEncodedBufferRunnable final : public Runnable {
    RefPtr<Session> mSession;
    nsTArray<nsTArray<uint8_t>> mBuffer;

   public:
    StoreEncodedBufferRunnable(Session* aSession,
                               nsTArray<nsTArray<uint8_t>>&& aBuffer)
        : Runnable("StoreEncodedBufferRunnable"),
          mSession(aSession),
          mBuffer(std::move(aBuffer)) {}

    NS_IMETHOD
    Run() override {
      MOZ_ASSERT(NS_IsMainThread());
      mSession->MaybeCreateMutableBlobStorage();
      for (const auto& part : mBuffer) {
        if (part.IsEmpty()) {
          continue;
        }

        nsresult rv = mSession->mMutableBlobStorage->Append(part.Elements(),
                                                            part.Length());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          mSession->DoSessionEndTask(rv);
          break;
        }
      }

      return NS_OK;
    }
  };

  class EncoderListener : public MediaEncoderListener {
   public:
    EncoderListener(TaskQueue* aEncoderThread, Session* aSession)
        : mEncoderThread(aEncoderThread), mSession(aSession) {}

    void Forget() {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      mSession = nullptr;
    }

    void Initialized() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderInitialized();
      }
    }

    void DataAvailable() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderDataAvailable();
      }
    }

    void Error() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderError();
      }
    }

    void Shutdown() override {
      MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
      if (mSession) {
        mSession->MediaEncoderShutdown();
      }
    }

   protected:
    RefPtr<TaskQueue> mEncoderThread;
    RefPtr<Session> mSession;
  };

  struct TrackTypeComparator {
    enum Type {
      AUDIO,
      VIDEO,
    };
    static bool Equals(const RefPtr<MediaStreamTrack>& aTrack, Type aType) {
      return (aType == AUDIO && aTrack->AsAudioStreamTrack()) ||
             (aType == VIDEO && aTrack->AsVideoStreamTrack());
    }
  };

 public:
  Session(MediaRecorder* aRecorder,
          nsTArray<RefPtr<MediaStreamTrack>> aMediaStreamTracks,
          TimeDuration aTimeslice, uint32_t aVideoBitsPerSecond,
          uint32_t aAudioBitsPerSecond)
      : mRecorder(aRecorder),
        mMediaStreamTracks(std::move(aMediaStreamTracks)),
        mMainThread(mRecorder->GetOwner()->EventTargetFor(TaskCategory::Other)),
        mMimeType(SelectMimeType(
            mMediaStreamTracks.Contains(TrackTypeComparator::VIDEO,
                                        TrackTypeComparator()),
            mRecorder->mAudioNode ||
                mMediaStreamTracks.Contains(TrackTypeComparator::AUDIO,
                                            TrackTypeComparator()),
            mRecorder->mConstrainedMimeType)),
        mTimeslice(aTimeslice),
        mVideoBitsPerSecond(aVideoBitsPerSecond),
        mAudioBitsPerSecond(aAudioBitsPerSecond),
        mRunningState(RunningState::Idling) {
    MOZ_ASSERT(NS_IsMainThread());

    mMaxMemory = Preferences::GetUint("media.recorder.max_memory",
                                      MAX_ALLOW_MEMORY_BUFFER);
  }

  void PrincipalChanged(MediaStreamTrack* aTrack) override {
    NS_ASSERTION(mMediaStreamTracks.Contains(aTrack),
                 "Principal changed for unrecorded track");
    if (!MediaStreamTracksPrincipalSubsumes(mRecorder, mMediaStreamTracks)) {
      DoSessionEndTask(NS_ERROR_DOM_SECURITY_ERR);
    }
  }

  void NotifyTrackAdded(const RefPtr<MediaStreamTrack>& aTrack) override {
    LOG(LogLevel::Warning,
        ("Session.NotifyTrackAdded %p Raising error due to track set change",
         this));
    DoSessionEndTask(NS_ERROR_ABORT);
  }

  void NotifyTrackRemoved(const RefPtr<MediaStreamTrack>& aTrack) override {
    if (aTrack->Ended()) {
      // TrackEncoder will pickup tracks that end itself.
      return;
    }
    LOG(LogLevel::Warning,
        ("Session.NotifyTrackRemoved %p Raising error due to track set change",
         this));
    DoSessionEndTask(NS_ERROR_ABORT);
  }

  void Start() {
    LOG(LogLevel::Debug, ("Session.Start %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (mRecorder->mStream) {
      // The TrackListener reports back when tracks are added or removed from
      // the MediaStream.
      mMediaStream = mRecorder->mStream;
      mMediaStream->RegisterTrackListener(this);

      uint8_t trackTypes = 0;
      int32_t audioTracks = 0;
      int32_t videoTracks = 0;
      for (const auto& track : mMediaStreamTracks) {
        if (track->AsAudioStreamTrack()) {
          ++audioTracks;
          trackTypes |= ContainerWriter::CREATE_AUDIO_TRACK;
        } else if (track->AsVideoStreamTrack()) {
          ++videoTracks;
          trackTypes |= ContainerWriter::CREATE_VIDEO_TRACK;
        } else {
          MOZ_CRASH("Unexpected track type");
        }
      }

      if (audioTracks > 1 || videoTracks > 1) {
        // When MediaRecorder supports multiple tracks, we should set up a
        // single MediaInputPort from the input stream, and let main thread
        // check track principals async later.
        nsPIDOMWindowInner* window = mRecorder->GetOwner();
        Document* document = window ? window->GetExtantDoc() : nullptr;
        nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                        NS_LITERAL_CSTRING("Media"), document,
                                        nsContentUtils::eDOM_PROPERTIES,
                                        "MediaRecorderMultiTracksNotSupported");
        DoSessionEndTask(NS_ERROR_ABORT);
        return;
      }

      for (const auto& t : mMediaStreamTracks) {
        t->AddPrincipalChangeObserver(this);
      }

      LOG(LogLevel::Debug, ("Session.Start track types = (%d)", trackTypes));
      InitEncoder(trackTypes, mMediaStreamTracks[0]->Graph()->GraphRate());
      return;
    }

    if (mRecorder->mAudioNode) {
      TrackRate trackRate =
          mRecorder->mAudioNode->Context()->Graph()->GraphRate();

      // Web Audio node has only audio.
      InitEncoder(ContainerWriter::CREATE_AUDIO_TRACK, trackRate);
      return;
    }

    MOZ_ASSERT(false, "Unknown source");
  }

  void Stop() {
    LOG(LogLevel::Debug, ("Session.Stop %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (mEncoder) {
      mEncoder->Stop();
    }

    // Remove main thread state added in Start().
    if (mMediaStream) {
      mMediaStream->UnregisterTrackListener(this);
      mMediaStream = nullptr;
    }

    {
      for (const auto& track : mMediaStreamTracks) {
        track->RemovePrincipalChangeObserver(this);
      }
    }

    if (mRunningState.isOk() &&
        mRunningState.inspect() == RunningState::Idling) {
      LOG(LogLevel::Debug, ("Session.Stop Explicit end task %p", this));
      // End the Session directly if there is no encoder.
      DoSessionEndTask(NS_OK);
    } else if (mRunningState.isOk() &&
               (mRunningState.inspect() == RunningState::Starting ||
                mRunningState.inspect() == RunningState::Running)) {
      mRunningState = RunningState::Stopping;
    }
  }

  void Pause() {
    LOG(LogLevel::Debug, ("Session.Pause"));
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT_IF(mRunningState.isOk(),
                  mRunningState.unwrap() != RunningState::Idling);
    if (mRunningState.isErr() ||
        mRunningState.unwrap() == RunningState::Stopping ||
        mRunningState.unwrap() == RunningState::Stopped) {
      return;
    }
    MOZ_ASSERT(mEncoder);
    mEncoder->Suspend();
  }

  void Resume() {
    LOG(LogLevel::Debug, ("Session.Resume"));
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT_IF(mRunningState.isOk(),
                  mRunningState.unwrap() != RunningState::Idling);
    if (mRunningState.isErr() ||
        mRunningState.unwrap() == RunningState::Stopping ||
        mRunningState.unwrap() == RunningState::Stopped) {
      return;
    }
    MOZ_ASSERT(mEncoder);
    mEncoder->Resume();
  }

  void RequestData() {
    LOG(LogLevel::Debug, ("Session.RequestData"));
    MOZ_ASSERT(NS_IsMainThread());

    GatherBlob()->Then(
        mMainThread, __func__,
        [this, self = RefPtr<Session>(this)](
            const BlobPromise::ResolveOrRejectValue& aResult) {
          if (aResult.IsReject()) {
            LOG(LogLevel::Warning, ("GatherBlob failed for RequestData()"));
            DoSessionEndTask(aResult.RejectValue());
            return;
          }

          nsresult rv =
              mRecorder->CreateAndDispatchBlobEvent(aResult.ResolveValue());
          if (NS_FAILED(rv)) {
            DoSessionEndTask(NS_OK);
          }
        });
  }

  void MaybeCreateMutableBlobStorage() {
    if (!mMutableBlobStorage) {
      mMutableBlobStorage = new MutableBlobStorage(
          MutableBlobStorage::eCouldBeInTemporaryFile, nullptr, mMaxMemory);
    }
  }

  static const bool IsExclusive = false;
  using BlobPromise = MozPromise<RefPtr<BlobImpl>, nsresult, IsExclusive>;
  class BlobStorer : public MutableBlobStorageCallback {
    MozPromiseHolder<BlobPromise> mHolder;

    virtual ~BlobStorer() = default;

   public:
    BlobStorer() = default;

    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BlobStorer, override)

    void BlobStoreCompleted(MutableBlobStorage*, BlobImpl* aBlobImpl,
                            nsresult aRv) override {
      MOZ_ASSERT(NS_IsMainThread());
      if (NS_FAILED(aRv)) {
        mHolder.Reject(aRv, __func__);
        return;
      }

      mHolder.Resolve(aBlobImpl, __func__);
    }

    RefPtr<BlobPromise> Promise() { return mHolder.Ensure(__func__); }
  };

 protected:
  RefPtr<BlobPromise> GatherBlobImpl() {
    RefPtr<BlobStorer> storer = MakeAndAddRef<BlobStorer>();
    MaybeCreateMutableBlobStorage();
    mMutableBlobStorage->GetBlobImplWhenReady(NS_ConvertUTF16toUTF8(mMimeType),
                                              storer);
    mMutableBlobStorage = nullptr;

    storer->Promise()->Then(
        mMainThread, __func__,
        [self = RefPtr<Session>(this), p = storer->Promise()] {
          if (self->mBlobPromise == p) {
            // Reset BlobPromise.
            self->mBlobPromise = nullptr;
          }
        });

    return storer->Promise();
  }

 public:
  // Stops gathering data into the current blob and resolves when the current
  // blob is available. Future data will be stored in a new blob.
  // Should a previous async GatherBlob() operation still be in progress, we'll
  // wait for it to finish before starting this one.
  RefPtr<BlobPromise> GatherBlob() {
    MOZ_ASSERT(NS_IsMainThread());
    if (!mBlobPromise) {
      return mBlobPromise = GatherBlobImpl();
    }
    return mBlobPromise = mBlobPromise->Then(mMainThread, __func__,
                                             [self = RefPtr<Session>(this)] {
                                               return self->GatherBlobImpl();
                                             });
  }

  RefPtr<SizeOfPromise> SizeOfExcludingThis(
      mozilla::MallocSizeOf aMallocSizeOf) {
    MOZ_ASSERT(NS_IsMainThread());
    size_t encodedBufferSize =
        mMutableBlobStorage ? mMutableBlobStorage->SizeOfCurrentMemoryBuffer()
                            : 0;

    if (!mEncoder) {
      return SizeOfPromise::CreateAndResolve(encodedBufferSize, __func__);
    }

    auto& encoder = mEncoder;
    return InvokeAsync(
        mEncoderThread, __func__,
        [encoder, encodedBufferSize, aMallocSizeOf]() {
          return SizeOfPromise::CreateAndResolve(
              encodedBufferSize + encoder->SizeOfExcludingThis(aMallocSizeOf),
              __func__);
        });
  }

 private:
  virtual ~Session() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mShutdownPromise);
    MOZ_ASSERT(!mShutdownBlocker);
    LOG(LogLevel::Debug, ("Session.~Session (%p)", this));
  }

  // Pull encoded media data from MediaEncoder and put into MutableBlobStorage.
  // If the bool aForceFlush is true, we will force a dispatch of a blob to
  // main thread.
  void Extract(TimeStamp aNow, bool aForceFlush) {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    LOG(LogLevel::Debug, ("Session.Extract %p", this));

    AUTO_PROFILER_LABEL("MediaRecorder::Session::Extract", OTHER);

    // Pull encoded media data from MediaEncoder
    nsTArray<nsTArray<uint8_t>> encodedBuf;
    nsresult rv = mEncoder->GetEncodedData(&encodedBuf);
    if (NS_FAILED(rv)) {
      MOZ_RELEASE_ASSERT(encodedBuf.IsEmpty());
      // Even if we failed to encode more data, it might be time to push a blob
      // with already encoded data.
    }

    // Append pulled data into cache buffer.
    NS_DispatchToMainThread(
        new StoreEncodedBufferRunnable(this, std::move(encodedBuf)));

    // Whether push encoded data back to onDataAvailable automatically or we
    // need a flush.
    bool pushBlob = aForceFlush;
    if (!pushBlob && !mLastBlobTimeStamp.IsNull() &&
        (aNow - mLastBlobTimeStamp) > mTimeslice) {
      pushBlob = true;
    }
    if (pushBlob) {
      MOZ_ASSERT(!mLastBlobTimeStamp.IsNull(),
                 "The encoder must have been initialized if there's data");
      mLastBlobTimeStamp = aNow;
      InvokeAsync(mMainThread, this, __func__, &Session::GatherBlob)
          ->Then(mMainThread, __func__,
                 [this, self = RefPtr<Session>(this)](
                     const BlobPromise::ResolveOrRejectValue& aResult) {
                   // Assert that we've seen the start event
                   MOZ_ASSERT_IF(
                       mRunningState.isOk(),
                       mRunningState.inspect() != RunningState::Starting);
                   if (aResult.IsReject()) {
                     LOG(LogLevel::Warning,
                         ("GatherBlob failed for pushing blob"));
                     DoSessionEndTask(aResult.RejectValue());
                     return;
                   }

                   nsresult rv = mRecorder->CreateAndDispatchBlobEvent(
                       aResult.ResolveValue());
                   if (NS_FAILED(rv)) {
                     DoSessionEndTask(NS_OK);
                   }
                 });
    }
  }

  void InitEncoder(uint8_t aTrackTypes, TrackRate aTrackRate) {
    LOG(LogLevel::Debug, ("Session.InitEncoder %p", this));
    MOZ_ASSERT(NS_IsMainThread());

    if (!mRunningState.isOk() ||
        mRunningState.inspect() != RunningState::Idling) {
      MOZ_ASSERT_UNREACHABLE("Double-init");
      return;
    }

    // Create a TaskQueue to read encode media data from MediaEncoder.
    MOZ_RELEASE_ASSERT(!mEncoderThread);
    RefPtr<SharedThreadPool> pool =
        GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER);
    if (!pool) {
      LOG(LogLevel::Debug, ("Session.InitEncoder %p Failed to create "
                            "MediaRecorderReadThread thread pool",
                            this));
      DoSessionEndTask(NS_ERROR_FAILURE);
      return;
    }

    mEncoderThread =
        MakeAndAddRef<TaskQueue>(pool.forget(), "MediaRecorderReadThread");

    MOZ_DIAGNOSTIC_ASSERT(!mShutdownBlocker);
    // Add a shutdown blocker so mEncoderThread can be shutdown async.
    class Blocker : public ShutdownBlocker {
      const RefPtr<Session> mSession;

     public:
      Blocker(RefPtr<Session> aSession, const nsString& aName)
          : ShutdownBlocker(aName), mSession(std::move(aSession)) {}

      NS_IMETHOD BlockShutdown(nsIAsyncShutdownClient*) override {
        Unused << mSession->Shutdown();
        return NS_OK;
      }
    };

    nsString name;
    name.AppendPrintf("MediaRecorder::Session %p shutdown", this);
    mShutdownBlocker = MakeAndAddRef<Blocker>(this, name);
    nsresult rv = GetShutdownBarrier()->AddBlocker(
        mShutdownBlocker, NS_LITERAL_STRING(__FILE__), __LINE__,
        NS_LITERAL_STRING("MediaRecorder::Session: shutdown"));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

    mEncoder = MediaEncoder::CreateEncoder(
        mEncoderThread, mMimeType, mAudioBitsPerSecond, mVideoBitsPerSecond,
        aTrackTypes, aTrackRate);

    if (!mEncoder) {
      LOG(LogLevel::Error, ("Session.InitEncoder !mEncoder %p", this));
      DoSessionEndTask(NS_ERROR_ABORT);
      return;
    }

    mEncoderListener = MakeAndAddRef<EncoderListener>(mEncoderThread, this);
    rv = mEncoderThread->Dispatch(NewRunnableMethod<RefPtr<EncoderListener>>(
        "mozilla::MediaEncoder::RegisterListener", mEncoder,
        &MediaEncoder::RegisterListener, mEncoderListener));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;

    if (mRecorder->mAudioNode) {
      mEncoder->ConnectAudioNode(mRecorder->mAudioNode,
                                 mRecorder->mAudioNodeOutput);
    }

    for (const auto& track : mMediaStreamTracks) {
      mEncoder->ConnectMediaStreamTrack(track);
    }

    // If a timeslice is defined we set an appropriate video keyframe interval.
    // This allows users to get blobs regularly when the timeslice interval is
    // shorter than the default key frame interval, as we'd normally wait for a
    // key frame before sending data to the blob.
    mEncoder->SetVideoKeyFrameInterval(
        std::max(TimeDuration::FromSeconds(1), mTimeslice).ToMilliseconds());

    // Set mRunningState to Running so that DoSessionEndTask will
    // take the responsibility to end the session.
    mRunningState = RunningState::Starting;
  }

  // This is the task that will stop recording per spec:
  // - Stop gathering data (this is inherently async)
  // - Set state to "inactive"
  // - Fire an error event, if NS_FAILED(rv)
  // - Discard blob data if rv is NS_ERROR_DOM_SECURITY_ERR
  // - Fire a Blob event
  // - Fire an event named stop
  void DoSessionEndTask(nsresult rv) {
    MOZ_ASSERT(NS_IsMainThread());
    if (mRunningState.isErr()) {
      // We have already ended with an error.
      return;
    }

    if (mRunningState.isOk() &&
        mRunningState.inspect() == RunningState::Stopped) {
      // We have already ended gracefully.
      return;
    }

    bool needsStartEvent = false;
    if (mRunningState.isOk() &&
        (mRunningState.inspect() == RunningState::Idling ||
         mRunningState.inspect() == RunningState::Starting)) {
      needsStartEvent = true;
    }

    if (rv == NS_OK) {
      mRunningState = RunningState::Stopped;
    } else {
      mRunningState = Err(rv);
    }

    GatherBlob()
        ->Then(
            mMainThread, __func__,
            [this, self = RefPtr<Session>(this), rv, needsStartEvent](
                const BlobPromise::ResolveOrRejectValue& aResult) {
              if (mRecorder->mSessions.LastElement() == this) {
                // Set state to inactive, but only if the recorder is not
                // controlled by another session already.
                mRecorder->Inactivate();
              }

              if (needsStartEvent) {
                mRecorder->DispatchSimpleEvent(NS_LITERAL_STRING("start"));
              }

              // If there was an error, Fire the appropriate one
              if (NS_FAILED(rv)) {
                mRecorder->NotifyError(rv);
              }

              // Fire a blob event named dataavailable
              RefPtr<BlobImpl> blobImpl;
              if (rv == NS_ERROR_DOM_SECURITY_ERR || aResult.IsReject()) {
                // In case of SecurityError, the blob data must be discarded.
                // We create a new empty one and throw the blob with its data
                // away.
                // In case we failed to gather blob data, we create an empty
                // memory blob instead.
                blobImpl = new EmptyBlobImpl(mMimeType);
              } else {
                blobImpl = aResult.ResolveValue();
              }
              if (NS_FAILED(mRecorder->CreateAndDispatchBlobEvent(blobImpl))) {
                // Failed to dispatch blob event. That's unexpected. It's
                // probably all right to fire an error event if we haven't
                // already.
                if (NS_SUCCEEDED(rv)) {
                  mRecorder->NotifyError(NS_ERROR_FAILURE);
                }
              }

              // Fire an event named stop
              mRecorder->DispatchSimpleEvent(NS_LITERAL_STRING("stop"));

              // And finally, Shutdown and destroy the Session
              return Shutdown();
            })
        ->Then(mMainThread, __func__, [this, self = RefPtr<Session>(this)] {
          GetShutdownBarrier()->RemoveBlocker(mShutdownBlocker);
          mShutdownBlocker = nullptr;
        });
  }

  void MediaEncoderInitialized() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    // Start issuing timeslice-based blobs.
    MOZ_ASSERT(mLastBlobTimeStamp.IsNull());
    mLastBlobTimeStamp = TimeStamp::Now();

    Extract(mLastBlobTimeStamp, false);

    NS_DispatchToMainThread(NewRunnableFrom([self = RefPtr<Session>(this), this,
                                             mime = mEncoder->MimeType()]() {
      if (mRunningState.isErr()) {
        return NS_OK;
      }
      RunningState state = self->mRunningState.inspect();
      if (state == RunningState::Starting || state == RunningState::Stopping) {
        if (state == RunningState::Starting) {
          // We set it to Running in the runnable since we can only assign
          // mRunningState on main thread. We set it before running the start
          // event runnable since that dispatches synchronously (and may cause
          // js calls to methods depending on mRunningState).
          mRunningState = RunningState::Running;

          mRecorder->mMimeType = mMimeType;
        }
        mRecorder->DispatchSimpleEvent(NS_LITERAL_STRING("start"));
      }
      return NS_OK;
    }));
  }

  void MediaEncoderDataAvailable() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());

    Extract(TimeStamp::Now(), false);
  }

  void MediaEncoderError() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    NS_DispatchToMainThread(NewRunnableMethod<nsresult>(
        "dom::MediaRecorder::Session::DoSessionEndTask", this,
        &Session::DoSessionEndTask, NS_ERROR_FAILURE));
  }

  void MediaEncoderShutdown() {
    MOZ_ASSERT(mEncoderThread->IsCurrentThreadIn());
    mEncoder->AssertShutdownCalled();

    mMainThread->Dispatch(NewRunnableMethod<nsresult>(
        "MediaRecorder::Session::MediaEncoderShutdown->DoSessionEndTask", this,
        &Session::DoSessionEndTask, NS_OK));

    // Clean up.
    mEncoderListener->Forget();
    DebugOnly<bool> unregistered =
        mEncoder->UnregisterListener(mEncoderListener);
    MOZ_ASSERT(unregistered);
  }

  RefPtr<ShutdownPromise> Shutdown() {
    MOZ_ASSERT(NS_IsMainThread());
    LOG(LogLevel::Debug, ("Session Shutdown %p", this));

    if (mShutdownPromise) {
      return mShutdownPromise;
    }

    mShutdownPromise = ShutdownPromise::CreateAndResolve(true, __func__);

    if (mEncoder) {
      MOZ_RELEASE_ASSERT(mEncoderListener);
      mShutdownPromise =
          mShutdownPromise
              ->Then(mEncoderThread, __func__,
                     [encoder = mEncoder, encoderListener = mEncoderListener] {
                       // Unregister the listener before canceling so that we
                       // don't get the Shutdown notification from Cancel().
                       encoder->UnregisterListener(encoderListener);
                       encoderListener->Forget();
                       return ShutdownPromise::CreateAndResolve(true, __func__);
                     })
              ->Then(mMainThread, __func__,
                     [encoder = mEncoder] { return encoder->Cancel(); })
              ->Then(mEncoderThread, __func__, [] {
                // Meh, this is just to convert the promise type to match
                // mShutdownPromise.
                return ShutdownPromise::CreateAndResolve(true, __func__);
              });
    }

    // Remove main thread state. This could be needed if Stop() wasn't called.
    if (mMediaStream) {
      mMediaStream->UnregisterTrackListener(this);
      mMediaStream = nullptr;
    }

    {
      auto tracks(std::move(mMediaStreamTracks));
      for (RefPtr<MediaStreamTrack>& track : tracks) {
        track->RemovePrincipalChangeObserver(this);
      }
    }

    // Break the cycle reference between Session and MediaRecorder.
    mShutdownPromise = mShutdownPromise->Then(
        mMainThread, __func__,
        [self = RefPtr<Session>(this)]() {
          self->mRecorder->RemoveSession(self);
          return ShutdownPromise::CreateAndResolve(true, __func__);
        },
        []() {
          MOZ_ASSERT_UNREACHABLE("Unexpected reject");
          return ShutdownPromise::CreateAndReject(false, __func__);
        });

    if (mEncoderThread) {
      mShutdownPromise = mShutdownPromise->Then(
          mMainThread, __func__,
          [encoderThread = mEncoderThread]() {
            return encoderThread->BeginShutdown();
          },
          []() {
            MOZ_ASSERT_UNREACHABLE("Unexpected reject");
            return ShutdownPromise::CreateAndReject(false, __func__);
          });
    }

    return mShutdownPromise;
  }

 private:
  enum class RunningState {
    Idling,    // Session has been created
    Starting,  // MediaEncoder started, waiting for data
    Running,   // MediaEncoder has produced data
    Stopping,  // Stop() has been called
    Stopped,   // Session has stopped without any error
  };

  // Our associated MediaRecorder.
  const RefPtr<MediaRecorder> mRecorder;

  // Stream currently recorded.
  RefPtr<DOMMediaStream> mMediaStream;

  // Tracks currently recorded. This should be a subset of mMediaStream's track
  // set.
  nsTArray<RefPtr<MediaStreamTrack>> mMediaStreamTracks;

  // Main thread used for MozPromise operations.
  const RefPtr<nsISerialEventTarget> mMainThread;
  // Runnable thread for reading data from MediaEncoder.
  RefPtr<TaskQueue> mEncoderThread;
  // MediaEncoder pipeline.
  RefPtr<MediaEncoder> mEncoder;
  // Listener through which MediaEncoder signals us.
  RefPtr<EncoderListener> mEncoderListener;
  // Set in Shutdown() and resolved when shutdown is complete.
  RefPtr<ShutdownPromise> mShutdownPromise;
  // A buffer to cache encoded media data.
  RefPtr<MutableBlobStorage> mMutableBlobStorage;
  // Max memory to use for the MutableBlobStorage.
  uint64_t mMaxMemory;
  // If set, is a promise for the latest GatherBlob() operation. Allows
  // GatherBlob() operations to be serialized in order to avoid races.
  RefPtr<BlobPromise> mBlobPromise;
  // Session mimeType
  const nsString mMimeType;
  // Timestamp of the last fired dataavailable event.
  TimeStamp mLastBlobTimeStamp;
  // The interval of passing encoded data from MutableBlobStorage to
  // onDataAvailable handler.
  const TimeDuration mTimeslice;
  // The video bitrate the recorder was configured with.
  const uint32_t mVideoBitsPerSecond;
  // The audio bitrate the recorder was configured with.
  const uint32_t mAudioBitsPerSecond;
  // The session's current main thread state. The error type gets set when
  // ending a recording with an error. An NS_OK error is invalid.
  // Main thread only.
  Result<RunningState, nsresult> mRunningState;
  // Shutdown blocker unique for this Session. Main thread only.
  RefPtr<ShutdownBlocker> mShutdownBlocker;
};

MediaRecorder::~MediaRecorder() {
  LOG(LogLevel::Debug, ("~MediaRecorder (%p)", this));
  UnRegisterActivityObserver();
}

MediaRecorder::MediaRecorder(nsPIDOMWindowInner* aOwnerWindow)
    : DOMEventTargetHelper(aOwnerWindow) {
  MOZ_ASSERT(aOwnerWindow);
  RegisterActivityObserver();
}

void MediaRecorder::RegisterActivityObserver() {
  if (nsPIDOMWindowInner* window = GetOwner()) {
    mDocument = window->GetExtantDoc();
    if (mDocument) {
      mDocument->RegisterActivityObserver(
          NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
    }
  }
}

void MediaRecorder::UnRegisterActivityObserver() {
  if (mDocument) {
    mDocument->UnregisterActivityObserver(
        NS_ISUPPORTS_CAST(nsIDocumentActivity*, this));
  }
}

void MediaRecorder::GetMimeType(nsString& aMimeType) { aMimeType = mMimeType; }

void MediaRecorder::Start(const Optional<uint32_t>& aTimeslice,
                          ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Start %p", this));

  InitializeDomExceptions();

  // When a MediaRecorder object???s start() method is invoked, the UA MUST run
  // the following steps:

  // 1. Let recorder be the MediaRecorder object on which the method was
  //    invoked.

  // 2. Let timeslice be the method???s first argument, if provided, or undefined.
  TimeDuration timeslice =
      aTimeslice.WasPassed()
          ? TimeDuration::FromMilliseconds(aTimeslice.Value())
          : TimeDuration::Forever();

  // 3. Let stream be the value of recorder???s stream attribute.

  // 4. Let tracks be the set of live tracks in stream???s track set.
  nsTArray<RefPtr<MediaStreamTrack>> tracks;
  if (mStream) {
    mStream->GetTracks(tracks);
  }
  for (const auto& t : nsTArray<RefPtr<MediaStreamTrack>>(tracks)) {
    if (t->Ended()) {
      tracks.RemoveElement(t);
    }
  }

  // 5. If the value of recorder???s state attribute is not inactive, throw an
  //    InvalidStateError DOMException and abort these steps.
  if (mState != RecordingState::Inactive) {
    aResult.ThrowInvalidStateError(
        "The MediaRecorder has already been started");
    return;
  }

  // 6. If the isolation properties of stream disallow access from recorder,
  //    throw a SecurityError DOMException and abort these steps.
  if (mStream) {
    RefPtr<nsIPrincipal> streamPrincipal = mStream->GetPrincipal();
    if (!PrincipalSubsumes(this, streamPrincipal)) {
      aResult.ThrowSecurityError(
          "The MediaStream's isolation properties disallow access from "
          "MediaRecorder");
      return;
    }
  }
  if (mAudioNode && !AudioNodePrincipalSubsumes(this, mAudioNode)) {
    LOG(LogLevel::Warning,
        ("MediaRecorder %p Start AudioNode principal check failed", this));
    aResult.ThrowSecurityError(
        "The AudioNode's isolation properties disallow access from "
        "MediaRecorder");
    return;
  }

  // 7. If stream is inactive, throw a NotSupportedError DOMException and abort
  //    these steps.
  if (mStream && !mStream->Active()) {
    aResult.ThrowNotSupportedError("The MediaStream is inactive");
    return;
  }

  // 8. If the [[ConstrainedMimeType]] slot specifies a media type, container,
  //    or codec, then run the following sub steps:
  //   1. Constrain the configuration of recorder to the media type, container,
  //      and codec specified in the [[ConstrainedMimeType]] slot.
  //   2. For each track in tracks, if the User Agent cannot record the track
  //      using the current configuration, then throw a NotSupportedError
  //      DOMException and abort all steps.
  Maybe<MediaContainerType> mime;
  if (mConstrainedMimeType.Length() > 0) {
    mime = MakeMediaContainerType(mConstrainedMimeType);
    MOZ_DIAGNOSTIC_ASSERT(
        mime,
        "Invalid media MIME type should have been caught by IsTypeSupported");
  }
  for (const auto& track : tracks) {
    TypeSupport support = CanRecordWith(track, mime, mConstrainedMimeType);
    if (support != TypeSupport::Supported) {
      nsString id;
      track->GetId(id);
      aResult.ThrowNotSupportedError(nsPrintfCString(
          "%s track cannot be recorded: %s",
          track->AsAudioStreamTrack() ? "An audio" : "A video",
          TypeSupportToCString(support, mConstrainedMimeType).get()));
      return;
    }
  }
  if (mAudioNode) {
    TypeSupport support = CanRecordAudioTrackWith(mime, mConstrainedMimeType);
    if (support != TypeSupport::Supported) {
      aResult.ThrowNotSupportedError(nsPrintfCString(
          "An AudioNode cannot be recorded: %s",
          TypeSupportToCString(support, mConstrainedMimeType).get()));
      return;
    }
  }

  // 9. If recorder???s [[ConstrainedBitsPerSecond]] slot is not undefined, set
  //    recorder???s videoBitsPerSecond and audioBitsPerSecond attributes to
  //    values the User Agent deems reasonable for the respective media types,
  //    for recording all tracks in tracks, such that the sum of
  //    videoBitsPerSecond and audioBitsPerSecond is close to the value of
  //    recorder???s
  //    [[ConstrainedBitsPerSecond]] slot.
  if (mConstrainedBitsPerSecond) {
    uint8_t numVideoTracks = 0;
    uint8_t numAudioTracks = 0;
    for (const auto& t : tracks) {
      if (t->AsVideoStreamTrack() && numVideoTracks < UINT8_MAX) {
        ++numVideoTracks;
      } else if (t->AsAudioStreamTrack() && numAudioTracks < UINT8_MAX) {
        ++numAudioTracks;
      }
    }
    if (mAudioNode) {
      MOZ_DIAGNOSTIC_ASSERT(!mStream);
      ++numAudioTracks;
    }
    SelectBitrates(*mConstrainedBitsPerSecond, numVideoTracks,
                   &mVideoBitsPerSecond, numAudioTracks, &mAudioBitsPerSecond);
  }

  // 10. Let videoBitrate be the value of recorder???s videoBitsPerSecond
  //     attribute, and constrain the configuration of recorder to target an
  //     aggregate bitrate of videoBitrate bits per second for all video tracks
  //     recorder will be recording. videoBitrate is a hint for the encoder and
  //     the value might be surpassed, not achieved, or only be achieved over a
  //     long period of time.
  const uint32_t videoBitrate = mVideoBitsPerSecond;

  // 11. Let audioBitrate be the value of recorder???s audioBitsPerSecond
  //     attribute, and constrain the configuration of recorder to target an
  //     aggregate bitrate of audioBitrate bits per second for all audio tracks
  //     recorder will be recording. audioBitrate is a hint for the encoder and
  //     the value might be surpassed, not achieved, or only be achieved over a
  //     long period of time.
  const uint32_t audioBitrate = mAudioBitsPerSecond;

  // 12. Set recorder???s state to recording
  mState = RecordingState::Recording;

  MediaRecorderReporter::AddMediaRecorder(this);
  // Start a session.
  mSessions.AppendElement();
  mSessions.LastElement() = new Session(this, std::move(tracks), timeslice,
                                        videoBitrate, audioBitrate);
  mSessions.LastElement()->Start();
}

void MediaRecorder::Stop(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Stop %p", this));
  MediaRecorderReporter::RemoveMediaRecorder(this);

  // When a MediaRecorder object???s stop() method is invoked, the UA MUST run the
  // following steps:

  // 1. Let recorder be the MediaRecorder object on which the method was
  //    invoked.

  // 2. If recorder???s state attribute is inactive, abort these steps.
  if (mState == RecordingState::Inactive) {
    return;
  }

  // 3. Inactivate the recorder with recorder.
  Inactivate();

  // 4. Queue a task, using the DOM manipulation task source, that runs the
  //    following steps:
  //   1. Stop gathering data.
  //   2. Let blob be the Blob of collected data so far, then fire a blob event
  //      named dataavailable at recorder with blob.
  //   3. Fire an event named stop at recorder.
  MOZ_ASSERT(mSessions.Length() > 0);
  mSessions.LastElement()->Stop();

  // 5. return undefined.
}

void MediaRecorder::Pause(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Pause %p", this));

  // When a MediaRecorder object???s pause() method is invoked, the UA MUST run
  // the following steps:

  // 1. If state is inactive, throw an InvalidStateError DOMException and abort
  //    these steps.
  if (mState == RecordingState::Inactive) {
    aResult.ThrowInvalidStateError("The MediaRecorder is inactive");
    return;
  }

  // 2. If state is paused, abort these steps.
  if (mState == RecordingState::Paused) {
    return;
  }

  // 3. Set state to paused, and queue a task, using the DOM manipulation task
  //    source, that runs the following steps:
  mState = RecordingState::Paused;

  // XXX - We pause synchronously pending spec issue
  //       https://github.com/w3c/mediacapture-record/issues/131
  //   1. Stop gathering data into blob (but keep it available so that
  //      recording can be resumed in the future).
  MOZ_ASSERT(!mSessions.IsEmpty());
  mSessions.LastElement()->Pause();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaRecorder::Pause", [recorder = RefPtr<MediaRecorder>(this)] {
        // 2. Let target be the MediaRecorder context object. Fire an event
        //    named pause at target.
        recorder->DispatchSimpleEvent(NS_LITERAL_STRING("pause"));
      }));

  // 4. return undefined.
}

void MediaRecorder::Resume(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.Resume %p", this));

  // When a MediaRecorder object???s resume() method is invoked, the UA MUST run
  // the following steps:

  // 1. If state is inactive, throw an InvalidStateError DOMException and abort
  //    these steps.
  if (mState == RecordingState::Inactive) {
    aResult.ThrowInvalidStateError("The MediaRecorder is inactive");
    return;
  }

  // 2. If state is recording, abort these steps.
  if (mState == RecordingState::Recording) {
    return;
  }

  // 3. Set state to recording, and queue a task, using the DOM manipulation
  //    task source, that runs the following steps:
  mState = RecordingState::Recording;

  // XXX - We resume synchronously pending spec issue
  //       https://github.com/w3c/mediacapture-record/issues/131
  //   1. Resume (or continue) gathering data into the current blob.
  MOZ_ASSERT(!mSessions.IsEmpty());
  mSessions.LastElement()->Resume();

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MediaRecorder::Resume", [recorder = RefPtr<MediaRecorder>(this)] {
        // 2. Let target be the MediaRecorder context object. Fire an event
        //    named resume at target.
        recorder->DispatchSimpleEvent(NS_LITERAL_STRING("resume"));
      }));

  // 4. return undefined.
}

void MediaRecorder::RequestData(ErrorResult& aResult) {
  LOG(LogLevel::Debug, ("MediaRecorder.RequestData %p", this));

  // When a MediaRecorder object???s requestData() method is invoked, the UA MUST
  // run the following steps:

  // 1. If state is inactive throw an InvalidStateError DOMException and
  //    terminate these steps. Otherwise the UA MUST queue a task, using the DOM
  //    manipulation task source, that runs the following steps:
  //   1. Let blob be the Blob of collected data so far and let target be the
  //      MediaRecorder context object, then fire a blob event named
  //      dataavailable at target with blob. (Note that blob will be empty if no
  //      data has been gathered yet.)
  //   2. Create a new Blob and gather subsequent data into it.
  if (mState == RecordingState::Inactive) {
    aResult.ThrowInvalidStateError("The MediaRecorder is inactive");
    return;
  }
  MOZ_ASSERT(mSessions.Length() > 0);
  mSessions.LastElement()->RequestData();

  // 2. return undefined.
}

JSObject* MediaRecorder::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return MediaRecorder_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<MediaRecorder> MediaRecorder::Constructor(
    const GlobalObject& aGlobal, DOMMediaStream& aStream,
    const MediaRecorderOptions& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // When the MediaRecorder() constructor is invoked, the User Agent MUST run
  // the following steps:

  // 1. Let stream be the constructor???s first argument.

  // 2. Let options be the constructor???s second argument.

  // 3. If invoking is type supported with options??? mimeType member as its
  //    argument returns false, throw a NotSupportedError DOMException and abort
  //    these steps.
  TypeSupport support = IsTypeSupportedImpl(aOptions.mMimeType);
  if (support != TypeSupport::Supported) {
    // This catches also the empty string mimeType when support for any encoders
    // has been disabled.
    aRv.ThrowNotSupportedError(
        TypeSupportToCString(support, aOptions.mMimeType));
    return nullptr;
  }

  // 4. Let recorder be a newly constructed MediaRecorder object.
  RefPtr<MediaRecorder> recorder = new MediaRecorder(ownerWindow);

  // 5. Let recorder have a [[ConstrainedMimeType]] internal slot, initialized
  //    to the value of options' mimeType member.
  recorder->mConstrainedMimeType = aOptions.mMimeType;

  // 6. Let recorder have a [[ConstrainedBitsPerSecond]] internal slot,
  //    initialized to the value of options??? bitsPerSecond member, if it is
  //    present, otherwise undefined.
  recorder->mConstrainedBitsPerSecond =
      aOptions.mBitsPerSecond.WasPassed()
          ? Some(aOptions.mBitsPerSecond.Value())
          : Nothing();

  // 7. Initialize recorder???s stream attribute to stream.
  recorder->mStream = &aStream;

  // 8. Initialize recorder???s mimeType attribute to the value of recorder???s
  //    [[ConstrainedMimeType]] slot.
  recorder->mMimeType = recorder->mConstrainedMimeType;

  // 9. Initialize recorder???s state attribute to inactive.
  recorder->mState = RecordingState::Inactive;

  // 10. Initialize recorder???s videoBitsPerSecond attribute to the value of
  //     options??? videoBitsPerSecond member, if it is present. Otherwise, choose
  //     a target value the User Agent deems reasonable for video.
  recorder->mVideoBitsPerSecond = aOptions.mVideoBitsPerSecond.WasPassed()
                                      ? aOptions.mVideoBitsPerSecond.Value()
                                      : DEFAULT_VIDEO_BITRATE_BPS;

  // 11. Initialize recorder???s audioBitsPerSecond attribute to the value of
  //     options??? audioBitsPerSecond member, if it is present. Otherwise, choose
  //     a target value the User Agent deems reasonable for audio.
  recorder->mAudioBitsPerSecond = aOptions.mAudioBitsPerSecond.WasPassed()
                                      ? aOptions.mAudioBitsPerSecond.Value()
                                      : DEFAULT_AUDIO_BITRATE_BPS;

  // 12. If recorder???s [[ConstrainedBitsPerSecond]] slot is not undefined, set
  //     recorder???s videoBitsPerSecond and audioBitsPerSecond attributes to
  //     values the User Agent deems reasonable for the respective media types,
  //     such that the sum of videoBitsPerSecond and audioBitsPerSecond is close
  //     to the value of recorder???s [[ConstrainedBitsPerSecond]] slot.
  if (recorder->mConstrainedBitsPerSecond) {
    SelectBitrates(*recorder->mConstrainedBitsPerSecond, 1,
                   &recorder->mVideoBitsPerSecond, 1,
                   &recorder->mAudioBitsPerSecond);
  }

  // 13. Return recorder.
  return recorder.forget();
}

/* static */
already_AddRefed<MediaRecorder> MediaRecorder::Constructor(
    const GlobalObject& aGlobal, AudioNode& aAudioNode,
    uint32_t aAudioNodeOutput, const MediaRecorderOptions& aOptions,
    ErrorResult& aRv) {
  // Allow recording from audio node only when pref is on.
  if (!Preferences::GetBool("media.recorder.audio_node.enabled", false)) {
    // Pretending that this constructor is not defined.
    aRv.ThrowTypeError<MSG_DOES_NOT_IMPLEMENT_INTERFACE>("Argument 1",
                                                         "MediaStream");
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  // aAudioNodeOutput doesn't matter to destination node because it has no
  // output.
  if (aAudioNode.NumberOfOutputs() > 0 &&
      aAudioNodeOutput >= aAudioNode.NumberOfOutputs()) {
    aRv.ThrowIndexSizeError("Invalid AudioNode output index");
    return nullptr;
  }

  // When the MediaRecorder() constructor is invoked, the User Agent MUST run
  // the following steps:

  // 1. Let stream be the constructor???s first argument. (we'll let audioNode be
  //    the first arg, and audioNodeOutput the second)

  // 2. Let options be the constructor???s second argument. (we'll let options be
  //    the third arg)

  // 3. If invoking is type supported with options??? mimeType member as its
  //    argument returns false, throw a NotSupportedError DOMException and abort
  //    these steps.
  TypeSupport support = IsTypeSupportedImpl(aOptions.mMimeType);
  if (support != TypeSupport::Supported) {
    // This catches also the empty string mimeType when support for any encoders
    // has been disabled.
    aRv.ThrowNotSupportedError(
        TypeSupportToCString(support, aOptions.mMimeType));
    return nullptr;
  }

  // 4. Let recorder be a newly constructed MediaRecorder object.
  RefPtr<MediaRecorder> recorder = new MediaRecorder(ownerWindow);

  // 5. Let recorder have a [[ConstrainedMimeType]] internal slot, initialized
  //    to the value of options' mimeType member.
  recorder->mConstrainedMimeType = aOptions.mMimeType;

  // 6. Let recorder have a [[ConstrainedBitsPerSecond]] internal slot,
  //    initialized to the value of options??? bitsPerSecond member, if it is
  //    present, otherwise undefined.
  recorder->mConstrainedBitsPerSecond =
      aOptions.mBitsPerSecond.WasPassed()
          ? Some(aOptions.mBitsPerSecond.Value())
          : Nothing();

  // 7. Initialize recorder???s stream attribute to stream. (make that the
  //    audioNode and audioNodeOutput equivalents)
  recorder->mAudioNode = &aAudioNode;
  recorder->mAudioNodeOutput = aAudioNodeOutput;

  // 8. Initialize recorder???s mimeType attribute to the value of recorder???s
  //    [[ConstrainedMimeType]] slot.
  recorder->mMimeType = recorder->mConstrainedMimeType;

  // 9. Initialize recorder???s state attribute to inactive.
  recorder->mState = RecordingState::Inactive;

  // 10. Initialize recorder???s videoBitsPerSecond attribute to the value of
  //     options??? videoBitsPerSecond member, if it is present. Otherwise, choose
  //     a target value the User Agent deems reasonable for video.
  recorder->mVideoBitsPerSecond = aOptions.mVideoBitsPerSecond.WasPassed()
                                      ? aOptions.mVideoBitsPerSecond.Value()
                                      : DEFAULT_VIDEO_BITRATE_BPS;

  // 11. Initialize recorder???s audioBitsPerSecond attribute to the value of
  //     options??? audioBitsPerSecond member, if it is present. Otherwise, choose
  //     a target value the User Agent deems reasonable for audio.
  recorder->mAudioBitsPerSecond = aOptions.mAudioBitsPerSecond.WasPassed()
                                      ? aOptions.mAudioBitsPerSecond.Value()
                                      : DEFAULT_AUDIO_BITRATE_BPS;

  // 12. If recorder???s [[ConstrainedBitsPerSecond]] slot is not undefined, set
  //     recorder???s videoBitsPerSecond and audioBitsPerSecond attributes to
  //     values the User Agent deems reasonable for the respective media types,
  //     such that the sum of videoBitsPerSecond and audioBitsPerSecond is close
  //     to the value of recorder???s [[ConstrainedBitsPerSecond]] slot.
  if (recorder->mConstrainedBitsPerSecond) {
    SelectBitrates(*recorder->mConstrainedBitsPerSecond, 1,
                   &recorder->mVideoBitsPerSecond, 1,
                   &recorder->mAudioBitsPerSecond);
  }

  // 13. Return recorder.
  return recorder.forget();
}

/* static */
bool MediaRecorder::IsTypeSupported(GlobalObject& aGlobal,
                                    const nsAString& aMIMEType) {
  return MediaRecorder::IsTypeSupported(aMIMEType);
}

/* static */
bool MediaRecorder::IsTypeSupported(const nsAString& aMIMEType) {
  return IsTypeSupportedImpl(aMIMEType) == TypeSupport::Supported;
}

nsresult MediaRecorder::CreateAndDispatchBlobEvent(BlobImpl* aBlobImpl) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");

  if (!GetOwnerGlobal()) {
    // This MediaRecorder has been disconnected in the meantime.
    return NS_ERROR_FAILURE;
  }

  RefPtr<Blob> blob = Blob::Create(GetOwnerGlobal(), aBlobImpl);
  if (NS_WARN_IF(!blob)) {
    return NS_ERROR_FAILURE;
  }

  BlobEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mData = blob;

  RefPtr<BlobEvent> event =
      BlobEvent::Constructor(this, NS_LITERAL_STRING("dataavailable"), init);
  event->SetTrusted(true);
  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

void MediaRecorder::DispatchSimpleEvent(const nsAString& aStr) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }

  rv = DOMEventTargetHelper::DispatchTrustedEvent(aStr);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error,
        ("MediaRecorder.DispatchSimpleEvent: DispatchTrustedEvent failed  %p",
         this));
    NS_ERROR("Failed to dispatch the event!!!");
  }
}

void MediaRecorder::NotifyError(nsresult aRv) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  nsresult rv = CheckCurrentGlobalCorrectness();
  if (NS_FAILED(rv)) {
    return;
  }
  MediaRecorderErrorEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  // These DOMExceptions have been created earlier so they can contain stack
  // traces. We attach the appropriate one here to be fired. We should have
  // exceptions here, but defensively check.
  switch (aRv) {
    case NS_ERROR_DOM_SECURITY_ERR:
      if (!mSecurityDomException) {
        LOG(LogLevel::Debug, ("MediaRecorder.NotifyError: "
                              "mSecurityDomException was not initialized"));
        mSecurityDomException = DOMException::Create(NS_ERROR_DOM_SECURITY_ERR);
      }
      init.mError = std::move(mSecurityDomException);
      break;
    default:
      if (!mUnknownDomException) {
        LOG(LogLevel::Debug, ("MediaRecorder.NotifyError: "
                              "mUnknownDomException was not initialized"));
        mUnknownDomException = DOMException::Create(NS_ERROR_DOM_UNKNOWN_ERR);
      }
      LOG(LogLevel::Debug, ("MediaRecorder.NotifyError: "
                            "mUnknownDomException being fired for aRv: %X",
                            uint32_t(aRv)));
      init.mError = std::move(mUnknownDomException);
  }

  RefPtr<MediaRecorderErrorEvent> event = MediaRecorderErrorEvent::Constructor(
      this, NS_LITERAL_STRING("error"), init);
  event->SetTrusted(true);

  IgnoredErrorResult res;
  DispatchEvent(*event, res);
  if (res.Failed()) {
    NS_ERROR("Failed to dispatch the error event!!!");
  }
}

void MediaRecorder::RemoveSession(Session* aSession) {
  LOG(LogLevel::Debug, ("MediaRecorder.RemoveSession (%p)", aSession));
  mSessions.RemoveElement(aSession);
}

void MediaRecorder::NotifyOwnerDocumentActivityChanged() {
  nsPIDOMWindowInner* window = GetOwner();
  NS_ENSURE_TRUE_VOID(window);
  Document* doc = window->GetExtantDoc();
  NS_ENSURE_TRUE_VOID(doc);

  LOG(LogLevel::Debug, ("MediaRecorder %p NotifyOwnerDocumentActivityChanged "
                        "IsActive=%d, "
                        "IsVisible=%d, ",
                        this, doc->IsActive(), doc->IsVisible()));
  if (!doc->IsActive() || !doc->IsVisible()) {
    // Stop the session.
    ErrorResult result;
    Stop(result);
    result.SuppressException();
  }
}

void MediaRecorder::Inactivate() {
  LOG(LogLevel::Debug, ("MediaRecorder.Inactivate %p", this));
  // The Inactivate the recorder algorithm given a recorder, is as follows:

  // 1. Set recorder???s mimeType attribute to the value of the
  //    [[ConstrainedMimeType]] slot.
  mMimeType = mConstrainedMimeType;

  // 2. Set recorder???s state attribute to inactive.
  mState = RecordingState::Inactive;

  // 3. If recorder???s [[ConstrainedBitsPerSecond]] slot is not undefined, set
  //    recorder???s videoBitsPerSecond and audioBitsPerSecond attributes to
  //    values the User Agent deems reasonable for the respective media types,
  //    such that the sum of videoBitsPerSecond and audioBitsPerSecond is close
  //    to the value of recorder???s [[ConstrainedBitsPerSecond]] slot.
  if (mConstrainedBitsPerSecond) {
    SelectBitrates(*mConstrainedBitsPerSecond, 1, &mVideoBitsPerSecond, 1,
                   &mAudioBitsPerSecond);
  }
}

void MediaRecorder::InitializeDomExceptions() {
  mSecurityDomException = DOMException::Create(NS_ERROR_DOM_SECURITY_ERR);
  mUnknownDomException = DOMException::Create(NS_ERROR_DOM_UNKNOWN_ERR);
}

RefPtr<MediaRecorder::SizeOfPromise> MediaRecorder::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) {
  MOZ_ASSERT(NS_IsMainThread());

  // The return type of a chained MozPromise cannot be changed, so we create a
  // holder for our desired return type and resolve that from All()->Then().
  auto holder = MakeRefPtr<Refcountable<MozPromiseHolder<SizeOfPromise>>>();
  RefPtr<SizeOfPromise> promise = holder->Ensure(__func__);

  nsTArray<RefPtr<SizeOfPromise>> promises(mSessions.Length());
  for (const RefPtr<Session>& session : mSessions) {
    promises.AppendElement(session->SizeOfExcludingThis(aMallocSizeOf));
  }

  SizeOfPromise::All(GetCurrentSerialEventTarget(), promises)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [holder](const nsTArray<size_t>& sizes) {
            size_t total = 0;
            for (const size_t& size : sizes) {
              total += size;
            }
            holder->Resolve(total, __func__);
          },
          []() { MOZ_CRASH("Unexpected reject"); });

  return promise;
}

StaticRefPtr<MediaRecorderReporter> MediaRecorderReporter::sUniqueInstance;

}  // namespace dom
}  // namespace mozilla

#undef LOG
