// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/chunking_handler.h"

#include <algorithm>

#include "packager/base/logging.h"
#include "packager/media/base/media_sample.h"

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
}  // namespace

ChunkingHandler::ChunkingHandler(const ChunkingParams& chunking_params)
    : chunking_params_(chunking_params) {
  CHECK_NE(chunking_params.segment_duration_in_seconds, 0u);
}

Status ChunkingHandler::InitializeInternal() {
  if (num_input_streams() != 1 || next_output_stream_index() != 1) {
    return Status(error::INVALID_ARGUMENT,
                  "Expects exactly one input and one output.");
  }
  return Status::OK;
}

Status ChunkingHandler::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(stream_data->stream_info));
    case StreamDataType::kCueEvent:
      return OnCueEvent(std::move(stream_data->cue_event));
    case StreamDataType::kSegmentInfo:
      VLOG(3) << "Droppping existing segment info.";
      return Status::OK;
    case StreamDataType::kMediaSample:
      return OnMediaSample(std::move(stream_data->media_sample));
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      return Dispatch(std::move(stream_data));
  }
}

Status ChunkingHandler::OnFlushRequest(size_t input_stream_index) {
  Status status = EndSegmentIfStarted();
  if (!status.ok())
    return status;
  return FlushDownstream(kStreamIndex);
}

Status ChunkingHandler::OnStreamInfo(std::shared_ptr<const StreamInfo> info) {
  time_scale_ = info->time_scale();
  segment_duration_ =
      chunking_params_.segment_duration_in_seconds * time_scale_;
  subsegment_duration_ =
      chunking_params_.subsegment_duration_in_seconds * time_scale_;
  return DispatchStreamInfo(kStreamIndex, std::move(info));
}

Status ChunkingHandler::OnCueEvent(std::shared_ptr<const CueEvent> event) {
  Status status = EndSegmentIfStarted();
  if (!status.ok())
    return status;
  // Force start new segment after cue event.
  segment_start_time_ = base::nullopt;
  return DispatchCueEvent(kStreamIndex, std::move(event));
}

Status ChunkingHandler::OnMediaSample(
    std::shared_ptr<const MediaSample> sample) {
  DCHECK_NE(time_scale_, 0u) << "kStreamInfo should arrive before kMediaSample";

  const int64_t timestamp = sample->dts();

  bool started_new_segment = false;
  const bool can_start_new_segment =
      sample->is_key_frame() || !chunking_params_.segment_sap_aligned;
  if (can_start_new_segment) {
    const int64_t segment_index = timestamp / segment_duration_;
    if (!segment_start_time_ || segment_index != current_segment_index_) {
      current_segment_index_ = segment_index;
      // Reset subsegment index.
      current_subsegment_index_ = 0;

      Status status = EndSegmentIfStarted();
      if (!status.ok())
        return status;
      segment_start_time_ = timestamp;
      subsegment_start_time_ = timestamp;
      started_new_segment = true;
    }
  }
  if (!started_new_segment && IsSubsegmentEnabled()) {
    const bool can_start_new_subsegment =
        sample->is_key_frame() || !chunking_params_.subsegment_sap_aligned;
    if (can_start_new_subsegment) {
      const int64_t subsegment_index =
          (timestamp - segment_start_time_.value()) / subsegment_duration_;
      if (subsegment_index != current_subsegment_index_) {
        current_subsegment_index_ = subsegment_index;

        Status status = EndSubsegmentIfStarted();
        if (!status.ok())
          return status;
        subsegment_start_time_ = timestamp;
      }
    }
  }

  VLOG(3) << "Sample ts: " << timestamp << " "
          << " duration: " << sample->duration() << " scale: " << time_scale_
          << (segment_start_time_ ? " dispatch " : " discard ");
  // Discard samples before segment start. If the segment has started,
  // |segment_start_time_| won't be null.
  if (!segment_start_time_)
    return Status::OK;
  last_sample_end_timestamp_ = timestamp + sample->duration();
  return DispatchMediaSample(kStreamIndex, std::move(sample));
}

Status ChunkingHandler::EndSegmentIfStarted() const {
  if (!segment_start_time_)
    return Status::OK;

  auto segment_info = std::make_shared<SegmentInfo>();
  segment_info->start_timestamp = segment_start_time_.value();
  segment_info->duration =
      last_sample_end_timestamp_ - segment_start_time_.value();
  return DispatchSegmentInfo(kStreamIndex, std::move(segment_info));
}

Status ChunkingHandler::EndSubsegmentIfStarted() const {
  if (!subsegment_start_time_)
    return Status::OK;

  auto subsegment_info = std::make_shared<SegmentInfo>();
  subsegment_info->start_timestamp = subsegment_start_time_.value();
  subsegment_info->duration =
      last_sample_end_timestamp_ - subsegment_start_time_.value();
  subsegment_info->is_subsegment = true;
  return DispatchSegmentInfo(kStreamIndex, std::move(subsegment_info));
}

}  // namespace media
}  // namespace shaka
