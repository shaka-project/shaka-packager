// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines Mpd flags.

#ifndef APP_MPD_FLAGS_H_
#define APP_MPD_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_bool(output_media_info);
DECLARE_string(mpd_output);
DECLARE_string(scheme_id_uri);
DECLARE_string(base_urls);

#endif  // APP_MPD_FLAGS_H_
