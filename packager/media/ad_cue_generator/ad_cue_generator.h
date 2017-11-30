// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_AD_CUE_GENERATOR_AD_CUE_GENERATOR_H_
#define PACKAGER_MEDIA_AD_CUE_GENERATOR_AD_CUE_GENERATOR_H_

#include "packager/media/base/media_handler.h"
#include "packager/media/public/ad_cue_generator_params.h"

namespace shaka {
namespace media {

/// AdCueGenerator converts out of band cuepoint markers into SCTE-35 events.
class AdCueGenerator : public MediaHandler {
 public:
  explicit AdCueGenerator(const AdCueGeneratorParams& ad_cue_generator_params);
  ~AdCueGenerator() override;

 private:
  AdCueGenerator(const AdCueGenerator&) = delete;
  AdCueGenerator& operator=(const AdCueGenerator&) = delete;

  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;

  // Dispatches SCTE35 events that are built from AdCueGenerator params.
  Status DispatchScte35Events(size_t stream_index, uint32_t time_scale);

  const AdCueGeneratorParams ad_cue_generator_params_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_AD_CUE_GENERATOR_AD_CUE_GENERATOR_H_
