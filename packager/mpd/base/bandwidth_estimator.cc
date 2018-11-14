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

BandwidthEstimator::BandwidthEstimator(double target_segment_duration)
    : target_segment_duration_(target_segment_duration) {}

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

  if (duration < 0.5 * target_segment_duration_) {
    // https://tools.ietf.org/html/rfc8216#section-4.1
    // The peak segment bit rate of a Media Playlist is the largest bit rate of
    // any continuous set of segments whose total duration is between 0.5
    // and 1.5 times the target duration.
    // Only the short segments are excluded here as our media playlist generator
    // sets the target duration in the playlist to the largest segment duration.
    // So although the segment duration could be 1.5 times the user provided
    // segment duration, it will never be larger than the actual target
    // duration.
    //
    // TODO(kqyang): Review if we can just stick to the user provided segment
    // duration as our target duration.
    //
    // We also apply the same exclusion to the bandwidth computation for DASH as
    // the bitrate for the short segment is not a good signal for peak
    // bandwidth.
    // See https://github.com/google/shaka-packager/issues/498 for details.
    VLOG(1) << "Exclude short segment (duration " << duration
            << ", target_duration " << target_segment_duration_
            << ") in peak bandwidth computation.";
    return;
  }
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
