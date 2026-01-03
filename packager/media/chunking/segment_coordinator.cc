// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/chunking/segment_coordinator.h>

#include <absl/log/log.h>

#include <packager/macros/status.h>

namespace shaka {
namespace media {

SegmentCoordinator::SegmentCoordinator() = default;

void SegmentCoordinator::MarkAsTeletextStream(size_t input_stream_index) {
  DVLOG(2) << "SegmentCoordinator: Marking stream " << input_stream_index
           << " as teletext";
  teletext_stream_indices_.insert(input_stream_index);
}

Status SegmentCoordinator::InitializeInternal() {
  // This handler accepts all stream types and passes them through.
  // The number of output streams equals the number of input streams.
  return Status::OK;
}

Status SegmentCoordinator::Process(std::unique_ptr<StreamData> stream_data) {
  const size_t input_stream_index = stream_data->stream_index;
  const StreamDataType stream_data_type = stream_data->stream_data_type;

  DVLOG(3) << "SegmentCoordinator::Process stream_index=" << input_stream_index
           << " type=" << StreamDataTypeToString(stream_data_type);

  // Handle SegmentInfo specially - replicate to teletext streams
  if (stream_data_type == StreamDataType::kSegmentInfo) {
    auto info = std::move(stream_data->segment_info);

    // First, dispatch to the same output stream (pass through)
    RETURN_IF_ERROR(DispatchSegmentInfo(input_stream_index, info));

    // If this is from a video/audio stream (not teletext), replicate to
    // teletext streams
    if (!IsTeletextStream(input_stream_index)) {
      RETURN_IF_ERROR(OnSegmentInfo(input_stream_index, std::move(info)));
    }

    return Status::OK;
  }

  // For all other data types, pass through unchanged
  return Dispatch(std::move(stream_data));
}

Status SegmentCoordinator::OnSegmentInfo(
    size_t input_stream_index,
    std::shared_ptr<const SegmentInfo> info) {
  // Only replicate full segments, not subsegments
  if (info->is_subsegment) {
    DVLOG(3) << "SegmentCoordinator: Skipping subsegment replication";
    return Status::OK;
  }

  // Replicate to all teletext streams
  if (teletext_stream_indices_.empty()) {
    DVLOG(3) << "SegmentCoordinator: No teletext streams registered, "
             << "skipping replication";
    return Status::OK;
  }

  // Set the sync source to the first non-teletext stream that sends
  // SegmentInfo. This ensures we only use one stream (typically video) for
  // alignment, avoiding issues when video and audio have different segment
  // boundaries.
  if (!sync_source_stream_index_.has_value()) {
    sync_source_stream_index_ = input_stream_index;
    DVLOG(2) << "SegmentCoordinator: Set sync source to stream "
             << input_stream_index;
  }

  // Only replicate from the sync source stream
  if (input_stream_index != sync_source_stream_index_.value()) {
    DVLOG(3) << "SegmentCoordinator: Ignoring SegmentInfo from stream "
             << input_stream_index << " (sync source is stream "
             << sync_source_stream_index_.value() << ")";
    return Status::OK;
  }

  // Update latest boundary for logging
  latest_segment_boundary_ = info->start_timestamp;

  DVLOG(2)
      << "SegmentCoordinator: Received SegmentInfo from sync source stream "
      << input_stream_index << " boundary=" << info->start_timestamp
      << " duration=" << info->duration
      << " segment_number=" << info->segment_number;

  DVLOG(2) << "SegmentCoordinator: Replicating segment boundary "
           << info->start_timestamp << " to " << teletext_stream_indices_.size()
           << " teletext stream(s)";

  // Replicate SegmentInfo to all teletext stream indices
  for (size_t teletext_stream_index : teletext_stream_indices_) {
    DVLOG(3) << "SegmentCoordinator: Replicating to teletext stream "
             << teletext_stream_index;
    RETURN_IF_ERROR(DispatchSegmentInfo(teletext_stream_index, info));
  }

  return Status::OK;
}

bool SegmentCoordinator::IsTeletextStream(size_t input_stream_index) const {
  return teletext_stream_indices_.count(input_stream_index) > 0;
}

}  // namespace media
}  // namespace shaka
