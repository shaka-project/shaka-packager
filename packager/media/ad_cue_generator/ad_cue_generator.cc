// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/ad_cue_generator/ad_cue_generator.h"

namespace shaka {
namespace media {

AdCueGenerator::AdCueGenerator(
    const AdCueGeneratorParams& ad_cue_generator_params)
    : ad_cue_generator_params_(ad_cue_generator_params) {}

AdCueGenerator::~AdCueGenerator() {}

}  // namespace media
}  // namespace shaka
