// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gflags/gflags.h>

DECLARE_string(profile);
DECLARE_bool(single_segment);
DECLARE_bool(webm_subsample_encryption);
DECLARE_double(availability_time_offset);
DECLARE_string(playready_key_id);
DECLARE_string(playready_key);
DECLARE_bool(mp4_use_decoding_timestamp_in_timeline);
DECLARE_int32(num_subsegments_per_sidx);
DECLARE_bool(generate_widevine_pssh);
DECLARE_bool(generate_playready_pssh);
DECLARE_bool(generate_common_pssh);
