// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/sync_sample_iterator.h"

#include <algorithm>

namespace shaka {
namespace media {
namespace mp4 {

SyncSampleIterator::SyncSampleIterator(const SyncSample& sync_sample)
    : sample_number_(1),
      sync_sample_vector_(sync_sample.sample_number),
      iterator_(sync_sample_vector_.begin()),
      is_empty_(iterator_ == sync_sample_vector_.end()) {}
SyncSampleIterator::~SyncSampleIterator() {}

bool SyncSampleIterator::AdvanceSample() {
  if (iterator_ != sync_sample_vector_.end() && sample_number_ == *iterator_)
    ++iterator_;
  ++sample_number_;
  return true;
}

bool SyncSampleIterator::IsSyncSample() const {
  // If the sync sample box is not present, every sample is a sync sample.
  if (is_empty_)
    return true;
  return iterator_ != sync_sample_vector_.end() && sample_number_ == *iterator_;
}

bool SyncSampleIterator::IsSyncSample(uint32_t sample) const {
  // If the sync sample box is not present, every sample is a sync sample.
  if (is_empty_)
    return true;
  return std::binary_search(
      sync_sample_vector_.begin(), sync_sample_vector_.end(), sample);
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
