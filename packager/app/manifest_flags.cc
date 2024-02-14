// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/app/manifest_flags.h>

ABSL_FLAG(double,
          time_shift_buffer_depth,
          1800.0,
          "Guaranteed duration of the time shifting buffer for HLS LIVE "
          "playlists and DASH dynamic media presentations, in seconds.");
ABSL_FLAG(
    uint64_t,
    preserved_segments_outside_live_window,
    50,
    "Segments outside the live window (defined by '--time_shift_buffer_depth') "
    "are automatically removed except for the most recent X segments defined "
    "by this parameter. This is needed to accommodate latencies in various "
    "stages of content serving pipeline, so that the segments stay accessible "
    "as they may still be accessed by the player."
    "The segments are not removed if the value is zero.");
ABSL_FLAG(std::string,
          default_language,
          "",
          "For DASH, any audio/text tracks tagged with this language will "
          "have <Role ... value=\"main\" /> in the manifest; For HLS, the "
          "first audio/text rendition in a group tagged with this language "
          "will have 'DEFAULT' attribute set to 'YES'. This allows the "
          "player to choose the correct default language for the content."
          "This applies to both audio and text tracks. The default "
          "language for text tracks can be overriden by "
          "'--default_text_language'.");
ABSL_FLAG(std::string,
          default_text_language,
          "",
          "Same as above, but this applies to text tracks only, and "
          "overrides the default language for text tracks.");
ABSL_FLAG(bool,
          force_cl_index,
          true,
          "True forces the muxer to order streams in the order given "
          "on the command-line. False uses the previous unordered behavior.");
