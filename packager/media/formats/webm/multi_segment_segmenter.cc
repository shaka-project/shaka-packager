// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/multi_segment_segmenter.h"

#include "packager/media/base/muxer_options.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace shaka {
namespace media {
namespace webm {

MultiSegmentSegmenter::MultiSegmentSegmenter(const MuxerOptions& options)
    : Segmenter(options), num_segment_(0) {}

MultiSegmentSegmenter::~MultiSegmentSegmenter() {}

Status MultiSegmentSegmenter::FinalizeSegment(uint64_t start_timescale,
                                              uint64_t duration_timescale,
                                              bool is_subsegment) {
  CHECK(cluster());
  Status status = Segmenter::FinalizeSegment(start_timescale,
                                             duration_timescale, is_subsegment);
  if (!status.ok())
    return status;
  if (!cluster()->Finalize())
    return Status(error::FILE_FAILURE, "Error finalizing segment.");
  if (!is_subsegment) {
    if (muxer_listener()) {
      const uint64_t size = cluster()->Size();
      muxer_listener()->OnNewSegment(writer_->file()->file_name(),
                                     start_timescale, duration_timescale, size);
    }
    VLOG(1) << "WEBM file '" << writer_->file()->file_name() << "' finalized.";
  }
  return Status::OK;
}

bool MultiSegmentSegmenter::GetInitRangeStartAndEnd(uint64_t* start,
                                                    uint64_t* end) {
  return false;
}

bool MultiSegmentSegmenter::GetIndexRangeStartAndEnd(uint64_t* start,
                                                     uint64_t* end) {
  return false;
}

Status MultiSegmentSegmenter::DoInitialize(std::unique_ptr<MkvWriter> writer) {
  writer_ = std::move(writer);
  return WriteSegmentHeader(0, writer_.get());
}

Status MultiSegmentSegmenter::DoFinalize() {
  return writer_->Close();
}

Status MultiSegmentSegmenter::NewSegment(uint64_t start_timescale,
                                         bool is_subsegment) {
  if (!is_subsegment) {
    // Create a new file for the new segment.
    std::string segment_name =
        GetSegmentName(options().segment_template, start_timescale,
                       num_segment_, options().bandwidth);
    writer_.reset(new MkvWriter);
    Status status = writer_->Open(segment_name);
    if (!status.ok())
      return status;
    num_segment_++;
  }

  uint64_t start_webm_timecode = FromBMFFTimescale(start_timescale);
  return SetCluster(start_webm_timecode, 0, writer_.get());
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
