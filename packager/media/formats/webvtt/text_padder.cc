// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webvtt/text_padder.h>

#include <algorithm>

#include <absl/log/check.h>

#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace {
const uint64_t kStreamIndex = 0;
}  // namespace

TextPadder::TextPadder(int64_t zero_start_bias_ms)
    : zero_start_bias_ms_(zero_start_bias_ms) {}

Status TextPadder::InitializeInternal() {
  return Status::OK;
}

Status TextPadder::Process(std::unique_ptr<StreamData> data) {
  DCHECK_EQ(data->stream_index, kStreamIndex);
  const bool is_text_sample =
      data->stream_data_type == StreamDataType::kTextSample;
  return is_text_sample ? OnTextSample(std::move(data))
                        : Dispatch(std::move(data));
}

Status TextPadder::OnTextSample(std::unique_ptr<StreamData> data) {
  const TextSample& sample = *data->text_sample;

  // If this is the first sample we have seen, we need to check if we should
  // start at time zero.
  if (max_end_time_ms_ < 0) {
    max_end_time_ms_ =
        sample.start_time() > zero_start_bias_ms_ ? sample.start_time() : 0;
  }

  // Check if there will be a gap between samples if we just dispatch this
  // sample right away. If there will be one, create an empty sample that will
  // fill in that gap.
  if (sample.start_time() > max_end_time_ms_) {
    const std::string kNoId = "";
    auto filler = std::make_shared<TextSample>(kNoId, max_end_time_ms_,
                                               sample.start_time(),
                                               TextSettings{}, TextFragment{});
    RETURN_IF_ERROR(
        MediaHandler::DispatchTextSample(kStreamIndex, std::move(filler)));
  }

  max_end_time_ms_ = std::max(max_end_time_ms_, sample.EndTime());
  return Dispatch(std::move(data));
}
}  // namespace media
}  // namespace shaka
