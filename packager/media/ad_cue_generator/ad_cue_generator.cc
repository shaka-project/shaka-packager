// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/ad_cue_generator/ad_cue_generator.h"

namespace shaka {
namespace media {

namespace {

// The AdCuGenerator only supports single input and single output.
const size_t kStreamIndex = 0;

}  // namespace

AdCueGenerator::AdCueGenerator(
    const AdCueGeneratorParams& ad_cue_generator_params)
    : ad_cue_generator_params_(ad_cue_generator_params) {}

AdCueGenerator::~AdCueGenerator() {}

Status AdCueGenerator::InitializeInternal() {
  return Status::OK;
}

Status AdCueGenerator::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      // TODO(kdevarakonda): dispatch scte35 events.
      return DispatchStreamInfo(kStreamIndex,
                                std::move(stream_data->stream_info));
    default:
      return Dispatch(std::move(stream_data));
  }
}

}  // namespace media
}  // namespace shaka
