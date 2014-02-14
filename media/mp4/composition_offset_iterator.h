// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implements a wrapper around Composition Time to Sample Box (CTTS) to iterate
// through the compressed table. This class also provides convenient functions
// to query total number of samples and the composition offset for a particular
// sample.

#ifndef MEDIA_MP4_COMPOSITION_OFFSET_ITERATOR_H_
#define MEDIA_MP4_COMPOSITION_OFFSET_ITERATOR_H_

#include <vector>

#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

class CompositionOffsetIterator {
 public:
  explicit CompositionOffsetIterator(
      const CompositionTimeToSample& composition_time_to_sample);

  // Advance the properties to refer to the next sample. Return status
  // indicating whether the sample is still valid.
  bool AdvanceSample();

  // Return whether the current sample is valid.
  bool IsValid();

  // Return sample offset for current sample.
  uint32 sample_offset() {
    return iterator_->sample_offset;
  }

  // Return sample offset @ sample, 1-based.
  uint32 SampleOffset(uint32 sample);

  // Return total number of samples.
  uint32 NumSamples();

 private:
  uint32 sample_index_;
  const std::vector<CompositionOffset>& composition_offset_table_;
  std::vector<CompositionOffset>::const_iterator iterator_;

  DISALLOW_COPY_AND_ASSIGN(CompositionOffsetIterator);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_COMPOSITION_OFFSET_ITERATOR_H_
