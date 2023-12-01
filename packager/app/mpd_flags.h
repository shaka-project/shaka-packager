// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#ifndef APP_MPD_FLAGS_H_
#define APP_MPD_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(bool, generate_static_live_mpd);
ABSL_DECLARE_FLAG(bool, output_media_info);
ABSL_DECLARE_FLAG(std::string, mpd_output);
ABSL_DECLARE_FLAG(std::string, base_urls);
ABSL_DECLARE_FLAG(double, minimum_update_period);
ABSL_DECLARE_FLAG(double, min_buffer_time);
ABSL_DECLARE_FLAG(double, suggested_presentation_delay);
ABSL_DECLARE_FLAG(std::string, utc_timings);
ABSL_DECLARE_FLAG(bool, generate_dash_if_iop_compliant_mpd);
ABSL_DECLARE_FLAG(bool, allow_approximate_segment_timeline);
ABSL_DECLARE_FLAG(bool, allow_codec_switching);
ABSL_DECLARE_FLAG(bool, include_mspr_pro_for_playready);
ABSL_DECLARE_FLAG(bool, dash_force_segment_list);
ABSL_DECLARE_FLAG(bool, low_latency_dash_mode);

#endif  // APP_MPD_FLAGS_H_
