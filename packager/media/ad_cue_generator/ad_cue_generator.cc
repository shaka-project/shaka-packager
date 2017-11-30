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
  if (num_input_streams() != 1 || next_output_stream_index() != 1) {
    return Status(error::INVALID_ARGUMENT,
                  "Expects exactly one input and one output.");
  }
  return Status::OK;
}

Status AdCueGenerator::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo: {
      const uint32_t time_scale = stream_data->stream_info->time_scale();
      Status status = Dispatch(std::move(stream_data));
      if (!status.ok()) {
        return status;
      }
      return DispatchScte35Events(kStreamIndex, time_scale);
    }
    default:
      return Dispatch(std::move(stream_data));
  }
}

Status AdCueGenerator::DispatchScte35Events(size_t stream_index,
                                            uint32_t time_scale) {
  Status status;
  for (const auto& cue_point : ad_cue_generator_params_.cue_points) {
    std::shared_ptr<Scte35Event> scte35_event = std::make_shared<Scte35Event>();
    scte35_event->start_time = cue_point.start_time_in_seconds * time_scale;
    scte35_event->duration = cue_point.duration_in_seconds * time_scale;
    status.Update(DispatchScte35Event(stream_index, std::move(scte35_event)));
    if (!status.ok()) {
      return status;
    }
  }
  return Status::OK;
}

}  // namespace media
}  // namespace shaka
