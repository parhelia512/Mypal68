/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PerformanceService.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

static StaticRefPtr<PerformanceService> gPerformanceService;
static StaticMutex gPerformanceServiceMutex;

/* static */
PerformanceService* PerformanceService::GetOrCreate() {
  StaticMutexAutoLock al(gPerformanceServiceMutex);

  if (!gPerformanceService) {
    gPerformanceService = new PerformanceService();
    ClearOnShutdown(&gPerformanceService);
  }

  return gPerformanceService;
}

DOMHighResTimeStamp PerformanceService::TimeOrigin(
    const TimeStamp& aCreationTimeStamp) const {
  return (aCreationTimeStamp - mCreationTimeStamp).ToMilliseconds() +
         (mCreationEpochTime / PR_USEC_PER_MSEC);
}

PerformanceService::PerformanceService() {
  mCreationTimeStamp = TimeStamp::Now();
  mCreationEpochTime = PR_Now();
}

}  // namespace dom
}  // namespace mozilla
