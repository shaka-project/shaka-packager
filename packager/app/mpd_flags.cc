// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#include <packager/app/mpd_flags.h>

ABSL_FLAG(bool,
          generate_static_live_mpd,
          false,
          "Set to true to generate static mpd. If segment_template is "
          "specified in stream descriptors, shaka-packager generates dynamic "
          "mpd by default; if this flag is enabled, shaka-packager generates "
          "static mpd instead. Note that if segment_template is not "
          "specified, shaka-packager always generates static mpd regardless "
          "of the value of this flag.");
ABSL_FLAG(bool,
          output_media_info,
          false,
          "Create a human readable format of MediaInfo. The output file name "
          "will be the name specified by output flag, suffixed with "
          "'.media_info'.");
ABSL_FLAG(std::string, mpd_output, "", "MPD output file name.");
ABSL_FLAG(std::string,
          base_urls,
          "",
          "Comma separated BaseURLs for the MPD. The values will be added "
          "as <BaseURL> element(s) immediately under the <MPD> element.");
ABSL_FLAG(double,
          min_buffer_time,
          2.0,
          "Specifies, in seconds, a common duration used in the definition "
          "of the MPD Representation data rate.");
ABSL_FLAG(double,
          minimum_update_period,
          5.0,
          "Indicates to the player how often to refresh the media "
          "presentation description in seconds. This value is used for "
          "dynamic MPD only.");
ABSL_FLAG(double,
          suggested_presentation_delay,
          0.0,
          "Specifies a delay, in seconds, to be added to the media "
          "presentation time. This value is used for dynamic MPD only.");
ABSL_FLAG(std::string,
          utc_timings,
          "",
          "Comma separated UTCTiming schemeIdUri and value pairs for the "
          "MPD. This value is used for dynamic MPD only.");
ABSL_FLAG(bool,
          generate_dash_if_iop_compliant_mpd,
          true,
          "Try to generate DASH-IF IOP compliant MPD. This is best effort "
          "and does not guarantee compliance.");
ABSL_FLAG(
    bool,
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
ABSL_FLAG(bool,
          allow_codec_switching,
          false,
          "If enabled, allow adaptive switching between different codecs, "
          "if they have the same language, media type (audio, video etc) and "
          "container type.");
ABSL_FLAG(bool,
          include_mspr_pro_for_playready,
          true,
          "If enabled, PlayReady Object <mspr:pro> will be inserted into "
          "<ContentProtection ...> element alongside with <cenc:pssh> "
          "when using PlayReady protection system.");
ABSL_FLAG(bool,
          dash_force_segment_list,
          false,
          "Uses SegmentList instead of SegmentBase. Use this if the "
          "content is huge and the total number of (sub)segment references "
          "is greater than what the sidx atom allows (65535). Currently "
          "this flag is only supported in DASH ondemand profile.");
ABSL_FLAG(
    bool,
    low_latency_dash_mode,
    false,
    "If enabled, LL-DASH streaming will be used, "
    "reducing overall latency by decoupling latency from segment duration. "
    "Please see "
    // clang-format off
    "https://shaka-project.github.io/shaka-packager/html/tutorials/low_latency.html "
    // clang-format on
    "for more information.");
