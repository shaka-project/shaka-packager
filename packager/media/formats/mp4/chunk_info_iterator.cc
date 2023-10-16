// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/chunk_info_iterator.h>

#include <algorithm>
#include <limits>

#include <absl/log/check.h>
#include <absl/log/log.h>

namespace shaka {
namespace media {
namespace mp4 {

ChunkInfoIterator::ChunkInfoIterator(const SampleToChunk& sample_to_chunk)
    : chunk_sample_index_(0),
      current_chunk_(0),
      chunk_info_table_(sample_to_chunk.chunk_info),
      iterator_(chunk_info_table_.begin()) {
  if (iterator_ != chunk_info_table_.end())
    current_chunk_ = iterator_->first_chunk;
}
ChunkInfoIterator::~ChunkInfoIterator() {}

bool ChunkInfoIterator::AdvanceChunk() {
  ++current_chunk_;
  if (iterator_ + 1 != chunk_info_table_.end()) {
    if (current_chunk_ >= (iterator_ + 1)->first_chunk)
      ++iterator_;
  }
  chunk_sample_index_ = 0;
  return true;
}

bool ChunkInfoIterator::AdvanceSample() {
  ++chunk_sample_index_;
  if (chunk_sample_index_ >= iterator_->samples_per_chunk)
    AdvanceChunk();
  return true;
}

bool ChunkInfoIterator::IsValid() const {
  return iterator_ != chunk_info_table_.end() &&
         chunk_sample_index_ < iterator_->samples_per_chunk;
}

uint32_t ChunkInfoIterator::NumSamples(uint32_t start_chunk,
                                       uint32_t end_chunk) const {
  DCHECK_LE(start_chunk, end_chunk);

  uint32_t last_chunk = 0;
  uint32_t num_samples = 0;
  for (std::vector<ChunkInfo>::const_iterator it = chunk_info_table_.begin();
       it != chunk_info_table_.end();
       ++it) {
    last_chunk = (it + 1 == chunk_info_table_.end())
                     ? std::numeric_limits<uint32_t>::max()
                     : (it + 1)->first_chunk - 1;
    if (last_chunk >= start_chunk) {
      num_samples += (std::min(end_chunk, last_chunk) -
                      std::max(start_chunk, it->first_chunk) + 1) *
                     it->samples_per_chunk;
      if (last_chunk >= end_chunk)
        break;
    }
  }
  return num_samples;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
