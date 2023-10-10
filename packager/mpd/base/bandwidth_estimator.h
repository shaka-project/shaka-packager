// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_BANDWIDTH_ESTIMATOR_H_
#define MPD_BASE_BANDWIDTH_ESTIMATOR_H_

#include <cstdint>
#include <vector>

namespace shaka {

class BandwidthEstimator {
 public:
  BandwidthEstimator();
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
  ///         nearest integer. Note that small blocks w.r.t.
  ///         |target_block_duration| are not counted.
  uint64_t Max() const;

 private:
  BandwidthEstimator(const BandwidthEstimator&) = delete;
  BandwidthEstimator& operator=(const BandwidthEstimator&) = delete;

  struct Block {
    uint64_t size_in_bits;
    double duration;
  };
  // Return the average block duration of the blocks in |initial_blocks_|.
  double GetAverageBlockDuration() const;
  // Return the bitrate of the block. Note that a bitrate of 0 is returned if
  // the block duration is less than 50% of target block duration.
  uint64_t GetBitrate(const Block& block, double target_block_duration) const;

  std::vector<Block> initial_blocks_;
  // Target block duration will be estimated from the average duration of the
  // initial blocks.
  double target_block_duration_ = 0;

  uint64_t total_size_in_bits_ = 0;
  double total_duration_ = 0;
  uint64_t max_bitrate_ = 0;
};

}  // namespace shaka

#endif  // MPD_BASE_BANDWIDTH_ESTIMATOR_H_
