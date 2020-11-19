// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_muxer.h"

#include <memory>
#include <regex>

#include "packager/file/file.h"
#include "packager/file/file_closer.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/formats/webvtt/webvtt_utils.h"
#include "packager/status_macros.h"

namespace shaka {
namespace media {
namespace webvtt {

WebVttMuxer::WebVttMuxer(const MuxerOptions& options) : Muxer(options) {}
WebVttMuxer::~WebVttMuxer() {}

Status WebVttMuxer::InitializeMuxer() {
  if (streams().size() != 1 || streams()[0]->stream_type() != kStreamText) {
    return Status(error::MUXER_FAILURE,
                  "Incorrect streams given to WebVTT muxer");
  }

  // Only initialize the stream once we see a cue to avoid empty files.
  muxer_listener()->OnMediaStart(options(), *streams()[0],
                                 streams()[0]->time_scale(),
                                 MuxerListener::kContainerText);

  auto* stream = static_cast<const TextStreamInfo*>(streams()[0].get());
  const std::string preamble = WebVttGetPreamble(*stream);
  buffer_.reset(new WebVttFileBuffer(
      options().transport_stream_timestamp_offset_ms, preamble));
  last_cue_ms_ = 0;

  return Status::OK;
}

Status WebVttMuxer::Finalize() {
  const float duration_ms = static_cast<float>(total_duration_ms_);
  float duration_seconds = duration_ms / 1000;

  // If we haven't seen any segments, this is a single-file.  In this case,
  // flush the single segment.
  MuxerListener::MediaRanges ranges;
  if (duration_seconds == 0 && last_cue_ms_ != 0) {
    DCHECK(options().segment_template.empty());
    duration_seconds = static_cast<float>(last_cue_ms_) / 1000;

    uint64_t size;
    RETURN_IF_ERROR(WriteToFile(options().output_file_name, &size));
    // Insert a dummy value so the HLS generator will generate a segment list.
    ranges.subsegment_ranges.emplace_back();

    muxer_listener()->OnNewSegment(
        options().output_file_name, 0,
        duration_seconds * streams()[0]->time_scale(), size);
  }

  muxer_listener()->OnMediaEnd(ranges, duration_seconds);

  return Status::OK;
}

Status WebVttMuxer::AddTextSample(size_t stream_id, const TextSample& sample) {
  // Ignore sync samples.
  if (sample.body().is_empty()) {
    return Status::OK;
  }

  if (sample.id().find('\n') != std::string::npos) {
    return Status(error::MUXER_FAILURE, "Text id cannot contain newlines");
  }

  last_cue_ms_ = sample.EndTime();
  buffer_->Append(sample);
  return Status::OK;
}

Status WebVttMuxer::FinalizeSegment(size_t stream_id,
                                    const SegmentInfo& segment_info) {
  total_duration_ms_ += segment_info.duration;

  const std::string& segment_template = options().segment_template;
  DCHECK(!segment_template.empty());
  const uint32_t index = segment_index_++;
  const uint64_t start = segment_info.start_timestamp;
  const uint64_t duration = segment_info.duration;
  const uint32_t bandwidth = options().bandwidth;

  const std::string filename =
      GetSegmentName(segment_template, start, index, bandwidth);
  uint64_t size;
  RETURN_IF_ERROR(WriteToFile(filename, &size));

  muxer_listener()->OnNewSegment(filename, start, duration, size);
  return Status::OK;
}

Status WebVttMuxer::WriteToFile(const std::string& filename, uint64_t* size) {
  // Write everything to the file before telling the manifest so that the
  // file will exist on disk.
  std::unique_ptr<File, FileCloser> file(File::Open(filename.c_str(), "w"));
  if (!file) {
    return Status(error::FILE_FAILURE, "Failed to open " + filename);
  }

  buffer_->WriteTo(file.get());
  buffer_->Reset();
  if (!file.release()->Close()) {
    return Status(error::FILE_FAILURE, "Failed to close " + filename);
  }

  if (size) {
    *size = File::GetFileSize(filename.c_str());
  }
  return Status::OK;
}

}  // namespace webvtt
}  // namespace media
}  // namespace shaka
