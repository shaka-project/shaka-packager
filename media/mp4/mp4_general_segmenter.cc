// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_general_segmenter.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/base/buffer_writer.h"
#include "media/base/media_stream.h"
#include "media/base/muxer_options.h"
#include "media/file/file.h"
#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

MP4GeneralSegmenter::MP4GeneralSegmenter(const MuxerOptions& options,
                                         scoped_ptr<FileType> ftyp,
                                         scoped_ptr<Movie> moov)
    : MP4Segmenter(options, ftyp.Pass(), moov.Pass()),
      styp_(new SegmentType),
      num_segments_(0) {
  // Use the same brands for styp as ftyp.
  styp_->major_brand = MP4Segmenter::ftyp()->major_brand;
  styp_->compatible_brands = MP4Segmenter::ftyp()->compatible_brands;
}

MP4GeneralSegmenter::~MP4GeneralSegmenter() {}

Status MP4GeneralSegmenter::Initialize(
    EncryptorSource* encryptor_source,
    const std::vector<MediaStream*>& streams) {
  Status status = MP4Segmenter::Initialize(encryptor_source, streams);
  if (!status.ok())
    return status;

  DCHECK(ftyp() != NULL && moov() != NULL);
  // Generate the output file with init segment.
  File* file = File::Open(options().output_file_name.c_str(), "w");
  if (file == NULL) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file for write " + options().output_file_name);
  }
  scoped_ptr<BufferWriter> buffer(new BufferWriter);
  ftyp()->Write(buffer.get());
  moov()->Write(buffer.get());
  status = buffer->WriteToFile(file);
  if (!file->Close()) {
    LOG(WARNING) << "Failed to close the file properly: "
                 << options().output_file_name;
  }
  return status;
}

Status MP4GeneralSegmenter::FinalizeSegment() {
  Status status = MP4Segmenter::FinalizeSegment();
  if (!status.ok())
    return status;

  DCHECK(sidx() != NULL);
  // earliest_presentation_time is the earliest presentation time of any
  // access unit in the reference stream in the first subsegment.
  // It will be re-calculated later when subsegments are finalized.
  sidx()->earliest_presentation_time =
      sidx()->references[0].earliest_presentation_time;

  if (options().num_subsegments_per_sidx <= 0)
    return WriteSegment();

  // sidx() contains pre-generated segment references with one reference per
  // fragment. Calculate |num_fragments_per_subsegment| and combine
  // pre-generated references into final subsegment references.
  uint32 num_fragments = sidx()->references.size();
  uint32 num_fragments_per_subsegment =
      (num_fragments - 1) / options().num_subsegments_per_sidx + 1;
  if (num_fragments_per_subsegment <= 1)
    return WriteSegment();

  uint32 frag_index = 0;
  uint32 subseg_index = 0;
  std::vector<SegmentReference>& refs = sidx()->references;
  uint64 first_sap_time =
      refs[0].sap_delta_time + refs[0].earliest_presentation_time;
  for (uint32 i = 1; i < num_fragments; ++i) {
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

  refs.resize(options().num_subsegments_per_sidx);

  // earliest_presentation_time is the earliest presentation time of any
  // access unit in the reference stream in the first subsegment.
  sidx()->earliest_presentation_time = refs[0].earliest_presentation_time;

  return WriteSegment();
}

Status MP4GeneralSegmenter::WriteSegment() {
  DCHECK(sidx() != NULL && fragment_buffer() != NULL && styp_ != NULL);

  scoped_ptr<BufferWriter> buffer(new BufferWriter());
  File* file;
  std::string file_name;
  if (options().segment_template.empty()) {
    // Append the segment to output file if segment template is not specified.
    file_name = options().output_file_name.c_str();
    file = File::Open(file_name.c_str(), "a+");
    if (file == NULL) {
      return Status(
          error::FILE_FAILURE,
          "Cannot open file for append " + options().output_file_name);
    }
  } else {
    // TODO(kqyang): generate the segment template name.
    file_name = options().segment_template;
    ReplaceSubstringsAfterOffset(
        &file_name, 0, "$Number$", base::UintToString(++num_segments_));
    file = File::Open(file_name.c_str(), "w");
    if (file == NULL) {
      return Status(error::FILE_FAILURE,
                    "Cannot open file for write " + file_name);
    }
    styp_->Write(buffer.get());
  }

  // If num_subsegments_per_sidx is negative, no SIDX box is generated.
  if (options().num_subsegments_per_sidx >= 0)
    sidx()->Write(buffer.get());

  Status status = buffer->WriteToFile(file);
  if (status.ok())
    status = fragment_buffer()->WriteToFile(file);

  if (!file->Close())
    LOG(WARNING) << "Failed to close the file properly: " << file_name;
  return status;
}

}  // namespace mp4
}  // namespace media
