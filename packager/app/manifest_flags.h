// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Common flags applicable to both DASH and HLS.

#ifndef PACKAGER_APP_MANIFEST_FLAGS_H_
#define PACKAGER_APP_MANIFEST_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_double(time_shift_buffer_depth);
DECLARE_uint64(preserved_segments_outside_live_window);
DECLARE_string(default_language);
DECLARE_string(default_text_language);

#endif  // PACKAGER_APP_MANIFEST_FLAGS_H_
