// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include <sys/time.h>
#include <time.h>
#if defined(OS_ANDROID)
#include <time64.h>
#endif

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#elif defined(OS_NACL)
#include "base/os_compat_nacl.h"
#endif

namespace {

// Define a system-specific SysTime that wraps either to a time_t or
// a time64_t depending on the host system, and associated convertion.
// See crbug.com/162007
#if defined(OS_ANDROID)
typedef time64_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  if (is_local)
    return mktime64(timestruct);
  else
    return timegm64(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  if (is_local)
    localtime64_r(&t, timestruct);
  else
    gmtime64_r(&t, timestruct);
}

#else  // OS_ANDROID
typedef time_t SysTime;

SysTime SysTimeFromTimeStruct(struct tm* timestruct, bool is_local) {
  if (is_local)
    return mktime(timestruct);
  else
    return timegm(timestruct);
}

void SysTimeToTimeStruct(SysTime t, struct tm* timestruct, bool is_local) {
  if (is_local)
    localtime_r(&t, timestruct);
  else
    gmtime_r(&t, timestruct);
}
#endif  // OS_ANDROID

#if !defined(OS_MACOSX)
// Helper function to get results from clock_gettime() as TimeTicks object.
// Minimum requirement is MONOTONIC_CLOCK to be supported on the system.
// FreeBSD 6 has CLOCK_MONOTONIC but defines _POSIX_MONOTONIC_CLOCK to -1.
#if (defined(OS_POSIX) &&                                               \
     defined(_POSIX_MONOTONIC_CLOCK) && _POSIX_MONOTONIC_CLOCK >= 0) || \
    defined(OS_BSD) || defined(OS_ANDROID)
base::TimeTicks ClockNow(clockid_t clk_id) {
  uint64_t absolute_micro;

  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    NOTREACHED() << "clock_gettime(" << clk_id << ") failed.";
    return base::TimeTicks();
  }

  absolute_micro =
      (static_cast<int64>(ts.tv_sec) * base::Time::kMicrosecondsPerSecond) +
      (static_cast<int64>(ts.tv_nsec) / base::Time::kNanosecondsPerMicrosecond);

  return base::TimeTicks::FromInternalValue(absolute_micro);
}
#else  // _POSIX_MONOTONIC_CLOCK
#error No usable tick clock function on this platform.
#endif  // _POSIX_MONOTONIC_CLOCK
#endif  // !defined(OS_MACOSX)

}  // namespace

