// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_MP4_SYNC_SAMPLE_ITERATOR_H_
#define MEDIA_MP4_SYNC_SAMPLE_ITERATOR_H_

#include <vector>

#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

/// Sync sample box (STSS) iterator used to iterate through the entries within
/// the compressed table.
class SyncSampleIterator {
 public:
  /// Create a new SyncSampleIterator from sync sample box.
  explicit SyncSampleIterator(const SyncSample& sync_sample);
  ~SyncSampleIterator();

  /// Advance to the next sample.
  /// @return true if not past the last sample, false otherwise.
  bool AdvanceSample();

  /// @return true if the current sample is a sync sample, false otherwise.
  bool IsSyncSample() const;

  /// @return true if @a sample (1-based) is a sync sample, false otherwise.
  bool IsSyncSample(uint32 sample) const;

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
