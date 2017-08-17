// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_timestamp.h"

#include <ctype.h>
#include <inttypes.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"

namespace shaka {
namespace media {
namespace {

bool GetTotalMilliseconds(uint64_t hours,
                          uint64_t minutes,
                          uint64_t seconds,
                          uint64_t ms,
                          uint64_t* out) {
  DCHECK(out);
  if (minutes > 59 || seconds > 59 || ms > 999) {
    VLOG(1) << "Hours:" << hours << " Minutes:" << minutes
            << " Seconds:" << seconds << " MS:" << ms
            << " shoud have never made it to GetTotalMilliseconds";
    return false;
  }
  *out = 60 * 60 * 1000 * hours + 60 * 1000 * minutes + 1000 * seconds + ms;
  return true;
}
}  // namespace

bool WebVttTimestampToMs(const base::StringPiece& source, uint64_t* out) {
  DCHECK(out);

  if (source.length() < 9) {
    LOG(WARNING) << "Timestamp '" << source << "' is mal-formed";
    return false;
  }

  const size_t minutes_begin = source.length() - 9;
  const size_t seconds_begin = source.length() - 6;
  const size_t milliseconds_begin = source.length() - 3;

  uint64_t hours = 0;
  uint64_t minutes = 0;
  uint64_t seconds = 0;
  uint64_t ms = 0;

  const bool has_hours =
      minutes_begin >= 3 && source[minutes_begin - 1] == ':' &&
      base::StringToUint64(source.substr(0, minutes_begin - 1), &hours);

  if ((minutes_begin == 0 || has_hours) && source[seconds_begin - 1] == ':' &&
      source[milliseconds_begin - 1] == '.' &&
      base::StringToUint64(source.substr(minutes_begin, 2), &minutes) &&
      base::StringToUint64(source.substr(seconds_begin, 2), &seconds) &&
      base::StringToUint64(source.substr(milliseconds_begin, 3), &ms)) {
    return GetTotalMilliseconds(hours, minutes, seconds, ms, out);
  }

  LOG(WARNING) << "Timestamp '" << source << "' is mal-formed";
  return false;
}

std::string MsToWebVttTimestamp(uint64_t ms) {
  uint64_t remaining = ms;

  uint64_t only_ms = remaining % 1000;
  remaining /= 1000;
  uint64_t only_seconds = remaining % 60;
  remaining /= 60;
  uint64_t only_minutes = remaining % 60;
  remaining /= 60;
  uint64_t only_hours = remaining;

  return base::StringPrintf("%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64
                            ".%03" PRIu64,
                            only_hours, only_minutes, only_seconds, only_ms);
}
}  // namespace media
}  // namespace shaka
