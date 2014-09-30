// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_BANDWIDTH_ESTIMATOR_H_
#define MPD_BASE_BANDWIDTH_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <list>

class BandwidthEstimator {
 public:
  /// @param num_blocks is the number of latest blocks to use. Negative values
  ///        use first N blocks. 0 uses all.
  explicit BandwidthEstimator(int num_blocks);
  ~BandwidthEstimator();

  // @param size is the size of the block in bytes. Should be positive.
  // @param duration is the length in seconds. Should be positive.
  void AddBlock(uint64_t size, double duration);

  // @return The estimate bandwidth, in bits per second, from the harmonic mean
  //         of the number of blocks specified in the constructor. The value is
  //         rounded up to the nearest integer.
  uint64_t Estimate() const;

  static const int kUseAllBlocks;

 private:
  const int num_blocks_for_estimation_;
  double harmonic_mean_denominator_;

  // This is not used when num_blocks_for_estimation_ != 0. Therefore it should
  // always be 0 if num_blocks_for_estimation_ != 0.
  size_t num_blocks_added_;
  std::list<double> history_;
};

#endif  // MPD_BASE_BANDWIDTH_ESTIMATOR_H_
