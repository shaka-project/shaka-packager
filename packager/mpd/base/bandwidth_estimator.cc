// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/bandwidth_estimator.h"

#include <cmath>
#include <cstdlib>

#include "packager/base/logging.h"

const int BandwidthEstimator::kUseAllBlocks = 0;

BandwidthEstimator::BandwidthEstimator(int num_blocks)
    : num_blocks_for_estimation_(num_blocks),
      harmonic_mean_denominator_(0.0),
      num_blocks_added_(0) {}
BandwidthEstimator::~BandwidthEstimator() {}

void BandwidthEstimator::AddBlock(uint64_t size, double duration) {
  DCHECK_GT(duration, 0.0);
  DCHECK_GT(size, 0u);

  if (num_blocks_for_estimation_ < 0 &&
      static_cast<int>(history_.size()) >= -1 * num_blocks_for_estimation_) {
    // Short circuiting the case where |num_blocks_for_estimation_| number of
    // blocks have been added already.
    return;
  }

  const int kBitsInByte = 8;
  const double bits_per_second_reciprocal = duration / (kBitsInByte * size);
  harmonic_mean_denominator_ += bits_per_second_reciprocal;
  if (num_blocks_for_estimation_ == kUseAllBlocks) {
    DCHECK_EQ(history_.size(), 0u);
    ++num_blocks_added_;
    return;
  }

  history_.push_back(bits_per_second_reciprocal);
  if (num_blocks_for_estimation_ > 0 &&
      static_cast<int>(history_.size()) > num_blocks_for_estimation_) {
    harmonic_mean_denominator_ -= history_.front();
    history_.pop_front();
  }

  DCHECK_NE(num_blocks_for_estimation_, kUseAllBlocks);
  DCHECK_LE(static_cast<int>(history_.size()), abs(num_blocks_for_estimation_));
  DCHECK_EQ(num_blocks_added_, 0u);
  return;
}

uint64_t BandwidthEstimator::Estimate() const {
  if (harmonic_mean_denominator_ == 0.0)
    return 0;

  const uint64_t num_blocks = num_blocks_for_estimation_ == kUseAllBlocks
                                  ? num_blocks_added_
                                  : history_.size();
  return static_cast<uint64_t>(ceil(num_blocks / harmonic_mean_denominator_));
}
