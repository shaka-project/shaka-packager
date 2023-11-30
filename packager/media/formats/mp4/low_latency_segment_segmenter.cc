// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp4/low_latency_segment_segmenter.h>

#include <algorithm>

#include <absl/log/check.h>

#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/macros/logging.h>
#include <packager/macros/status.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/media_handler.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/muxer_util.h>
#include <packager/media/event/muxer_listener.h>
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/media/formats/mp4/fragmenter.h>
#include <packager/media/formats/mp4/key_frame_info.h>

namespace shaka {
namespace media {
namespace mp4 {

LowLatencySegmentSegmenter::LowLatencySegmentSegmenter(
    const MuxerOptions& options,
    std::unique_ptr<FileType> ftyp,
    std::unique_ptr<Movie> moov)
    : Segmenter(options, std::move(ftyp), std::move(moov)),
      styp_(new SegmentType),
      num_segments_(0) {
  // Use the same brands for styp as ftyp.
  styp_->major_brand = Segmenter::ftyp()->major_brand;
  styp_->compatible_brands = Segmenter::ftyp()->compatible_brands;
  // Replace 'cmfc' with 'cmfs' for CMAF segments compatibility.
  std::replace(styp_->compatible_brands.begin(), styp_->compatible_brands.end(),
               FOURCC_cmfc, FOURCC_cmfs);
}

LowLatencySegmentSegmenter::~LowLatencySegmentSegmenter() {}

bool LowLatencySegmentSegmenter::GetInitRange(size_t* offset, size_t* size) {
  VLOG(1) << "LowLatencySegmentSegmenter outputs init segment: "
          << options().output_file_name;
  return false;
}

bool LowLatencySegmentSegmenter::GetIndexRange(size_t* offset, size_t* size) {
  VLOG(1) << "LowLatencySegmentSegmenter does not have index range.";
  return false;
}

std::vector<Range> LowLatencySegmentSegmenter::GetSegmentRanges() {
  VLOG(1) << "LowLatencySegmentSegmenter does not have media segment ranges.";
  return std::vector<Range>();
}

Status LowLatencySegmentSegmenter::DoInitialize() {
  return WriteInitSegment();
}

Status LowLatencySegmentSegmenter::DoFinalize() {
  // Update init segment with media duration set.
  RETURN_IF_ERROR(WriteInitSegment());
  SetComplete();
  return Status::OK;
}

Status LowLatencySegmentSegmenter::DoFinalizeSegment() {
  return FinalizeSegment();
}

Status LowLatencySegmentSegmenter::DoFinalizeChunk() {
  if (is_initial_chunk_in_seg_) {
    return WriteInitialChunk();
  }
  return WriteChunk();
}

Status LowLatencySegmentSegmenter::WriteInitSegment() {
  DCHECK(ftyp());
  DCHECK(moov());
  // Generate the output file with init segment.
  std::unique_ptr<File, FileCloser> file(
      File::Open(options().output_file_name.c_str(), "w"));
  if (!file) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file for write " + options().output_file_name);
  }
  std::unique_ptr<BufferWriter> buffer(new BufferWriter);
  ftyp()->Write(buffer.get());
  moov()->Write(buffer.get());
  return buffer->WriteToFile(file.get());
}

Status LowLatencySegmentSegmenter::WriteInitialChunk() {
  DCHECK(sidx());
  DCHECK(fragment_buffer());
  DCHECK(styp_);

  DCHECK(!sidx()->references.empty());
  // earliest_presentation_time is the earliest presentation time of any access
  // unit in the reference stream in the first subsegment.
  sidx()->earliest_presentation_time =
      sidx()->references[0].earliest_presentation_time;

  if (options().segment_template.empty()) {
    // Append the segment to output file if segment template is not specified.
    file_name_ = options().output_file_name.c_str();
  } else {
    file_name_ = GetSegmentName(options().segment_template,
                                sidx()->earliest_presentation_time,
                                num_segments_, options().bandwidth);
  }

  // Create the segment file
  segment_file_.reset(File::Open(file_name_.c_str(), "a"));
  if (!segment_file_) {
    return Status(error::FILE_FAILURE,
                  "Cannot open segment file: " + file_name_);
  }

  std::unique_ptr<BufferWriter> buffer(new BufferWriter());

  // Write the styp header to the beginning of the segment.
  styp_->Write(buffer.get());

  const size_t segment_header_size = buffer->Size();
  segment_size_ = segment_header_size + fragment_buffer()->Size();
  DCHECK_NE(segment_size_, 0u);

  RETURN_IF_ERROR(buffer->WriteToFile(segment_file_.get()));
  if (muxer_listener()) {
    for (const KeyFrameInfo& key_frame_info : key_frame_infos()) {
      muxer_listener()->OnKeyFrame(
          key_frame_info.timestamp,
          segment_header_size + key_frame_info.start_byte_offset,
          key_frame_info.size);
    }
  }

  // Write the chunk data to the file
  RETURN_IF_ERROR(fragment_buffer()->WriteToFile(segment_file_.get()));

  uint64_t segment_duration = GetSegmentDuration();
  UpdateProgress(segment_duration);

  if (muxer_listener()) {
    if (!ll_dash_mpd_values_initialized_) {
      // Set necessary values for LL-DASH mpd after the first chunk has been
      // processed.
      muxer_listener()->OnSampleDurationReady(sample_duration());
      muxer_listener()->OnAvailabilityOffsetReady();
      muxer_listener()->OnSegmentDurationReady();
      ll_dash_mpd_values_initialized_ = true;
    }
    // Add the current segment in the manifest.
    // Following chunks will be appended to the open segment file.
    muxer_listener()->OnNewSegment(file_name_,
                                   sidx()->earliest_presentation_time,
                                   segment_duration, segment_size_);
    is_initial_chunk_in_seg_ = false;
  }

  return Status::OK;
}

Status LowLatencySegmentSegmenter::WriteChunk() {
  DCHECK(fragment_buffer());

  // Write the chunk data to the file
  RETURN_IF_ERROR(fragment_buffer()->WriteToFile(segment_file_.get()));

  UpdateProgress(GetSegmentDuration());

  return Status::OK;
}

Status LowLatencySegmentSegmenter::FinalizeSegment() {
  if (muxer_listener()) {
    muxer_listener()->OnCompletedSegment(GetSegmentDuration(), segment_size_);
  }
  // Close the file now that the final chunk has been written
  if (!segment_file_.release()->Close()) {
    return Status(
        error::FILE_FAILURE,
        "Cannot close file " + file_name_ +
            ", possibly file permission issue or running out of disk space.");
  }

  // Current segment is complete. Reset state in preparation for the next
  // segment.
  is_initial_chunk_in_seg_ = true;
  segment_size_ = 0u;
  num_segments_++;

  return Status::OK;
}

uint64_t LowLatencySegmentSegmenter::GetSegmentDuration() {
  DCHECK(sidx());

  uint64_t segment_duration = 0;
  // ISO/IEC 23009-1:2012: the value shall be identical to sum of the the
  // values of all Subsegment_duration fields in the first ‘sidx’ box.
  for (size_t i = 0; i < sidx()->references.size(); ++i)
    segment_duration += sidx()->references[i].subsegment_duration;

  return segment_duration;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
