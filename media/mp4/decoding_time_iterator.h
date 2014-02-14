// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implements a wrapper around Decoding Time to Sample Box (STTS) to iterate
// through the compressed table. This class also provides convenient functions
// to query total number of samples and the duration from start_sample to
// end_sample.

#ifndef MEDIA_MP4_DECODING_TIME_ITERATOR_H_
#define MEDIA_MP4_DECODING_TIME_ITERATOR_H_

#include <vector>

#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

class DecodingTimeIterator {
 public:
  explicit DecodingTimeIterator(
      const DecodingTimeToSample& decoding_time_to_sample);

  // Advance the properties to refer to the next sample. Return status
  // indicating whether the sample is still valid.
  bool AdvanceSample();

  // Return whether the current sample is valid.
  bool IsValid();

  // Return sample delta for current sample.
  uint32 sample_delta() {
    return iterator_->sample_delta;
  }

  // Return duration from start_sample to end_sample, both 1-based, inclusive.
  uint64 Duration(uint32 start_sample, uint32 end_sample);

  // Return total number of samples in the table.
  uint32 NumSamples();

 private:
  uint32 sample_index_;
  const std::vector<DecodingTime>& decoding_time_table_;
  std::vector<DecodingTime>::const_iterator iterator_;

  DISALLOW_COPY_AND_ASSIGN(DecodingTimeIterator);
};

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_MP4_DECODING_TIME_ITERATOR_H_
