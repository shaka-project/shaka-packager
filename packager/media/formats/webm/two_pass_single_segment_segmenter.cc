// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/two_pass_single_segment_segmenter.h"

#include <algorithm>

#include "packager/media/base/media_sample.h"
#include "packager/media/base/media_stream.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/file/file_util.h"
#include "packager/third_party/libwebm/src/mkvmuxer.hpp"
#include "packager/third_party/libwebm/src/mkvmuxerutil.hpp"
#include "packager/third_party/libwebm/src/webmids.hpp"

namespace shaka {
namespace media {
namespace webm {
namespace {
// Cues will be inserted before clusters. All clusters will be shifted down by
// the size of cues. However, cluster positions affect the size of cues. This
// function adjusts cues size iteratively until it is stable.
// Returns the size of updated Cues.
uint64_t UpdateCues(mkvmuxer::Cues* cues) {
  uint64_t cues_size = cues->Size();
  uint64_t adjustment = cues_size;
  while (adjustment != 0) {
    for (int i = 0; i < cues->cue_entries_size(); ++i) {
      mkvmuxer::CuePoint* cue = cues->GetCueByIndex(i);
      cue->set_cluster_pos(cue->cluster_pos() + adjustment);
    }
    uint64_t new_cues_size = cues->Size();
    DCHECK_LE(cues_size, new_cues_size);
    adjustment = new_cues_size - cues_size;
    cues_size = new_cues_size;
  }
  return cues_size;
}

// Skips a given number of bytes in a file by reading.  This allows
// forward-seeking in non-seekable files.
bool ReadSkip(File* file, int64_t byte_count) {
  const int64_t kBufferSize = 0x40000;  // 256KB.
  std::unique_ptr<char[]> buffer(new char[kBufferSize]);
  int64_t bytes_read = 0;
  while (bytes_read < byte_count) {
    int64_t size = std::min(kBufferSize, byte_count - bytes_read);
    int64_t result = file->Read(buffer.get(), size);
    // Only give success if there are no errors, not at EOF, and read exactly
    // byte_count bytes.
    if (result <= 0)
      return false;

    bytes_read += result;
  }

  DCHECK_EQ(bytes_read, byte_count);
  return true;
}
}  // namespace

TwoPassSingleSegmentSegmenter::TwoPassSingleSegmentSegmenter(
    const MuxerOptions& options)
    : SingleSegmentSegmenter(options) {}

TwoPassSingleSegmentSegmenter::~TwoPassSingleSegmentSegmenter() {}

Status TwoPassSingleSegmentSegmenter::DoInitialize(
    std::unique_ptr<MkvWriter> writer) {
  // Assume the amount of time to copy the temp file as the same amount
  // of time as to make it.
  set_progress_target(info()->duration() * 2);

  real_writer_ = std::move(writer);

  if (!TempFilePath(options().temp_dir, &temp_file_name_))
    return Status(error::FILE_FAILURE, "Unable to create temporary file.");
  std::unique_ptr<MkvWriter> temp(new MkvWriter);
  Status status = temp->Open(temp_file_name_);
  if (!status.ok())
    return status;

  return SingleSegmentSegmenter::DoInitialize(std::move(temp));
}

Status TwoPassSingleSegmentSegmenter::DoFinalize() {
  if (!cluster()->Finalize())
    return Status(error::FILE_FAILURE, "Error finalizing cluster.");

  const uint64_t header_size = init_end() + 1;
  const uint64_t cues_pos = header_size - segment_payload_pos();
  const uint64_t cues_size = UpdateCues(cues());
  seek_head()->set_cues_pos(cues_pos);
  seek_head()->set_cluster_pos(cues_pos + cues_size);

  // Write the header to the real output file.
  const uint64_t file_size = writer()->Position() + cues_size;
  Status temp = WriteSegmentHeader(file_size, real_writer_.get());
  if (!temp.ok())
    return temp;
  DCHECK_EQ(real_writer_->Position(), static_cast<int64_t>(header_size));

  // Write the cues to the real output file.
  set_index_start(real_writer_->Position());
  if (!cues()->Write(real_writer_.get()))
    return Status(error::FILE_FAILURE, "Error writing Cues data.");
  set_index_end(real_writer_->Position() - 1);
  DCHECK_EQ(real_writer_->Position(),
            static_cast<int64_t>(segment_payload_pos() + cues_pos + cues_size));

  // Close the temp file and open it for reading.
  set_writer(std::unique_ptr<MkvWriter>());
  std::unique_ptr<File, FileCloser> temp_reader(
      File::Open(temp_file_name_.c_str(), "r"));
  if (!temp_reader)
    return Status(error::FILE_FAILURE, "Error opening temp file.");

  // Skip the header that has already been written.
  if (!ReadSkip(temp_reader.get(), header_size))
    return Status(error::FILE_FAILURE, "Error reading temp file.");

  // Copy the rest of the data over.
  if (!CopyFileWithClusterRewrite(temp_reader.get(), real_writer_.get(),
                                  cluster()->Size())) {
    return Status(error::FILE_FAILURE, "Error copying temp file.");
  }

  // Close and delete the temp file.
  temp_reader.reset();
  if (!File::Delete(temp_file_name_.c_str())) {
    LOG(WARNING) << "Unable to delete temporary file " << temp_file_name_;
  }

  return real_writer_->Close();
}

bool TwoPassSingleSegmentSegmenter::CopyFileWithClusterRewrite(
    File* source,
    MkvWriter* dest,
    uint64_t last_size) {
  const int cluster_id_size = mkvmuxer::GetUIntSize(mkvmuxer::kMkvCluster);
  const int cluster_size_size = 8;  // The size of the Cluster size integer.
  const int cluster_header_size = cluster_id_size + cluster_size_size;

  // We are at the start of a cluster, so copy the ID.
  if (dest->WriteFromFile(source, cluster_id_size) != cluster_id_size)
    return false;

  for (int i = 0; i < cues()->cue_entries_size() - 1; ++i) {
    // Write the size of the cluster.
    const mkvmuxer::CuePoint* cue = cues()->GetCueByIndex(i);
    const mkvmuxer::CuePoint* next_cue = cues()->GetCueByIndex(i + 1);
    const int64_t cluster_payload_size =
        next_cue->cluster_pos() - cue->cluster_pos() - cluster_header_size;
    if (mkvmuxer::WriteUIntSize(dest, cluster_payload_size, cluster_size_size))
      return false;
    if (!ReadSkip(source, cluster_size_size))
      return false;

    // Copy the cluster and the next cluster's ID.
    int64_t to_copy = cluster_payload_size + cluster_id_size;
    if (dest->WriteFromFile(source, to_copy) != to_copy)
      return false;

    // Update the progress; need to convert from WebM timecode to ISO BMFF.
    const uint64_t webm_delta_time = next_cue->time() - cue->time();
    const uint64_t delta_time = FromWebMTimecode(webm_delta_time);
    UpdateProgress(delta_time);
  }

  // The last cluster takes up until the cues.
  const uint64_t last_cluster_payload_size = last_size - cluster_header_size;
  if (mkvmuxer::WriteUIntSize(dest, last_cluster_payload_size,
                              cluster_size_size))
    return false;
  if (!ReadSkip(source, cluster_size_size))
    return false;

  // Copy the last cluster.
  return dest->WriteFromFile(source) ==
         static_cast<int64_t>(last_cluster_payload_size);
}

}  // namespace webm
}  // namespace media
}  // namespace shaka
