// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_APP_HLS_FLAGS_H_
#define PACKAGER_APP_HLS_FLAGS_H_

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(std::string, hls_master_playlist_output);
ABSL_DECLARE_FLAG(std::string, hls_base_url);
ABSL_DECLARE_FLAG(std::string, hls_key_uri);
ABSL_DECLARE_FLAG(std::string, hls_playlist_type);
ABSL_DECLARE_FLAG(int32_t, hls_media_sequence_number);

#endif  // PACKAGER_APP_HLS_FLAGS_H_
