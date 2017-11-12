// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/hls_flags.h"

DEFINE_string(hls_master_playlist_output,
              "",
              "Output path for the master playlist for HLS. This flag must be"
              "used to output HLS.");
DEFINE_string(hls_base_url,
              "",
              "The base URL for the Media Playlists and media files listed in "
              "the playlists. This is the prefix for the files.");
DEFINE_string(hls_key_uri,
              "",
              "The key uri for 'identity' and 'com.apple.streamingkeydelivery' "
              "key formats. Ignored if the playlist is not encrypted or not "
              "using the above key formats.");
DEFINE_string(hls_playlist_type,
              "VOD",
              "VOD, EVENT, or LIVE. This defines the EXT-X-PLAYLIST-TYPE in "
              "the HLS specification. For hls_playlist_type of LIVE, "
              "EXT-X-PLAYLIST-TYPE tag is omitted.");
