// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/text_chunker.h"

#include "packager/status_macros.h"

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;

std::shared_ptr<const SegmentInfo> MakeSegmentInfo(int64_t start_ms,
                                                   int64_t end_ms) {
  DCHECK_LT(start_ms, end_ms);

  std::shared_ptr<SegmentInfo> info = std::make_shared<SegmentInfo>();
  info->start_timestamp = start_ms;
  info->duration = end_ms - start_ms;

  return info;
}
}  // namespace

TextChunker::TextChunker(int64_t segment_duration_ms)
    : segment_duration_ms_(segment_duration_ms),
      segment_start_ms_(0),
      segment_expected_end_ms_(segment_duration_ms) {}

Status TextChunker::InitializeInternal() {
  return Status::OK;
}

Status TextChunker::Process(std::unique_ptr<StreamData> data) {
  switch (data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(std::move(data->stream_info));
    case StreamDataType::kTextSample:
      return OnTextSample(data->text_sample);
    case StreamDataType::kCueEvent:
      return OnCueEvent(data->cue_event);
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type for this handler");
  }
}

Status TextChunker::OnFlushRequest(size_t input_stream_index) {
  // Keep outputting segments until all the samples leave the system.
  while (segment_samples_.size()) {
    RETURN_IF_ERROR(EndSegment(segment_expected_end_ms_));
  }

  return FlushAllDownstreams();
}

Status TextChunker::OnStreamInfo(std::shared_ptr<const StreamInfo> info) {
  // There is no information we need from the stream info, so just pass it
  // downstream.
  return DispatchStreamInfo(kStreamIndex, std::move(info));
}

Status TextChunker::OnCueEvent(std::shared_ptr<const CueEvent> event) {
  // We are going to cut the current segment into two using the event's time as
  // the division.
  const int64_t cue_time_in_ms = event->time_in_seconds * 1000;

  // In the case that there is a gap with no samples between the last sample
  // and the cue event, output all the segments until we get to the segment that
  // the cue event interrupts.
  while (segment_expected_end_ms_ < cue_time_in_ms) {
    RETURN_IF_ERROR(EndSegment(segment_expected_end_ms_));
  }

  RETURN_IF_ERROR(EndSegment(cue_time_in_ms));
  RETURN_IF_ERROR(DispatchCueEvent(kStreamIndex, std::move(event)));

  return Status::OK;
}

Status TextChunker::OnTextSample(std::shared_ptr<const TextSample> sample) {
  // Output all segments that come before our new sample.
  while (segment_expected_end_ms_ <= sample->start_time()) {
    RETURN_IF_ERROR(EndSegment(segment_expected_end_ms_));
  }

  segment_samples_.push_back(std::move(sample));

  return Status::OK;
}

Status TextChunker::EndSegment(int64_t segment_actual_end_ms) {
  // Output all the samples that are part of the segment.
  for (const auto& sample : segment_samples_) {
    RETURN_IF_ERROR(DispatchTextSample(kStreamIndex, sample));
  }

  RETURN_IF_ERROR(DispatchSegmentInfo(
      kStreamIndex, MakeSegmentInfo(segment_start_ms_, segment_actual_end_ms)));

  // Create a new segment that comes right after the old segment and remove all
  // samples that don't cross over into the new segment.
  StartNewSegment(segment_actual_end_ms);

  return Status::OK;
}

void TextChunker::StartNewSegment(int64_t start_ms) {
  segment_start_ms_ = start_ms;
  segment_expected_end_ms_ = start_ms + segment_duration_ms_;

  // Remove all samples that no longer overlap with the new segment.
  segment_samples_.remove_if(
      [start_ms](const std::shared_ptr<const TextSample>& sample) {
        return sample->EndTime() <= start_ms;
      });
}

}  // namespace media
}  // namespace shaka
