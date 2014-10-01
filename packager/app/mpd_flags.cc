// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#include "packager/app/mpd_flags.h"

// TODO(rkuroiwa, kqyang): Remove the 'Exclusive' statements once
// --output_media_info can work together with --mpd_output.
DEFINE_bool(output_media_info,
            false,
            "Create a human readable format of MediaInfo. The output file name "
            "will be the name specified by output flag, suffixed with "
            "'.media_info'. Exclusive with --mpd_output.");
DEFINE_string(mpd_output, "",
              "MPD output file name. Exclusive with --output_media_info.");
DEFINE_string(scheme_id_uri,
              "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed",
              "This flag only applies if output_media_info is true. This value "
              "will be set in MediaInfo if the stream is encrypted. "
              "If the stream is encrypted, MPD requires a <ContentProtection> "
              "element which requires the schemeIdUri attribute. "
              "Default value is Widevine PSSH system ID, and it is valid only "
              "for ISO BMFF.");
DEFINE_string(base_urls,
              "",
              "Comma separated BaseURLs for the MPD. The values will be added "
              "as <BaseURL> element(s) immediately under the <MPD> element.");
DEFINE_double(min_buffer_time,
              2.0,
              "Specifies, in seconds, a common duration used in the definition "
              "of the MPD Representation data rate.");
DEFINE_double(availability_time_offset,
              10.0,
              "Offset with respect to the wall clock time for MPD "
              "availabilityStartTime and availabilityEndTime values, in "
              " seconds. This value is used for live profile only.");
DEFINE_double(minimum_update_period,
              5.0,
              "Indicates to the player how often to refresh the media "
              "presentation description in seconds. This value is used for "
              "live profile only.");
DEFINE_double(time_shift_buffer_depth,
              1800.0,
              "Guaranteed duration of the time shifting buffer for dynamic "
              "media presentations, in seconds.");
DEFINE_double(suggested_presentation_delay,
              0.0,
              "Specifies a delay, in seconds, to be added to the media "
              "presentation time. This value is used for live profile only.");
