// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Muxer flags.

#ifndef APP_MUXER_FLAGS_H_
#define APP_MUXER_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_double(clear_lead);
DECLARE_double(segment_duration);
DECLARE_bool(segment_sap_aligned);
DECLARE_double(fragment_duration);
DECLARE_bool(fragment_sap_aligned);
DECLARE_int32(num_subsegments_per_sidx);
DECLARE_string(temp_dir);
DECLARE_bool(mp4_include_pssh_in_stream);
DECLARE_bool(mp4_use_decoding_timestamp_in_timeline);

#endif  // APP_MUXER_FLAGS_H_
