// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/mp4/chunk_info_iterator.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"

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

bool ChunkInfoIterator::IsValid() {
  return iterator_ != chunk_info_table_.end()
      && chunk_sample_index_ < iterator_->samples_per_chunk;
}

uint32 ChunkInfoIterator::NumSamples(uint32 start_chunk, uint32 end_chunk) {
  DCHECK(start_chunk <= end_chunk);
  uint32 last_chunk = 0;
  uint32 num_samples = 0;
  for (std::vector<ChunkInfo>::const_iterator it = chunk_info_table_.begin();
      it != chunk_info_table_.end(); ++it) {
    last_chunk = (
        (it + 1 == chunk_info_table_.end()) ?
            std::numeric_limits<uint32>::max() : (it + 1)->first_chunk) - 1;
    if (last_chunk >= start_chunk) {
      num_samples += (std::min(end_chunk, last_chunk)
          - std::max(start_chunk, it->first_chunk) + 1)
          * it->samples_per_chunk;
      if (last_chunk >= end_chunk)
        break;
    }
  }
  return num_samples;
}

}  // namespace mp4
}  // namespace media
