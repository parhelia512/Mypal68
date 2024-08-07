/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "ProfileBuffer.h"
#include "ProfiledThreadData.h"
#include "ProfileJSONWriter.h"
#include "ThreadInfo.h"

ProfilerBacktrace::ProfilerBacktrace(
    const char* aName, int aThreadId,
    UniquePtr<mozilla::BlocksRingBuffer> aBlocksRingBuffer,
    mozilla::UniquePtr<ProfileBuffer> aProfileBuffer)
    : mName(strdup(aName)),
      mThreadId(aThreadId),
      mBlocksRingBuffer(std::move(aBlocksRingBuffer)),
      mProfileBuffer(std::move(aProfileBuffer)) {
  MOZ_COUNT_CTOR(ProfilerBacktrace);
  MOZ_ASSERT(
      !!mBlocksRingBuffer,
      "ProfilerBacktrace only takes a non-null UniquePtr<BlocksRingBuffer>");
  MOZ_ASSERT(
      !!mProfileBuffer,
      "ProfilerBacktrace only takes a non-null UniquePtr<ProfileBuffer>");
  MOZ_ASSERT(!mBlocksRingBuffer->IsThreadSafe(),
             "ProfilerBacktrace only takes a non-thread-safe BlocksRingBuffer");
}

ProfilerBacktrace::~ProfilerBacktrace() { MOZ_COUNT_DTOR(ProfilerBacktrace); }

void ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                                   const mozilla::TimeStamp& aProcessStartTime,
                                   UniqueStacks& aUniqueStacks) {
  // Unlike ProfiledThreadData::StreamJSON, we don't need to call
  // ProfileBuffer::AddJITInfoForRange because mProfileBuffer does not contain
  // any JitReturnAddr entries. For synchronous samples, JIT frames get expanded
  // at sample time.
  StreamSamplesAndMarkers(mName.get(), mThreadId, *mProfileBuffer, aWriter,
                          NS_LITERAL_CSTRING(""), aProcessStartTime,
                          /* aRegisterTime */ mozilla::TimeStamp(),
                          /* aUnregisterTime */ mozilla::TimeStamp(),
                          /* aSinceTime */ 0, aUniqueStacks);
}
