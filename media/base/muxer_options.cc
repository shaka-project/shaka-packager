// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/base/muxer_options.h"

namespace media {

MuxerOptions::MuxerOptions()
    : single_segment(false),
      segment_duration(0),
      fragment_duration(0),
      segment_sap_aligned(false),
      fragment_sap_aligned(false),
      normalize_presentation_timestamp(false),
      num_subsegments_per_sidx(0) {}
MuxerOptions::~MuxerOptions() {}

}  // namespace media
