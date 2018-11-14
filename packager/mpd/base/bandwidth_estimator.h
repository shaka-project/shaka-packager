// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_BANDWIDTH_ESTIMATOR_H_
#define MPD_BASE_BANDWIDTH_ESTIMATOR_H_

#include <stdint.h>

namespace shaka {

class BandwidthEstimator {
 public:
  explicit BandwidthEstimator(double target_segment_duration);
  ~BandwidthEstimator();

  /// @param size is the size of the block in bytes. Should be positive.
  /// @param duration is the length in seconds. Should be positive.
  void AddBlock(uint64_t size_in_bytes, double duration);

  /// @return The estimate bandwidth, in bits per second, calculated from the
  ///         sum of the sizes of every block, divided by the sum of durations
  ///         of every block, of the number of blocks specified in the
  ///         constructor. The value is rounded up to the nearest integer.
  uint64_t Estimate() const;

  /// @return The max bandwidth, in bits per second, of the number of blocks
  ///         specified in the constructor. The value is rounded up to the
  ///         nearest integer.
  uint64_t Max() const;

 private:
  BandwidthEstimator(const BandwidthEstimator&) = delete;
  BandwidthEstimator& operator=(const BandwidthEstimator&) = delete;

  const double target_segment_duration_ = 0;
  uint64_t total_size_in_bits_ = 0;
  double total_duration_ = 0;
  uint64_t max_bitrate_ = 0;
};

}  // namespace shaka

#endif  // MPD_BASE_BANDWIDTH_ESTIMATOR_H_
