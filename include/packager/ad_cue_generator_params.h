// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_AD_CUE_GENERATOR_PARAMS_H_
#define PACKAGER_PUBLIC_AD_CUE_GENERATOR_PARAMS_H_

#include <vector>

namespace shaka {

struct Cuepoint {
  /// Start time of the cuepoint relative to start of the stream.
  double start_time_in_seconds = 0;

  /// Duration of the ad.
  double duration_in_seconds = 0;
};

/// Cuepoint generator related parameters.
struct AdCueGeneratorParams {
  /// List of cuepoints.
  std::vector<Cuepoint> cue_points;
};

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_AD_CUE_GENERATOR_PARAMS_H_
