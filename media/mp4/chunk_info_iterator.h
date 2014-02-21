// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implements a wrapper around Sample to Chunk Box (STSC) to iterate through
// the compressed table by sample/chunk. This class also provides a convenient
// function to query total number of samples from start_chunk to end_chunk.

#ifndef MEDIA_MP4_CHUNK_INFO_ITERATOR_H_
#define MEDIA_MP4_CHUNK_INFO_ITERATOR_H_

#include <vector>

#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

class ChunkInfoIterator {
 public:
  explicit ChunkInfoIterator(const SampleToChunk& sample_to_chunk);

  // Advance the properties to refer to the next chunk. Return status
  // indicating whether the chunk is still valid.
  bool AdvanceChunk();

  // Advance the properties to refer to the next sample. Return status
  // indicating whether the sample is still valid.
  bool AdvanceSample();

  // Return whether the current chunk is valid.
  bool IsValid() const;

  // Return current chunk.
  uint32 current_chunk() const { return current_chunk_; }

  // Return samples per chunk for current chunk.
  uint32 samples_per_chunk() const { return iterator_->samples_per_chunk; }

  // Return sample description index for current chunk.
  uint32 sample_description_index() const {
    return iterator_->sample_description_index;
  }

  // Return number of samples from start_chunk to end_chunk, both 1-based,
  // inclusive.
  uint32 NumSamples(uint32 start_chunk, uint32 end_chunk) const;

  // Return the last first_chunk in chunk_info_table.
  uint32 LastFirstChunk() const {
    return chunk_info_table_.empty() ? 0
                                     : chunk_info_table_.back().first_chunk;
  }

 private:
  uint32 chunk_sample_index_;
  uint32 current_chunk_;
  const std::vector<ChunkInfo>& chunk_info_table_;
  std::vector<ChunkInfo>::const_iterator iterator_;

  DISALLOW_COPY_AND_ASSIGN(ChunkInfoIterator);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_CHUNK_INFO_ITERATOR_H_
