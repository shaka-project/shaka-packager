// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/bandwidth_estimator.h"

#include <algorithm>
#include <cmath>

#include "packager/base/logging.h"

namespace shaka {

BandwidthEstimator::BandwidthEstimator() = default;
BandwidthEstimator::~BandwidthEstimator() = default;

void BandwidthEstimator::AddBlock(uint64_t size_in_bytes, double duration) {
  if (size_in_bytes == 0 || duration == 0) {
    LOG(WARNING) << "Ignore block with size=" << size_in_bytes
                 << ", duration=" << duration;
    return;
  }

  const int kBitsInByte = 8;
  const uint64_t size_in_bits = size_in_bytes * kBitsInByte;
  total_size_in_bits_ += size_in_bits;

  total_duration_ += duration;

  const uint64_t bitrate = static_cast<uint64_t>(ceil(size_in_bits / duration));
  max_bitrate_ = std::max(bitrate, max_bitrate_);
}

uint64_t BandwidthEstimator::Estimate() const {
  if (total_duration_ == 0)
    return 0;
  return static_cast<uint64_t>(ceil(total_size_in_bits_ / total_duration_));
}

uint64_t BandwidthEstimator::Max() const {
  return max_bitrate_;
}

}  // namespace shaka
