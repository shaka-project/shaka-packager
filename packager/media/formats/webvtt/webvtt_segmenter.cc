// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_segmenter.h"

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
}

WebVttSegmenter::WebVttSegmenter(uint64_t segment_duration_ms)
    : segment_duration_ms_(segment_duration_ms) {}

Status WebVttSegmenter::InitializeInternal() {
  return Status::OK;
}

Status WebVttSegmenter::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return DispatchStreamInfo(kStreamIndex,
                                std::move(stream_data->stream_info));
    case StreamDataType::kTextSample:
      return OnTextSample(stream_data->text_sample);
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type for this handler");
  }
}

Status WebVttSegmenter::OnFlushRequest(size_t input_stream_index) {
  Status status;
  while (status.ok() && samples_.size()) {
    // It is not possible for there to be any gaps, or else we would have
    // already ended the segments before them. So just close the last remaining
    // open segments.
    OnSegmentEnd(samples_.top().segment);
  }
  return status.ok() ? FlushAllDownstreams() : status;
}

Status WebVttSegmenter::OnTextSample(std::shared_ptr<const TextSample> sample) {
  const uint64_t start_segment = sample->start_time() / segment_duration_ms_;

  // Find the last segment that overlaps the sample. Adjust the sample by one
  // ms (smallest time unit) in case |EndTime| falls on the segment boundary.
  DCHECK_GT(sample->duration(), 0u);
  const uint64_t ending_segment =
      (sample->EndTime() - 1) / segment_duration_ms_;

  DCHECK_GE(ending_segment, start_segment);

  // Samples must always be advancing. If a sample comes in out of order,
  // skip the sample.
  if (samples_.size() && samples_.top().segment > start_segment) {
    LOG(WARNING) << "New sample has arrived out of order. Skipping sample "
                 << "as segment start is " << start_segment << " and segment "
                 << "head is " << samples_.top().segment << ".";
    return Status::OK;
  }

  for (uint64_t segment = start_segment; segment <= ending_segment; segment++) {
    WebVttSegmentedTextSample seg_sample;
    seg_sample.segment = segment;
    seg_sample.sample = sample;

    samples_.push(seg_sample);
  }

  // Output all the segments that come before the start of this cue's first
  // segment.
  for (; current_segment_ < start_segment; current_segment_++) {
    Status status = OnSegmentEnd(current_segment_);
    if (!status.ok()) {
      return status;
    }
  }

  return Status::OK;
}

Status WebVttSegmenter::OnSegmentEnd(uint64_t segment) {
  Status status;
  while (status.ok() && samples_.size() && samples_.top().segment == segment) {
    status.Update(
        DispatchTextSample(kStreamIndex, std::move(samples_.top().sample)));
    samples_.pop();
  }

  // Only send the segment info if all the samples were accepted.
  if (status.ok()) {
    std::shared_ptr<SegmentInfo> info = std::make_shared<SegmentInfo>();
    info->start_timestamp = segment * segment_duration_ms_;
    info->duration = segment_duration_ms_;

    status.Update(DispatchSegmentInfo(kStreamIndex, std::move(info)));
  }

  return status;
}
}  // namespace media
}  // namespace shaka
