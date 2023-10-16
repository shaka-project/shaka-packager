// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_CHUNK_INFO_ITERATOR_H_
#define PACKAGER_MEDIA_FORMATS_MP4_CHUNK_INFO_ITERATOR_H_

#include <cstdint>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/formats/mp4/box_definitions.h>

namespace shaka {
namespace media {
namespace mp4 {

/// Sample to chunk box (STSC) iterator used to iterate through the compressed
/// table by sample/chunk. This class also provides a convenient function to
/// query total number of samples from start_chunk to end_chunk.
class ChunkInfoIterator {
 public:
  /// Create ChunkInfoIterator from sample to chunk box.
  explicit ChunkInfoIterator(const SampleToChunk& sample_to_chunk);
  ~ChunkInfoIterator();

  /// Advance to the next chunk.
  /// @return true if not past the last chunk, false otherwise.
  bool AdvanceChunk();

  /// Advance to the next sample.
  /// @return true if not past the last sample, false otherwise.
  bool AdvanceSample();

  /// @return true if not past the last chunk/sample, false otherwise.
  bool IsValid() const;

  /// @return Current chunk.
  uint32_t current_chunk() const { return current_chunk_; }

  /// @return Samples per chunk for current chunk.
  uint32_t samples_per_chunk() const { return iterator_->samples_per_chunk; }

  /// @return Sample description index for current chunk.
  uint32_t sample_description_index() const {
    return iterator_->sample_description_index;
  }

  /// @return Number of samples from start_chunk to end_chunk, both 1-based,
  ///         inclusive.
  uint32_t NumSamples(uint32_t start_chunk, uint32_t end_chunk) const;

  /// @return The last first_chunk in chunk_info_table.
  uint32_t LastFirstChunk() const {
    return chunk_info_table_.empty() ? 0
                                     : chunk_info_table_.back().first_chunk;
  }

 private:
  uint32_t chunk_sample_index_;
  uint32_t current_chunk_;
  const std::vector<ChunkInfo>& chunk_info_table_;
  std::vector<ChunkInfo>::const_iterator iterator_;

  DISALLOW_COPY_AND_ASSIGN(ChunkInfoIterator);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_CHUNK_INFO_ITERATOR_H_
