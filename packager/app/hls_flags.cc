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
