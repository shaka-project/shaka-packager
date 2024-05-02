// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Muxer flags.

#ifndef APP_MUXER_FLAGS_H_
#define APP_MUXER_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(double, clear_lead);
ABSL_DECLARE_FLAG(double, segment_duration);
ABSL_DECLARE_FLAG(bool, segment_sap_aligned);
ABSL_DECLARE_FLAG(double, fragment_duration);
ABSL_DECLARE_FLAG(bool, fragment_sap_aligned);
ABSL_DECLARE_FLAG(bool, generate_sidx_in_media_segments);
ABSL_DECLARE_FLAG(std::string, temp_dir);
ABSL_DECLARE_FLAG(bool, mp4_include_pssh_in_stream);
ABSL_DECLARE_FLAG(int32_t, transport_stream_timestamp_offset_ms);
ABSL_DECLARE_FLAG(int32_t, default_text_zero_bias_ms);
ABSL_DECLARE_FLAG(int64_t, start_segment_number);

#endif  // APP_MUXER_FLAGS_H_
