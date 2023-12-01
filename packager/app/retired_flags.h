// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

ABSL_DECLARE_FLAG(std::string, profile);
ABSL_DECLARE_FLAG(bool, single_segment);
ABSL_DECLARE_FLAG(bool, webm_subsample_encryption);
ABSL_DECLARE_FLAG(double, availability_time_offset);
ABSL_DECLARE_FLAG(std::string, playready_key_id);
ABSL_DECLARE_FLAG(std::string, playready_key);
ABSL_DECLARE_FLAG(bool, mp4_use_decoding_timestamp_in_timeline);
ABSL_DECLARE_FLAG(int32_t, num_subsegments_per_sidx);
ABSL_DECLARE_FLAG(bool, generate_widevine_pssh);
ABSL_DECLARE_FLAG(bool, generate_playready_pssh);
ABSL_DECLARE_FLAG(bool, generate_common_pssh);

namespace shaka {
bool ValidateRetiredFlags();
}
