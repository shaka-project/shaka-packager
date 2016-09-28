// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#ifndef APP_MPD_FLAGS_H_
#define APP_MPD_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_bool(output_media_info);
DECLARE_string(mpd_output);
DECLARE_string(base_urls);
DECLARE_double(availability_time_offset);
DECLARE_double(minimum_update_period);
DECLARE_double(min_buffer_time);
DECLARE_double(time_shift_buffer_depth);
DECLARE_double(suggested_presentation_delay);
DECLARE_string(default_language);
DECLARE_bool(generate_dash_if_iop_compliant_mpd);

#endif  // APP_MPD_FLAGS_H_
