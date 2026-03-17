// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/timestamp_util.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {

int64_t SignedPtsDiff(int64_t a, int64_t b) {
  // Compute difference in 33-bit space (modulo 2^33)
  int64_t diff = (a - b) & (kPtsWrapAround - 1);

  // Convert to signed range: (-2^32, 2^32)
  // If diff >= 2^32, it represents a negative value in 33-bit two's complement
  if (diff >= kPtsHalfWrapAround) {
    diff -= kPtsWrapAround;
  }

  return diff;
}

bool PtsIsBefore(int64_t a, int64_t b) {
  return SignedPtsDiff(a, b) < 0;
}

bool PtsIsBeforeOrEqual(int64_t a, int64_t b) {
  return SignedPtsDiff(a, b) <= 0;
}

int64_t PtsUnwrapper::Unwrap(int64_t wrapped_pts) {
  // Mask to 33-bit range in case input exceeds limit
  // (Some muxers may produce values > 2^33)
  wrapped_pts = wrapped_pts & (kPtsWrapAround - 1);

  DCHECK_GE(wrapped_pts, 0);
  DCHECK_LT(wrapped_pts, kPtsWrapAround);

  if (!initialized_) {
    // First timestamp - use as-is
    last_wrapped_ = wrapped_pts;
    initialized_ = true;
    DVLOG(3) << "PtsUnwrapper: Initialized with PTS " << wrapped_pts;
    return wrapped_pts;
  }

  // Compute signed difference from last timestamp
  int64_t diff = SignedPtsDiff(wrapped_pts, last_wrapped_);

  // Detect wrap-around: If we see a large negative jump (more than half the
  // wrap range), it means we wrapped forward from a high value to a low value
  // Example: last=8589934500, current=100 => diff=-8589934400
  // But in 33-bit signed space: diff = 192 (wrapped forward)
  //
  // Note: SignedPtsDiff already handles this correctly, so if diff is negative,
  // it means we genuinely went backward in time (e.g., stream discontinuity)

  if (diff < 0) {
    DVLOG(2) << "PtsUnwrapper: Detected backward jump from " << last_wrapped_
             << " to " << wrapped_pts << " (diff=" << diff << ")";
    // This shouldn't happen in normal operation (timestamps should be
    // monotonic) But handle it gracefully by not adjusting offset
  }

  // Check for wrap-around: last_wrapped > current, but diff is positive
  // This happens when: last=8589934500, current=100 => diff=692
  if (wrapped_pts < last_wrapped_ && diff > 0) {
    // Wrapped around - add 2^33 to offset
    unwrapped_offset_ += kPtsWrapAround;
    DVLOG(2) << "PtsUnwrapper: Detected wrap-around from " << last_wrapped_
             << " to " << wrapped_pts << ", new offset=" << unwrapped_offset_;
  }

  last_wrapped_ = wrapped_pts;
  int64_t unwrapped = wrapped_pts + unwrapped_offset_;

  DVLOG(3) << "PtsUnwrapper: Unwrap(" << wrapped_pts << ") = " << unwrapped
           << " (offset=" << unwrapped_offset_ << ")";

  return unwrapped;
}

void PtsUnwrapper::Reset() {
  initialized_ = false;
  last_wrapped_ = 0;
  unwrapped_offset_ = 0;
  DVLOG(2) << "PtsUnwrapper: Reset";
}

}  // namespace media
}  // namespace shaka
