// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#include "packager/app/mpd_flags.h"

// TODO(kqyang): Rename to generate_static_live_mpd.
DEFINE_bool(generate_static_mpd,
            false,
            "Set to true to generate static mpd. If segment_template is "
            "specified in stream descriptors, shaka-packager generates dynamic "
            "mpd by default; if this flag is enabled, shaka-packager generates "
            "static mpd instead. Note that if segment_template is not "
            "specified, shaka-packager always generates static mpd regardless "
            "of the value of this flag.");
// TODO(rkuroiwa, kqyang): Remove the 'Exclusive' statements once
// --output_media_info can work together with --mpd_output.
DEFINE_bool(output_media_info,
            false,
            "Create a human readable format of MediaInfo. The output file name "
            "will be the name specified by output flag, suffixed with "
            "'.media_info'. Exclusive with --mpd_output.");
DEFINE_string(mpd_output, "",
              "MPD output file name. Exclusive with --output_media_info.");
DEFINE_string(base_urls,
              "",
              "Comma separated BaseURLs for the MPD. The values will be added "
              "as <BaseURL> element(s) immediately under the <MPD> element.");
DEFINE_double(min_buffer_time,
              2.0,
              "Specifies, in seconds, a common duration used in the definition "
              "of the MPD Representation data rate.");
DEFINE_double(minimum_update_period,
              5.0,
              "Indicates to the player how often to refresh the media "
              "presentation description in seconds. This value is used for "
              "dynamic MPD only.");
DEFINE_double(suggested_presentation_delay,
              0.0,
              "Specifies a delay, in seconds, to be added to the media "
              "presentation time. This value is used for dynamic MPD only.");
DEFINE_string(utc_timings,
              "",
              "Comma separated UTCTiming schemeIdUri and value pairs for the "
              "MPD. This value is used for dynamic MPD only.");
DEFINE_bool(generate_dash_if_iop_compliant_mpd,
            true,
            "Try to generate DASH-IF IOP compliant MPD. This is best effort "
            "and does not guarantee compliance.");
DEFINE_bool(
    allow_approximate_segment_timeline,
    false,
    "For live profile only. "
    "If enabled, segments with close duration (i.e. with difference less than "
    "one sample) are considered to have the same duration. This enables MPD "
    "generator to generate less SegmentTimeline entries. If all segments are "
    "of the same duration except the last one, we will do further optimization "
    "to use SegmentTemplate@duration instead and omit SegmentTimeline "
    "completely."
    "Ignored if $Time$ is used in segment template, since $Time$ requires "
    "accurate Segment Timeline.");
