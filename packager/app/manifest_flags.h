// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Common flags applicable to both DASH and HLS.

#ifndef PACKAGER_APP_MANIFEST_FLAGS_H_
#define PACKAGER_APP_MANIFEST_FLAGS_H_

#include <cstdint>

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(double, time_shift_buffer_depth);
ABSL_DECLARE_FLAG(uint64_t, preserved_segments_outside_live_window);
ABSL_DECLARE_FLAG(std::string, default_language);
ABSL_DECLARE_FLAG(std::string, default_text_language);
ABSL_DECLARE_FLAG(bool, force_cl_index);

#endif  // PACKAGER_APP_MANIFEST_FLAGS_H_