namespace base {

struct timespec TimeDelta::ToTimeSpec() const {
  int64 microseconds = InMicroseconds();
  time_t seconds = 0;
  if (microseconds >= Time::kMicrosecondsPerSecond) {
    seconds = InSeconds();
    microseconds -= seconds * Time::kMicrosecondsPerSecond;
  }
  struct timespec result =
      {seconds,
       static_cast<long>(microseconds * Time::kNanosecondsPerMicrosecond)};
  return result;
}

#if !defined(OS_MACOSX)
// The Time routines in this file use standard POSIX routines, or almost-
// standard routines in the case of timegm.  We need to use a Mach-specific
// function for TimeTicks::Now() on Mac OS X.

// Time -----------------------------------------------------------------------

// Windows uses a Gregorian epoch of 1601.  We need to match this internally
// so that our time representations match across all platforms.  See bug 14734.
//   irb(main):010:0> Time.at(0).getutc()
//   => Thu Jan 01 00:00:00 UTC 1970
//   irb(main):011:0> Time.at(-11644473600).getutc()
//   => Mon Jan 01 00:00:00 UTC 1601
static const int64 kWindowsEpochDeltaSeconds = GG_INT64_C(11644473600);
static const int64 kWindowsEpochDeltaMilliseconds =
    kWindowsEpochDeltaSeconds * Time::kMillisecondsPerSecond;

// static
const int64 Time::kWindowsEpochDeltaMicroseconds =
    kWindowsEpochDeltaSeconds * Time::kMicrosecondsPerSecond;

// Some functions in time.cc use time_t directly, so we provide an offset
// to convert from time_t (Unix epoch) and internal (Windows epoch).
// static
const int64 Time::kTimeTToMicrosecondsOffset = kWindowsEpochDeltaMicroseconds;

// static
Time Time::Now() {
  struct timeval tv;
  struct timezone tz = { 0, 0 };  // UTC
  if (gettimeofday(&tv, &tz) != 0) {
    DCHECK(0) << "Could not determine time of day";
    LOG_ERRNO(ERROR) << "Call to gettimeofday failed.";
    // Return null instead of uninitialized |tv| value, which contains random
    // garbage data. This may result in the crash seen in crbug.com/147570.
    return Time();
  }
  // Combine seconds and microseconds in a 64-bit field containing microseconds
  // since the epoch.  That's enough for nearly 600 centuries.  Adjust from
  // Unix (1970) to Windows (1601) epoch.
  return Time((tv.tv_sec * kMicrosecondsPerSecond + tv.tv_usec) +
      kWindowsEpochDeltaMicroseconds);
}

// static
Time Time::NowFromSystemTime() {
  // Just use Now() because Now() returns the system time.
  return Now();
}

void Time::Explode(bool is_local, Exploded* exploded) const {
  // Time stores times with microsecond resolution, but Exploded only carries
  // millisecond resolution, so begin by being lossy.  Adjust from Windows
  // epoch (1601) to Unix epoch (1970);
  int64 microseconds = us_ - kWindowsEpochDeltaMicroseconds;
  // The following values are all rounded towards -infinity.
  int64 milliseconds;  // Milliseconds since epoch.
  SysTime seconds;  // Seconds since epoch.
  int millisecond;  // Exploded millisecond value (0-999).
  if (microseconds >= 0) {
    // Rounding towards -infinity <=> rounding towards 0, in this case.
    milliseconds = microseconds / kMicrosecondsPerMillisecond;
    seconds = milliseconds / kMillisecondsPerSecond;
    millisecond = milliseconds % kMillisecondsPerSecond;
  } else {
    // Round these *down* (towards -infinity).
    milliseconds = (microseconds - kMicrosecondsPerMillisecond + 1) /
                   kMicrosecondsPerMillisecond;
    seconds = (milliseconds - kMillisecondsPerSecond + 1) /
              kMillisecondsPerSecond;
    // Make this nonnegative (and between 0 and 999 inclusive).
    millisecond = milliseconds % kMillisecondsPerSecond;
    if (millisecond < 0)
      millisecond += kMillisecondsPerSecond;
  }

  struct tm timestruct;
  SysTimeToTimeStruct(seconds, &timestruct, is_local);

  exploded->year         = timestruct.tm_year + 1900;
  exploded->month        = timestruct.tm_mon + 1;
  exploded->day_of_week  = timestruct.tm_wday;
  exploded->day_of_month = timestruct.tm_mday;
  exploded->hour         = timestruct.tm_hour;
  exploded->minute       = timestruct.tm_min;
  exploded->second       = timestruct.tm_sec;
  exploded->millisecond  = millisecond;
}

// static
Time Time::FromExploded(bool is_local, const Exploded& exploded) {
  struct tm timestruct;
  timestruct.tm_sec    = exploded.second;
  timestruct.tm_min    = exploded.minute;
  timestruct.tm_hour   = exploded.hour;
  timestruct.tm_mday   = exploded.day_of_month;
  timestruct.tm_mon    = exploded.month - 1;
  timestruct.tm_year   = exploded.year - 1900;
  timestruct.tm_wday   = exploded.day_of_week;  // mktime/timegm ignore this
  timestruct.tm_yday   = 0;     // mktime/timegm ignore this
  timestruct.tm_isdst  = -1;    // attempt to figure it out
#if !defined(OS_NACL) && !defined(OS_SOLARIS)
  timestruct.tm_gmtoff = 0;     // not a POSIX field, so mktime/timegm ignore
  timestruct.tm_zone   = NULL;  // not a POSIX field, so mktime/timegm ignore
#endif

  SysTime seconds = SysTimeFromTimeStruct(&timestruct, is_local);

  int64 milliseconds;
  // Handle overflow.  Clamping the range to what mktime and timegm might
  // return is the best that can be done here.  It's not ideal, but it's better
  // than failing here or ignoring the overflow case and treating each time
  // overflow as one second prior to the epoch.
  if (seconds == -1 &&
      (exploded.year < 1969 || exploded.year > 1970)) {
    // If exploded.year is 1969 or 1970, take -1 as correct, with the
    // time indicating 1 second prior to the epoch.  (1970 is allowed to handle
    // time zone and DST offsets.)  Otherwise, return the most future or past
    // time representable.  Assumes the time_t epoch is 1970-01-01 00:00:00 UTC.
    //
    // The minimum and maximum representible times that mktime and timegm could
    // return are used here instead of values outside that range to allow for
    // proper round-tripping between exploded and counter-type time
    // representations in the presence of possible truncation to time_t by
    // division and use with other functions that accept time_t.
    //
    // When representing the most distant time in the future, add in an extra
    // 999ms to avoid the time being less than any other possible value that
    // this function can return.
    if (exploded.year < 1969) {
      CHECK(sizeof(SysTime) < sizeof(int64)) << "integer overflow";
      milliseconds = std::numeric_limits<SysTime>::min();
      milliseconds *= kMillisecondsPerSecond;
    } else {
      CHECK(sizeof(SysTime) < sizeof(int64)) << "integer overflow";
      milliseconds = std::numeric_limits<SysTime>::max();
      milliseconds *= kMillisecondsPerSecond;
      milliseconds += (kMillisecondsPerSecond - 1);
    }
  } else {
    milliseconds = seconds * kMillisecondsPerSecond + exploded.millisecond;
  }

  // Adjust from Unix (1970) to Windows (1601) epoch.
  return Time((milliseconds * kMicrosecondsPerMillisecond) +
      kWindowsEpochDeltaMicroseconds);
}

// TimeTicks ------------------------------------------------------------------
// static
TimeTicks TimeTicks::Now() {
  return ClockNow(CLOCK_MONOTONIC);
}

// static
TimeTicks TimeTicks::HighResNow() {
  return Now();
}

// static
TimeTicks TimeTicks::ThreadNow() {
#if defined(_POSIX_THREAD_CPUTIME) && (_POSIX_THREAD_CPUTIME >= 0)
  return ClockNow(CLOCK_THREAD_CPUTIME_ID);
#else
  NOTREACHED();
  return TimeTicks();
#endif
}

#if defined(OS_CHROMEOS)
// Force definition of the system trace clock; it is a chromeos-only api
// at the moment and surfacing it in the right place requires mucking
// with glibc et al.
#define CLOCK_SYSTEM_TRACE 11

// static
TimeTicks TimeTicks::NowFromSystemTraceTime() {
  uint64_t absolute_micro;

  struct timespec ts;
  if (clock_gettime(CLOCK_SYSTEM_TRACE, &ts) != 0) {
    // NB: fall-back for a chrome os build running on linux
    return HighResNow();
  }

  absolute_micro =
      (static_cast<int64>(ts.tv_sec) * Time::kMicrosecondsPerSecond) +
      (static_cast<int64>(ts.tv_nsec) / Time::kNanosecondsPerMicrosecond);

  return TimeTicks(absolute_micro);
}

#else // !defined(OS_CHROMEOS)

// static
TimeTicks TimeTicks::NowFromSystemTraceTime() {
  return HighResNow();
}

#endif // defined(OS_CHROMEOS)

#endif  // !OS_MACOSX

// static
Time Time::FromTimeVal(struct timeval t) {
  DCHECK_LT(t.tv_usec, static_cast<int>(Time::kMicrosecondsPerSecond));
  DCHECK_GE(t.tv_usec, 0);
  if (t.tv_usec == 0 && t.tv_sec == 0)
    return Time();
  if (t.tv_usec == static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1 &&
      t.tv_sec == std::numeric_limits<time_t>::max())
    return Max();
  return Time(
      (static_cast<int64>(t.tv_sec) * Time::kMicrosecondsPerSecond) +
      t.tv_usec +
      kTimeTToMicrosecondsOffset);
}

struct timeval Time::ToTimeVal() const {
  struct timeval result;
  if (is_null()) {
    result.tv_sec = 0;
    result.tv_usec = 0;
    return result;
  }
  if (is_max()) {
    result.tv_sec = std::numeric_limits<time_t>::max();
    result.tv_usec = static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1;
    return result;
  }
  int64 us = us_ - kTimeTToMicrosecondsOffset;
  result.tv_sec = us / Time::kMicrosecondsPerSecond;
  result.tv_usec = us % Time::kMicrosecondsPerSecond;
  return result;
}

}  // namespace base
