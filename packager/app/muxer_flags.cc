// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Muxer flags.

#include "packager/app/muxer_flags.h"

DEFINE_double(clear_lead,
              10.0f,
              "Clear lead in seconds if encryption is enabled.");
DEFINE_double(segment_duration,
              10.0f,
              "Segment duration in seconds. If single_segment is specified, "
              "this parameter sets the duration of a subsegment; otherwise, "
              "this parameter sets the duration of a segment. Actual segment "
              "durations may not be exactly as requested.");
DEFINE_bool(segment_sap_aligned,
            true,
            "Force segments to begin with stream access points.");
DEFINE_double(fragment_duration,
              10.0f,
              "Fragment duration in seconds. Should not be larger than "
              "the segment duration. Actual fragment durations may not be "
              "exactly as requested.");
DEFINE_bool(fragment_sap_aligned,
            true,
            "Force fragments to begin with stream access points. This flag "
            "implies segment_sap_aligned.");
DEFINE_int32(num_subsegments_per_sidx,
             1,
             "For ISO BMFF only. Set the number of subsegments in each "
             "SIDX box. If 0, a single SIDX box is used per segment; if "
             "-1, no SIDX box is used; Otherwise, the muxer packs N "
             "subsegments in the root SIDX of the segment, with "
             "segment_duration/N/fragment_duration fragments per "
             "subsegment.");
DEFINE_string(temp_dir,
              "",
              "Specify a directory in which to store temporary (intermediate) "
              " files. Used only if single_segment=true.");
DEFINE_bool(mp4_include_pssh_in_stream,
            true,
            "MP4 only: include pssh in the encrypted stream.");
DEFINE_bool(mp4_use_decoding_timestamp_in_timeline,
            false,
            "If set, decoding timestamp instead of presentation timestamp will "
            "be used when generating media timeline, e.g. timestamps in sidx "
            "and mpd. This is to workaround a Chromium bug that decoding "
            "timestamp is used in buffered range, https://crbug.com/398130.");
