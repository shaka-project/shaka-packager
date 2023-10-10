// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/composition_offset_iterator.h>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {
namespace mp4 {

CompositionOffsetIterator::CompositionOffsetIterator(
    const CompositionTimeToSample& composition_time_to_sample)
    : sample_index_(0),
      composition_offset_table_(composition_time_to_sample.composition_offset),
      iterator_(composition_offset_table_.begin()) {}
CompositionOffsetIterator::~CompositionOffsetIterator() {}

bool CompositionOffsetIterator::AdvanceSample() {
  ++sample_index_;
  if (sample_index_ >= iterator_->sample_count) {
    ++iterator_;
    if (iterator_ == composition_offset_table_.end())
      return false;
    sample_index_ = 0;
  }
  return true;
}

bool CompositionOffsetIterator::IsValid() const {
  return iterator_ != composition_offset_table_.end() &&
         sample_index_ < iterator_->sample_count;
}

int64_t CompositionOffsetIterator::SampleOffset(uint32_t sample) const {
  uint32_t current_sample = 0;
  std::vector<CompositionOffset>::const_iterator it =
      composition_offset_table_.begin();
  for (; it != composition_offset_table_.end(); ++it) {
    current_sample += it->sample_count;
    if (current_sample >= sample)
      return it->sample_offset;
  }
  DCHECK_LE(sample, current_sample) << " Sample is invalid";
  return 0;
}

uint32_t CompositionOffsetIterator::NumSamples() const {
  uint32_t num_samples = 0;
  std::vector<CompositionOffset>::const_iterator it =
      composition_offset_table_.begin();
  for (; it != composition_offset_table_.end(); ++it) {
    num_samples += it->sample_count;
  }
  return num_samples;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
