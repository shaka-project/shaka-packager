// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/single_segment_segmenter.h"

#include "packager/media/base/muxer_options.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"

namespace shaka {
namespace media {
namespace webm {

SingleSegmentSegmenter::SingleSegmentSegmenter(const MuxerOptions& options)
    : Segmenter(options), init_end_(0), index_start_(0) {}

SingleSegmentSegmenter::~SingleSegmentSegmenter() {}

Status SingleSegmentSegmenter::FinalizeSegment(uint64_t start_timescale,
                                               uint64_t duration_timescale,
                                               bool is_subsegment) {
  Status status = Segmenter::FinalizeSegment(start_timescale,
                                             duration_timescale, is_subsegment);
  if (!status.ok())
    return status;
  // No-op for subsegment in single segment mode.
  if (is_subsegment)
    return Status::OK;
  CHECK(cluster());
  if (!cluster()->Finalize())
    return Status(error::FILE_FAILURE, "Error finalizing cluster.");
  return Status::OK;
}

bool SingleSegmentSegmenter::GetInitRangeStartAndEnd(uint64_t* start,
                                                     uint64_t* end) {
  // The init range is the header, from the start of the file to the size of
  // the header.
  *start = 0;
  *end = init_end_;
  return true;
}

bool SingleSegmentSegmenter::GetIndexRangeStartAndEnd(uint64_t* start,
                                                      uint64_t* end) {
  // The index is the Cues element, which is always placed at the end of the
  // file.
  *start = index_start_;
  *end = index_end_;
  return true;
}

Status SingleSegmentSegmenter::DoInitialize() {
  if (!writer_) {
    std::unique_ptr<MkvWriter> writer(new MkvWriter);
    Status status = writer->Open(options().output_file_name);
    if (!status.ok())
      return status;
    writer_ = std::move(writer);
  }

  Status ret = WriteSegmentHeader(0, writer_.get());
  init_end_ = writer_->Position() - 1;
  seek_head()->set_cluster_pos(init_end_ + 1 - segment_payload_pos());
  return ret;
}

Status SingleSegmentSegmenter::DoFinalize() {
  // Write the Cues to the end of the file.
  index_start_ = writer_->Position();
  seek_head()->set_cues_pos(index_start_ - segment_payload_pos());
  if (!cues()->Write(writer_.get()))
    return Status(error::FILE_FAILURE, "Error writing Cues data.");

  // The WebM index is at the end of the file.
  index_end_ = writer_->Position() - 1;
  writer_->Position(0);

  Status status = WriteSegmentHeader(index_end_ + 1, writer_.get());
  status.Update(writer_->Close());
  return status;
}

Status SingleSegmentSegmenter::NewSegment(uint64_t start_timescale,
                                          bool is_subsegment) {
  // No-op for subsegment in single segment mode.
  if (is_subsegment)
    return Status::OK;
  // Create a new Cue point.
  uint64_t position = writer_->Position();
  uint64_t start_webm_timecode = FromBMFFTimescale(start_timescale);

  mkvmuxer::CuePoint* cue_point = new mkvmuxer::CuePoint;
  cue_point->set_time(start_webm_timecode);
  cue_point->set_track(track_id());
  cue_point->set_cluster_pos(position - segment_payload_pos());
  if (!cues()->AddCue(cue_point))
    return Status(error::INTERNAL_ERROR, "Error adding CuePoint.");

  return SetCluster(start_webm_timecode, position, writer_.get());
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
