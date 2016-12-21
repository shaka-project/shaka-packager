// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_MPD_OPTIONS_H_
#define MPD_BASE_MPD_OPTIONS_H_

#include <string>

namespace shaka {

enum class DashProfile {
  kUnknown,
  kOnDemand,
  kLive,
};

enum class MpdType { kStatic, kDynamic };

/// Defines Mpd Options.
struct MpdOptions {
  DashProfile dash_profile = DashProfile::kOnDemand;
  MpdType mpd_type = MpdType::kStatic;
  double availability_time_offset = 0;
  double minimum_update_period = 0;
  // TODO(tinskip): Set min_buffer_time in unit tests rather than here.
  double min_buffer_time = 2.0;
  double time_shift_buffer_depth = 0;
  double suggested_presentation_delay = 0;
  std::string default_language;
};

}  // namespace shaka

#endif  // MPD_BASE_MPD_OPTIONS_H_
