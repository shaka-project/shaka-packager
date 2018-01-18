// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/manifest_flags.h"

DEFINE_double(time_shift_buffer_depth,
              1800.0,
              "Guaranteed duration of the time shifting buffer for HLS LIVE "
              "playlists and DASH dynamic media presentations, in seconds.");
DEFINE_string(default_language,
              "",
              "For DASH, any audio/text tracks tagged with this language will "
              "have <Role ... value=\"main\" /> in the manifest; For HLS, the "
              "first audio/text rendition in a group tagged with this language "
              "will have 'DEFAULT' attribute set to 'YES'. This allows the "
              "player to choose the correct default language for the content.");
