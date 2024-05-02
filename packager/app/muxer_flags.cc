// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Muxer flags.

#include <packager/app/muxer_flags.h>

ABSL_FLAG(double,
          clear_lead,
          5.0f,
          "Clear lead in seconds if encryption is enabled. Note that we do "
          "not support partial segment encryption, so it is rounded up to "
          "full segments. Set it to a value smaller than segment_duration "
          "so only the first segment is in clear since the first segment "
          "could be smaller than segment_duration if there is small "
          "non-zero starting timestamp.");
ABSL_FLAG(double,
          segment_duration,
          6.0f,
          "Segment duration in seconds. If single_segment is specified, "
          "this parameter sets the duration of a subsegment; otherwise, "
          "this parameter sets the duration of a segment. Actual segment "
          "durations may not be exactly as requested.");
ABSL_FLAG(bool,
          segment_sap_aligned,
          true,
          "Force segments to begin with stream access points.");
ABSL_FLAG(double,
          fragment_duration,
          0,
          "Fragment duration in seconds. Should not be larger than "
          "the segment duration. Actual fragment durations may not be "
          "exactly as requested.");
ABSL_FLAG(bool,
          fragment_sap_aligned,
          true,
          "Force fragments to begin with stream access points. This flag "
          "implies segment_sap_aligned.");
ABSL_FLAG(bool,
          generate_sidx_in_media_segments,
          true,
          "Indicates whether to generate 'sidx' box in media segments. Note "
          "that it is required for DASH on-demand profile (not using segment "
          "template).");
ABSL_FLAG(std::string,
          temp_dir,
          "",
          "Specify a directory in which to store temporary (intermediate) "
          " files. Used only if single_segment=true.");
ABSL_FLAG(bool,
          mp4_include_pssh_in_stream,
          true,
          "MP4 only: include pssh in the encrypted stream.");
ABSL_FLAG(int32_t,
          transport_stream_timestamp_offset_ms,
          100,
          "A positive value, in milliseconds, by which output timestamps "
          "are offset to compensate for possible negative timestamps in the "
          "input. For example, timestamps from ISO-BMFF after adjusted by "
          "EditList could be negative. In transport streams, timestamps are "
          "not allowed to be less than zero.");
ABSL_FLAG(
    int32_t,
    default_text_zero_bias_ms,
    0,
    "A positive value, in milliseconds. It is the threshold used to "
    "determine if we should assume that the text stream actually starts "
    "at time zero. If the first sample comes before default_text_zero_bias_ms, "
    "then the start will be padded as the stream is assumed to start at zero. "
    "If the first sample comes after default_text_zero_bias_ms then the start "
    "of the stream will not be padded as we cannot assume the start time of "
    "the stream.");

ABSL_FLAG(int64_t,
          start_segment_number,
          1,
          "Indicates the startNumber in DASH SegmentTemplate and HLS "
          "segment name.");