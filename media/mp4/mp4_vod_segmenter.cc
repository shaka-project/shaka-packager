// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mp4/mp4_vod_segmenter.h"

#include "media/base/buffer_writer.h"
#include "media/base/media_stream.h"
#include "media/base/muxer_options.h"
#include "media/file/file.h"
#include "media/mp4/box_definitions.h"

namespace media {
namespace mp4 {

MP4VODSegmenter::MP4VODSegmenter(const MuxerOptions& options,
                                 scoped_ptr<FileType> ftyp,
                                 scoped_ptr<Movie> moov)
    : MP4Segmenter(options, ftyp.Pass(), moov.Pass()) {}
MP4VODSegmenter::~MP4VODSegmenter() {}

Status MP4VODSegmenter::Initialize(EncryptorSource* encryptor_source,
                                   double clear_lead_in_seconds,
                                   const std::vector<MediaStream*>& streams) {
  Status status = MP4Segmenter::Initialize(
      encryptor_source, clear_lead_in_seconds, streams);
  if (!status.ok())
    return status;
  temp_file_.reset(File::Open(options().temp_file_name.c_str(), "w"));
  if (temp_file_ == NULL) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file to write " + options().temp_file_name);
  }
  return Status::OK;
}

Status MP4VODSegmenter::Finalize() {
  DCHECK(temp_file_ != NULL && ftyp() != NULL && moov() != NULL &&
         vod_sidx_ != NULL);

  Status status = MP4Segmenter::Finalize();
  if (!status.ok())
    return status;

  // Close the temp file to prepare for reading later.
  if (!temp_file_.release()->Close()) {
    return Status(error::FILE_FAILURE,
                  "Cannot close the temp file " + options().temp_file_name);
  }

  scoped_ptr<File, FileCloser> file(
      File::Open(options().output_file_name.c_str(), "w"));
  if (file == NULL) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file to write " + options().output_file_name);
  }

  // Write ftyp, moov and sidx to output file.
  scoped_ptr<BufferWriter> buffer(new BufferWriter());
  ftyp()->Write(buffer.get());
  moov()->Write(buffer.get());
  vod_sidx_->Write(buffer.get());
  status = buffer->WriteToFile(file.get());
  if (!status.ok())
    return status;

  // Load the temp file and write to output file.
  scoped_ptr<File, FileCloser> temp_file(
      File::Open(options().temp_file_name.c_str(), "r"));
  if (temp_file == NULL) {
    return Status(error::FILE_FAILURE,
                  "Cannot open file to read " + options().temp_file_name);
  }

  const int kBufSize = 0x40000;  // 256KB.
  scoped_ptr<uint8[]> buf(new uint8[kBufSize]);
  while (!temp_file->Eof()) {
    int64 size = temp_file->Read(buf.get(), kBufSize);
    if (size <= 0) {
      return Status(error::FILE_FAILURE,
                    "Failed to read file " + options().temp_file_name);
    }
    int64 size_written = file->Write(buf.get(), size);
    if (size_written != size) {
      return Status(error::FILE_FAILURE,
                    "Failed to write file " + options().output_file_name);
    }
  }
  return Status::OK;
}

bool MP4VODSegmenter::GetInitRange(size_t* offset, size_t* size) {
  // In Finalize, ftyp and moov gets written first so offset must be 0.
  *offset = 0;
  *size = ftyp()->ComputeSize() + moov()->ComputeSize();
  return true;
}

bool MP4VODSegmenter::GetIndexRange(size_t* offset, size_t* size) {
  // Index range is right after init range so the offset must be the size of
  // ftyp and moov.
  *offset = ftyp()->ComputeSize() + moov()->ComputeSize();
  *size = vod_sidx_->ComputeSize();
  return true;
}

Status MP4VODSegmenter::FinalizeSegment() {
  Status status = MP4Segmenter::FinalizeSegment();
  if (!status.ok())
    return status;

  DCHECK(sidx() != NULL && fragment_buffer() != NULL);
  // sidx() contains pre-generated segment references with one reference per
  // fragment. In VOD, this segment is converted into a subsegment, i.e. one
  // reference, which contains all the fragments in sidx().
  std::vector<SegmentReference>& refs = sidx()->references;
  SegmentReference& vod_ref = refs[0];
  uint64 first_sap_time =
      refs[0].sap_delta_time + refs[0].earliest_presentation_time;
  for (uint32 i = 1; i < sidx()->references.size(); ++i) {
    vod_ref.referenced_size += refs[i].referenced_size;
    // TODO(kqyang): should we calculate subsegment duration by subtracting
    // earliest_presentation time instead?
    vod_ref.subsegment_duration += refs[i].subsegment_duration;
    vod_ref.earliest_presentation_time = std::min(
        vod_ref.earliest_presentation_time, refs[i].earliest_presentation_time);

    if (vod_ref.sap_type == SegmentReference::TypeUnknown &&
        refs[i].sap_type != SegmentReference::TypeUnknown) {
      vod_ref.sap_type = refs[i].sap_type;
      first_sap_time =
          refs[i].sap_delta_time + refs[i].earliest_presentation_time;
    }
  }
  // Calculate sap delta time w.r.t. earliest_presentation_time.
  if (vod_ref.sap_type != SegmentReference::TypeUnknown) {
    vod_ref.sap_delta_time =
        first_sap_time - vod_ref.earliest_presentation_time;
  }

  // Create segment if it does not exist yet.
  if (vod_sidx_ == NULL) {
    vod_sidx_.reset(new SegmentIndex());
    vod_sidx_->reference_id = sidx()->reference_id;
    vod_sidx_->timescale = sidx()->timescale;
    // earliest_presentation_time is the earliest presentation time of any
    // access unit in the reference stream in the first subsegment.
    vod_sidx_->earliest_presentation_time = vod_ref.earliest_presentation_time;
  }
  vod_sidx_->references.push_back(vod_ref);

  // Append fragment buffer to temp file.
  return fragment_buffer()->WriteToFile(temp_file_.get());
}

}  // namespace mp4
}  // namespace media
