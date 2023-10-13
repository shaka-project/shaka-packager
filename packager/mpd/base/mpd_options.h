// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_BASE_MPD_OPTIONS_H_
#define MPD_BASE_MPD_OPTIONS_H_

#include <string>

#include <packager/mpd_params.h>

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
  MpdParams mpd_params;
};

}  // namespace shaka

#endif  // MPD_BASE_MPD_OPTIONS_H_
