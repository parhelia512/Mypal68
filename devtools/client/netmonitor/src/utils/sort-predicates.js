/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getAbbreviatedMimeType,
  getEndTime,
  getResponseTime,
  getResponseHeader,
  getStartTime,
  ipToLong,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  RESPONSE_HEADERS,
} = require("devtools/client/netmonitor/src/constants");
const {
  getUrlBaseName,
} = require("devtools/client/netmonitor/src/utils/request-utils");

/**
 * Predicates used when sorting items.
 *
 * @param object first
 *        The first item used in the comparison.
 * @param object second
 *        The second item used in the comparison.
 * @return number
 *         <0 to sort first to a lower index than second
 *         =0 to leave first and second unchanged with respect to each other
 *         >0 to sort second to a lower index than first
 */

function compareValues(first, second) {
  if (first === second) {
    return 0;
  }
  return first > second ? 1 : -1;
}

function waterfall(first, second) {
  const result = compareValues(first.startedMs, second.startedMs);
  return result || compareValues(first.id, second.id);
}

function status(first, second) {
  const result = compareValues(first.status, second.status);
  return result || waterfall(first, second);
}

function method(first, second) {
  const result = compareValues(first.method, second.method);
  return result || waterfall(first, second);
}

function file(first, second) {
  const firstUrl = first.urlDetails.baseNameWithQuery.toLowerCase();
  const secondUrl = second.urlDetails.baseNameWithQuery.toLowerCase();
  const result = compareValues(firstUrl, secondUrl);
  return result || waterfall(first, second);
}

function url(first, second) {
  const firstUrl = first.url.toLowerCase();
  const secondUrl = second.url.toLowerCase();
  const result = compareValues(firstUrl, secondUrl);
  return result || waterfall(first, second);
}

function protocol(first, second) {
  const result = compareValues(first.httpVersion, second.httpVersion);
  return result || waterfall(first, second);
}

function scheme(first, second) {
  const result = compareValues(
    first.urlDetails.scheme,
    second.urlDetails.scheme
  );
  return result || waterfall(first, second);
}

function startTime(first, second) {
  const result = compareValues(getStartTime(first), getStartTime(second));
  return result || waterfall(first, second);
}

function endTime(first, second) {
  const result = compareValues(getEndTime(first), getEndTime(second));
  return result || waterfall(first, second);
}

function responseTime(first, second) {
  const result = compareValues(getResponseTime(first), getResponseTime(second));
  return result || waterfall(first, second);
}

function duration(first, second) {
  const result = compareValues(first.totalTime, second.totalTime);
  return result || waterfall(first, second);
}

function latency(first, second) {
  const { eventTimings: firstEventTimings = { timings: {} } } = first;
  const { eventTimings: secondEventTimings = { timings: {} } } = second;
  const result = compareValues(
    firstEventTimings.timings.wait,
    secondEventTimings.timings.wait
  );
  return result || waterfall(first, second);
}

function compareHeader(header, first, second) {
  const result = compareValues(
    getResponseHeader(first, header) || "",
    getResponseHeader(second, header) || ""
  );
  return result || waterfall(first, second);
}

const responseHeaders = RESPONSE_HEADERS.reduce(
  (acc, header) =>
    Object.assign(acc, {
      [header]: (first, second) => compareHeader(header, first, second),
    }),
  {}
);

function domain(first, second) {
  const firstDomain = first.urlDetails.host.toLowerCase();
  const secondDomain = second.urlDetails.host.toLowerCase();
  const result = compareValues(firstDomain, secondDomain);
  return result || waterfall(first, second);
}

function remoteip(first, second) {
  const firstIP = ipToLong(first.remoteAddress);
  const secondIP = ipToLong(second.remoteAddress);
  const result = compareValues(firstIP, secondIP);
  return result || waterfall(first, second);
}

function cause(first, second) {
  const firstCause = first.cause.type;
  const secondCause = second.cause.type;
  const result = compareValues(firstCause, secondCause);
  return result || waterfall(first, second);
}

function initiator(first, second) {
  const firstCause = first.cause.type;
  const secondCause = second.cause.type;

  let firstInitiator = "";
  let firstInitiatorLineNumber = 0;

  if (first.cause.lastFrame) {
    firstInitiator = getUrlBaseName(first.cause.lastFrame.filename);
    firstInitiatorLineNumber = first.cause.lastFrame.lineNumber;
  }

  let secondInitiator = "";
  let secondInitiatorLineNumber = 0;

  if (second.cause.lastFrame) {
    secondInitiator = getUrlBaseName(second.cause.lastFrame.filename);
    secondInitiatorLineNumber = second.cause.lastFrame.lineNumber;
  }

  let result;
  // if both initiators don't have a stack trace, compare their causes
  if (!firstInitiator && !secondInitiator) {
    result = compareValues(firstCause, secondCause);
  } else if (!firstInitiator || !secondInitiator) {
    // if one initiator doesn't have a stack trace but the other does, former should precede the latter
    result = compareValues(firstInitiatorLineNumber, secondInitiatorLineNumber);
  } else {
    result = compareValues(firstInitiator, secondInitiator);
    if (result === 0) {
      result = compareValues(
        firstInitiatorLineNumber,
        secondInitiatorLineNumber
      );
    }
  }

  return result || waterfall(first, second);
}

function setCookies(first, second) {
  let { responseCookies: firstResponseCookies = { cookies: [] } } = first;
  let { responseCookies: secondResponseCookies = { cookies: [] } } = second;
  firstResponseCookies = firstResponseCookies.cookies || firstResponseCookies;
  secondResponseCookies =
    secondResponseCookies.cookies || secondResponseCookies;
  const result = compareValues(
    firstResponseCookies.length,
    secondResponseCookies.length
  );
  return result || waterfall(first, second);
}

function cookies(first, second) {
  let { requestCookies: firstRequestCookies = { cookies: [] } } = first;
  let { requestCookies: secondRequestCookies = { cookies: [] } } = second;
  firstRequestCookies = firstRequestCookies.cookies || firstRequestCookies;
  secondRequestCookies = secondRequestCookies.cookies || secondRequestCookies;
  const result = compareValues(
    firstRequestCookies.length,
    secondRequestCookies.length
  );
  return result || waterfall(first, second);
}

function type(first, second) {
  const firstType = getAbbreviatedMimeType(first.mimeType).toLowerCase();
  const secondType = getAbbreviatedMimeType(second.mimeType).toLowerCase();
  const result = compareValues(firstType, secondType);
  return result || waterfall(first, second);
}

function transferred(first, second) {
  const result = compareValues(first.transferredSize, second.transferredSize);
  return result || waterfall(first, second);
}

function contentSize(first, second) {
  const result = compareValues(first.contentSize, second.contentSize);
  return result || waterfall(first, second);
}

const sorters = {
  status,
  method,
  domain,
  file,
  protocol,
  scheme,
  cookies,
  setCookies,
  remoteip,
  cause,
  initiator,
  type,
  transferred,
  contentSize,
  startTime,
  endTime,
  responseTime,
  duration,
  latency,
  waterfall,
  url,
};
exports.Sorters = Object.assign(sorters, responseHeaders);
