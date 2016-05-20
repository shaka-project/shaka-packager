// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_MPD_OPTIONS_H_
#define MPD_BASE_MPD_OPTIONS_H_

#include <string>

#include "packager/version/version.h"

namespace shaka {

/// Defines Mpd Options.
struct MpdOptions {
  MpdOptions()
      : availability_time_offset(0),
        minimum_update_period(0),
        // TODO(tinskip): Set min_buffer_time in unit tests rather than here.
        min_buffer_time(2.0),
        time_shift_buffer_depth(0),
        suggested_presentation_delay(0),
        packager_version_string(kPackagerVersion) {}

  ~MpdOptions() {};

  double availability_time_offset;
  double minimum_update_period;
  double min_buffer_time;
  double time_shift_buffer_depth;
  double suggested_presentation_delay;
  std::string packager_version_string;
};

}  // namespace shaka

#endif  // MPD_BASE_MPD_OPTIONS_H_

