// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MPD_PUBLIC_MPD_PARAMS_H_
#define PACKAGER_MPD_PUBLIC_MPD_PARAMS_H_

#include <string>
#include <vector>

namespace shaka {

/// DASH MPD related parameters.
struct MpdParams {
  /// MPD output file path.
  std::string mpd_output;
  /// BaseURLs for the MPD. The values will be added as <BaseURL> element(s)
  /// under the <MPD> element.
  std::vector<std::string> base_urls;
  /// Set MPD@minBufferTime attribute, which specifies, in seconds, a common
  /// duration used in the definition of the MPD representation data rate. A
  /// client can be assured of having enough data for continous playout
  /// providing playout begins at min_buffer_time after the first bit is
  /// received.
  double min_buffer_time = 2.0;
  /// Generate static MPD for live profile. Note that this flag has no effect
  /// for on-demand profile, in which case static MPD is always used.
  bool generate_static_live_mpd = false;
  /// Set MPD@timeShiftBufferDepth attribute, which is the guaranteed duration
  /// of the time shifting buffer for 'dynamic' media presentations, in seconds.
  double time_shift_buffer_depth = 0;
  /// Set MPD@suggestedPresentationDelay attribute. For 'dynamic' media
  /// presentations, it specifies a delay, in seconds, to be added to the media
  /// presentation time. The attribute is not set if the value is 0; the client
  /// is expected to choose a suitable value in this case.
  static constexpr double kSuggestedPresentationDelayNotSet = 0;
  double suggested_presentation_delay = kSuggestedPresentationDelayNotSet;
  /// Set MPD@minimumUpdatePeriod attribute, which indicates to the player how
  /// often to refresh the MPD in seconds. For dynamic MPD only.
  double minimum_update_period = 0;
  /// The tracks tagged with this language will have <Role ... value=\"main\" />
  /// in the manifest. This allows the player to choose the correct default
  /// language for the content.
  std::string default_language;
  /// Try to generate DASH-IF IOP compliant MPD.
  bool generate_dash_if_iop_compliant_mpd = true;
};

}  // namespace shaka

#endif  // PACKAGER_MPD_PUBLIC_MPD_PARAMS_H_
