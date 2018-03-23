// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/chunking/text_chunker.h"

namespace shaka {
namespace media {
namespace {
const size_t kStreamIndex = 0;
}  // namespace

TextChunker::TextChunker(uint64_t segment_duration_ms)
    : segment_duration_ms_(segment_duration_ms) {}

Status TextChunker::InitializeInternal() {
  return Status::OK;
}

Status TextChunker::Process(std::unique_ptr<StreamData> stream_data) {
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

Status TextChunker::OnFlushRequest(size_t input_stream_index) {
  // At this point we know that there is a single series of consecutive
  // segments, all we need to do is run through all of them.
  for (const auto& pair : segment_map_) {
    Status status = DispatchSegmentWithSamples(pair.first, pair.second);

    if (!status.ok()) {
      return status;
    }
  }

  segment_map_.clear();

  return FlushAllDownstreams();
}

Status TextChunker::OnTextSample(std::shared_ptr<const TextSample> sample) {
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

  // Add the sample to each segment it spans.
  for (uint64_t segment = start_segment; segment <= ending_segment; segment++) {
    segment_map_[segment].push_back(sample);
  }

  // Move forward segment-by-segment so that we output empty segments to fill
  // any segments with no cues.
  for (uint64_t segment = head_segment_; segment < start_segment; segment++) {
    auto it = segment_map_.find(segment);

    Status status;
    if (it == segment_map_.end()) {
      const SegmentSamples kNoSamples;
      status.Update(DispatchSegmentWithSamples(segment, kNoSamples));
    } else {
      // We found a segment, output all the samples. Remove it from the map as
      // we should never need to write to it again.
      status.Update(DispatchSegmentWithSamples(segment, it->second));
      segment_map_.erase(it);
    }

    // If we fail to output a single sample, just stop.
    if (!status.ok()) {
      return status;
    }
  }

  // Jump ahead to the start of this segment as we should never have any samples
  // start before |start_segment|.
  head_segment_ = start_segment;

  return Status::OK;
}

Status TextChunker::DispatchSegmentWithSamples(uint64_t segment,
                                               const SegmentSamples& samples) {
  Status status;
  for (const auto& sample : samples) {
    status.Update(DispatchTextSample(kStreamIndex, sample));
  }

  // Only send the segment info if all the samples were successful.
  if (!status.ok()) {
    return status;
  }

  std::shared_ptr<SegmentInfo> info = std::make_shared<SegmentInfo>();
  info->start_timestamp = segment * segment_duration_ms_;
  info->duration = segment_duration_ms_;

  return DispatchSegmentInfo(kStreamIndex, std::move(info));
}
}  // namespace media
}  // namespace shaka
