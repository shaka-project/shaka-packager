// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/muxer_options.h"

namespace edash_packager {
namespace media {

MuxerOptions::MuxerOptions()
    : single_segment(false),
      segment_duration(0),
      fragment_duration(0),
      segment_sap_aligned(false),
      fragment_sap_aligned(false),
      num_subsegments_per_sidx(0),
      bandwidth(0) {}
MuxerOptions::~MuxerOptions() {}

}  // namespace media
}  // namespace edash_packager
