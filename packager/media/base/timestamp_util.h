// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_TIMESTAMP_UTIL_H_
#define PACKAGER_MEDIA_BASE_TIMESTAMP_UTIL_H_

#include <cstdint>

namespace shaka {
namespace media {

// MPEG-2 TS PTS/DTS are 33-bit values that wrap around at 2^33
constexpr int64_t kPtsWrapAround =
    (1LL << 33);  // 8589934592 (~26.5 hours @ 90kHz)
constexpr int64_t kPtsHalfWrapAround =
    (1LL << 32);  // 4294967296 (~13.25 hours @ 90kHz)

/// Computes the signed difference between two PTS/DTS timestamps, handling
/// wrap-around correctly.
///
/// This function treats timestamps as 33-bit values that wrap around at 2^33.
/// The result is in the range (-2^32, 2^32), where:
/// - Positive result: 'a' is temporally after 'b'
/// - Negative result: 'a' is temporally before 'b'
/// - Zero: 'a' and 'b' are equal
///
/// Examples:
///   SignedPtsDiff(100, 50) = 50           // Normal case
///   SignedPtsDiff(100, 8589934500) = 692  // 'a' wrapped around, is after 'b'
///   SignedPtsDiff(8589934500, 100) = -692 // 'b' wrapped around, 'a' is before
///
/// @param a First timestamp (33-bit PTS/DTS value)
/// @param b Second timestamp (33-bit PTS/DTS value)
/// @return Signed difference indicating temporal order
int64_t SignedPtsDiff(int64_t a, int64_t b);

/// Returns true if timestamp 'a' is temporally before timestamp 'b',
/// handling wrap-around correctly.
///
/// @param a First timestamp (33-bit PTS/DTS value)
/// @param b Second timestamp (33-bit PTS/DTS value)
/// @return true if 'a' occurs before 'b' in time
bool PtsIsBefore(int64_t a, int64_t b);

/// Returns true if timestamp 'a' is temporally before or equal to timestamp
/// 'b', handling wrap-around correctly.
///
/// @param a First timestamp (33-bit PTS/DTS value)
/// @param b Second timestamp (33-bit PTS/DTS value)
/// @return true if 'a' occurs before or at the same time as 'b'
bool PtsIsBeforeOrEqual(int64_t a, int64_t b);

/// Unwraps 33-bit PTS/DTS timestamps to produce monotonically increasing
/// 64-bit timestamps, handling wrap-around transitions.
///
/// This class maintains state across multiple timestamp observations and
/// detects when a wrap-around occurs, automatically adjusting the output
/// to maintain monotonicity.
///
/// Example usage:
///   PtsUnwrapper unwrapper;
///   unwrapper.Unwrap(8589934500);  // Returns 8589934500 (first call)
///   unwrapper.Unwrap(100);          // Returns 8589934692 (detected wrap)
///   unwrapper.Unwrap(200);          // Returns 8589934792 (continues)
///
/// Thread-safety: NOT thread-safe. Each stream should have its own instance.
class PtsUnwrapper {
 public:
  PtsUnwrapper() = default;

  /// Unwraps a wrapped timestamp to a monotonically increasing value.
  ///
  /// @param wrapped_pts 33-bit PTS/DTS value (may wrap around)
  /// @return Unwrapped 64-bit timestamp (monotonically increasing)
  int64_t Unwrap(int64_t wrapped_pts);

  /// Resets the unwrapper state (for stream discontinuities).
  void Reset();

  /// Returns true if the unwrapper has been initialized with at least one
  /// timestamp.
  bool IsInitialized() const { return initialized_; }

 private:
  bool initialized_ = false;
  int64_t last_wrapped_ = 0;
  int64_t unwrapped_offset_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TIMESTAMP_UTIL_H_
