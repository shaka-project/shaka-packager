// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#include "app/mpd_flags.h"

DEFINE_bool(output_media_info,
            false,
            "Create a human readable format of MediaInfo. The output file name "
            "will be the name specified by output flag, suffixed with "
            "'.media_info'.");
DEFINE_string(mpd_output, "", "MPD output file name.");
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
