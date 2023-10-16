// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/decoding_time_iterator.h>

#include <algorithm>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {
namespace mp4 {

DecodingTimeIterator::DecodingTimeIterator(
    const DecodingTimeToSample& decoding_time_to_sample)
    : sample_index_(0),
      decoding_time_table_(decoding_time_to_sample.decoding_time),
      iterator_(decoding_time_table_.begin()) {}
DecodingTimeIterator::~DecodingTimeIterator() {}

bool DecodingTimeIterator::AdvanceSample() {
  ++sample_index_;
  if (sample_index_ >= iterator_->sample_count) {
    ++iterator_;
    if (iterator_ == decoding_time_table_.end())
      return false;
    sample_index_ = 0;
  }
  return true;
}

bool DecodingTimeIterator::IsValid() const {
  return iterator_ != decoding_time_table_.end() &&
         sample_index_ < iterator_->sample_count;
}

int64_t DecodingTimeIterator::Duration(uint32_t start_sample,
                                       uint32_t end_sample) const {
  DCHECK_LE(start_sample, end_sample);
  uint32_t current_sample = 0;
  uint32_t prev_sample = 0;
  int64_t duration = 0;
  std::vector<DecodingTime>::const_iterator it = decoding_time_table_.begin();
  for (; it != decoding_time_table_.end(); ++it) {
    current_sample += it->sample_count;
    if (current_sample >= start_sample) {
      duration += (std::min(end_sample, current_sample) -
                   std::max(start_sample, prev_sample + 1) + 1) *
                  it->sample_delta;
      if (current_sample >= end_sample)
        break;
    }
    prev_sample = current_sample;
  }
  return duration;
}

uint32_t DecodingTimeIterator::NumSamples() const {
  uint32_t num_samples = 0;
  std::vector<DecodingTime>::const_iterator it = decoding_time_table_.begin();
  for (; it != decoding_time_table_.end(); ++it) {
    num_samples += it->sample_count;
  }
  return num_samples;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
