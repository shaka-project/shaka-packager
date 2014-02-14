// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implements a wrapper around Sync Sample Box (STSS) to iterate through the
// compressed table.

#ifndef MEDIA_MP4_SYNC_SAMPLE_ITERATOR_H_
#define MEDIA_MP4_SYNC_SAMPLE_ITERATOR_H_

#include <vector>

#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

// Sample to Chunk Box (STSS) Iterator.
class SyncSampleIterator {
 public:
  explicit SyncSampleIterator(const SyncSample& sync_sample);

  // Advance the properties to refer to the next sample. Return status
  // indicating whether the sample is still valid.
  bool AdvanceSample();

  // Return whether the current sample is a sync sample.
  bool IsSyncSample();

  // Return whether sample (1-based) is a sync sample.
  bool IsSyncSample(uint32 sample);

 private:
  uint32 sample_number_;
  const std::vector<uint32>& sync_sample_vector_;
  std::vector<uint32>::const_iterator iterator_;
  bool is_empty_;

  DISALLOW_COPY_AND_ASSIGN(SyncSampleIterator);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_SYNC_SAMPLE_ITERATOR_H_
