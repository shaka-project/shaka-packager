// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/mock_muxer_listener.h"

namespace shaka {
namespace media {

MockMuxerListener::MockMuxerListener() {}
MockMuxerListener::~MockMuxerListener() {}

void MockMuxerListener::OnMediaEnd(const MediaRanges& range,
                                   float duration_seconds) {
  const bool has_init_range = static_cast<bool>(range.init_range);
  Range init_range = {};
  if (has_init_range) {
    init_range = range.init_range.value();
  }
  const bool has_index_range = static_cast<bool>(range.index_range);
  Range index_range = {};
  if (has_index_range) {
    index_range = range.index_range.value();
  }

  OnMediaEndMock(has_init_range, init_range.start, init_range.end,
                 has_index_range, index_range.start, index_range.end,
                 !range.subsegment_ranges.empty(), range.subsegment_ranges,
                 duration_seconds);
}

}  // namespace media
}  // namespace shaka
