// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/bandwidth_estimator.h>

#include <algorithm>
#include <cmath>
#include <numeric>

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <packager/macros/logging.h>

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

  const size_t kTargetDurationThreshold = 10;
  if (initial_blocks_.size() < kTargetDurationThreshold) {
    initial_blocks_.push_back({size_in_bits, duration});
    return;
  }

  if (target_block_duration_ == 0) {
    // Use the average duration as the target block duration. It will be used
    // to filter small blocks from bandwidth calculation.
    target_block_duration_ = GetAverageBlockDuration();
    for (const Block& block : initial_blocks_) {
      max_bitrate_ =
          std::max(max_bitrate_, GetBitrate(block, target_block_duration_));
    }
    return;
  }
  max_bitrate_ = std::max(max_bitrate_, GetBitrate({size_in_bits, duration},
                                                   target_block_duration_));
}

uint64_t BandwidthEstimator::Estimate() const {
  if (total_duration_ == 0)
    return 0;
  return static_cast<uint64_t>(ceil(total_size_in_bits_ / total_duration_));
}

uint64_t BandwidthEstimator::Max() const {
  if (max_bitrate_ != 0)
    return max_bitrate_;

  // We don't have the |target_block_duration_| yet. Calculate a target
  // duration from the current available blocks.
  DCHECK(target_block_duration_ == 0);
  const double target_block_duration = GetAverageBlockDuration();

  // Calculate maximum bitrate with the target duration calculated above.
  uint64_t max_bitrate = 0;
  for (const Block& block : initial_blocks_) {
    max_bitrate =
        std::max(max_bitrate, GetBitrate(block, target_block_duration));
  }
  return max_bitrate;
}

double BandwidthEstimator::GetAverageBlockDuration() const {
  if (initial_blocks_.empty())
    return 0.0;
  const double sum =
      std::accumulate(initial_blocks_.begin(), initial_blocks_.end(), 0.0,
                      [](double duration, const Block& block) {
                        return duration + block.duration;
                      });
  return sum / initial_blocks_.size();
}

uint64_t BandwidthEstimator::GetBitrate(const Block& block,
                                        double target_block_duration) const {
  if (block.duration < 0.5 * target_block_duration) {
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
    // We also apply the same exclusion to the bandwidth computation for DASH as
    // the bitrate for the short segment is not a good signal for peak
    // bandwidth.
    // See https://github.com/shaka-project/shaka-packager/issues/498 for
    // details.
    VLOG(1) << "Exclude short segment (duration " << block.duration
            << ", target_duration " << target_block_duration
            << ") in peak bandwidth computation.";
    return 0;
  }
  return static_cast<uint64_t>(ceil(block.size_in_bits / block.duration));
}

}  // namespace shaka
