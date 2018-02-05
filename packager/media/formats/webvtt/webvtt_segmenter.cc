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
  while (segment_map_.size()) {
    uint64_t segment = segment_map_.begin()->first;

    // OnSegmentEnd will remove the segment from the map.
    Status status = OnSegmentEnd(segment);
    if (!status.ok()) {
      return status;
    }
  }

  return FlushAllDownstreams();
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
  if (head_segment_ > start_segment) {
    LOG(WARNING) << "New sample has arrived out of order. Skipping sample "
                 << "as segment start is " << start_segment << " and segment "
                 << "head is " << head_segment_ << ".";
    return Status::OK;
  }

  // Now that we are accepting this new segment, its starting segment is our
  // new head segment.
  head_segment_ = start_segment;

  // Add the sample to each segment it spans.
  for (uint64_t segment = start_segment; segment <= ending_segment; segment++) {
    segment_map_[segment].push_back(sample);
  }

  // Output all the segments that come before the start of this cue's first
  // segment.
  while (segment_map_.size()) {
    // Since the segments are in accending order, we can break out of the loop
    // once we catch-up to the new samples starting segment.
    const uint64_t segment = segment_map_.begin()->first;
    if (segment >= start_segment) {
      break;
    }

    // Output the segment. If there is an error, there is no reason to continue.
    Status status = OnSegmentEnd(segment);
    if (!status.ok()) {
      return status;
    }
  }

  return Status::OK;
}

Status WebVttSegmenter::OnSegmentEnd(uint64_t segment) {
  Status status;

  const auto found = segment_map_.find(segment);
  if (found != segment_map_.end()) {
    for (const auto& sample : found->second) {
      status.Update(DispatchTextSample(kStreamIndex, sample));
    }

    // Stop storing the segment.
    segment_map_.erase(found);
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
