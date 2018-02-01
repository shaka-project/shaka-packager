// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp4/multi_segment_segmenter.h"

#include <algorithm>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_util.h"
#include "packager/file/file.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/formats/mp4/box_definitions.h"
#include "packager/media/formats/mp4/key_frame_info.h"

namespace shaka {
namespace media {
namespace mp4 {

MultiSegmentSegmenter::MultiSegmentSegmenter(const MuxerOptions& options,
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

MultiSegmentSegmenter::~MultiSegmentSegmenter() {}

bool MultiSegmentSegmenter::GetInitRange(size_t* offset, size_t* size) {
  VLOG(1) << "MultiSegmentSegmenter outputs init segment: "
          << options().output_file_name;
  return false;
}

bool MultiSegmentSegmenter::GetIndexRange(size_t* offset, size_t* size) {
  VLOG(1) << "MultiSegmentSegmenter does not have index range.";
  return false;
}

std::vector<Range> MultiSegmentSegmenter::GetSegmentRanges() {
  VLOG(1) << "MultiSegmentSegmenter does not have media segment ranges.";
  return std::vector<Range>();
}

Status MultiSegmentSegmenter::DoInitialize() {
  DCHECK(ftyp());
  DCHECK(moov());
  // Generate the output file with init segment.
  File* file = File::Open(options().output_file_name.c_str(), "w");
  if (file == NULL) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file for write " + options().output_file_name);
  }
  std::unique_ptr<BufferWriter> buffer(new BufferWriter);
  ftyp()->Write(buffer.get());
  moov()->Write(buffer.get());
  Status status = buffer->WriteToFile(file);
  if (!file->Close()) {
    LOG(WARNING) << "Failed to close the file properly: "
                 << options().output_file_name;
  }
  return status;
}

Status MultiSegmentSegmenter::DoFinalize() {
  SetComplete();
  return Status::OK;
}

Status MultiSegmentSegmenter::DoFinalizeSegment() {
  DCHECK(sidx());
  // earliest_presentation_time is the earliest presentation time of any
  // access unit in the reference stream in the first subsegment.
  // It will be re-calculated later when subsegments are finalized.
  sidx()->earliest_presentation_time =
      sidx()->references[0].earliest_presentation_time;

  if (options().mp4_params.num_subsegments_per_sidx <= 0)
    return WriteSegment();

  // sidx() contains pre-generated segment references with one reference per
  // fragment. Calculate |num_fragments_per_subsegment| and combine
  // pre-generated references into final subsegment references.
  size_t num_fragments = sidx()->references.size();
  size_t num_fragments_per_subsegment =
      (num_fragments - 1) / options().mp4_params.num_subsegments_per_sidx + 1;
  if (num_fragments_per_subsegment <= 1)
    return WriteSegment();

  size_t frag_index = 0;
  size_t subseg_index = 0;
  std::vector<SegmentReference>& refs = sidx()->references;
  uint64_t first_sap_time =
      refs[0].sap_delta_time + refs[0].earliest_presentation_time;
  for (size_t i = 1; i < num_fragments; ++i) {
    refs[subseg_index].referenced_size += refs[i].referenced_size;
    refs[subseg_index].subsegment_duration += refs[i].subsegment_duration;
    refs[subseg_index].earliest_presentation_time =
        std::min(refs[subseg_index].earliest_presentation_time,
                 refs[i].earliest_presentation_time);
    if (refs[subseg_index].sap_type == SegmentReference::TypeUnknown &&
        refs[i].sap_type != SegmentReference::TypeUnknown) {
      refs[subseg_index].sap_type = refs[i].sap_type;
      first_sap_time =
          refs[i].sap_delta_time + refs[i].earliest_presentation_time;
    }
    if (++frag_index >= num_fragments_per_subsegment) {
      // Calculate sap delta time w.r.t. sidx_->earliest_presentation_time.
      if (refs[subseg_index].sap_type != SegmentReference::TypeUnknown) {
        refs[subseg_index].sap_delta_time =
            first_sap_time - refs[subseg_index].earliest_presentation_time;
      }
      if (++i >= num_fragments)
        break;
      refs[++subseg_index] = refs[i];
      first_sap_time =
          refs[i].sap_delta_time + refs[i].earliest_presentation_time;
      frag_index = 1;
    }
  }

  refs.resize(options().mp4_params.num_subsegments_per_sidx);

  // earliest_presentation_time is the earliest presentation time of any
  // access unit in the reference stream in the first subsegment.
  sidx()->earliest_presentation_time = refs[0].earliest_presentation_time;

  return WriteSegment();
}

Status MultiSegmentSegmenter::WriteSegment() {
  DCHECK(sidx());
  DCHECK(fragment_buffer());
  DCHECK(styp_);

  std::unique_ptr<BufferWriter> buffer(new BufferWriter());
  File* file;
  std::string file_name;
  if (options().segment_template.empty()) {
    // Append the segment to output file if segment template is not specified.
    file_name = options().output_file_name.c_str();
    file = File::Open(file_name.c_str(), "a");
    if (file == NULL) {
      return Status(
          error::FILE_FAILURE,
          "Cannot open file for append " + options().output_file_name);
    }
  } else {
    file_name = GetSegmentName(options().segment_template,
                               sidx()->earliest_presentation_time,
                               num_segments_++, options().bandwidth);
    file = File::Open(file_name.c_str(), "w");
    if (file == NULL) {
      return Status(error::FILE_FAILURE,
                    "Cannot open file for write " + file_name);
    }
    styp_->Write(buffer.get());
  }

  // If num_subsegments_per_sidx is negative, no SIDX box is generated.
  if (options().mp4_params.num_subsegments_per_sidx >= 0)
    sidx()->Write(buffer.get());

  const size_t segment_header_size = buffer->Size();
  const size_t segment_size = segment_header_size + fragment_buffer()->Size();
  DCHECK_NE(segment_size, 0u);

  Status status = buffer->WriteToFile(file);
  if (status.ok()) {
    if (muxer_listener()) {
      for (const KeyFrameInfo& key_frame_info : key_frame_infos()) {
        muxer_listener()->OnKeyFrame(
            key_frame_info.timestamp,
            segment_header_size + key_frame_info.start_byte_offset,
            key_frame_info.size);
      }
    }
    status = fragment_buffer()->WriteToFile(file);
  }

  if (!file->Close())
    LOG(WARNING) << "Failed to close the file properly: " << file_name;

  if (!status.ok())
    return status;

  uint64_t segment_duration = 0;
  // ISO/IEC 23009-1:2012: the value shall be identical to sum of the the
  // values of all Subsegment_duration fields in the first ‘sidx’ box.
  for (size_t i = 0; i < sidx()->references.size(); ++i)
    segment_duration += sidx()->references[i].subsegment_duration;

  UpdateProgress(segment_duration);
  if (muxer_listener()) {
    muxer_listener()->OnSampleDurationReady(sample_duration());
    muxer_listener()->OnNewSegment(file_name,
                                   sidx()->earliest_presentation_time,
                                   segment_duration, segment_size);
  }

  return Status::OK;
}

}  // namespace mp4
}  // namespace media
}  // namespace shaka
