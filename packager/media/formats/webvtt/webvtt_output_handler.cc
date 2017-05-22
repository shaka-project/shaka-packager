// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webvtt/webvtt_output_handler.h"

#include "packager/base/logging.h"
#include "packager/file/file.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/formats/webvtt/webvtt_timestamp.h"

namespace shaka {
namespace media {
void WebVttOutputHandler::WriteCue(const std::string& id,
                                   uint64_t start_ms,
                                   uint64_t end_ms,
                                   const std::string& settings,
                                   const std::string& payload) {
  // Build a block of text that makes up the cue so that we can use a loop to
  // write all the lines.
  const std::string start = MsToWebVttTimestamp(start_ms);
  const std::string end = MsToWebVttTimestamp(end_ms);

  // Ids are optional
  if (id.length()) {
    buffer_.append(id);
    buffer_.append("\n");  // end of id
  }

  buffer_.append(start);
  buffer_.append(" --> ");
  buffer_.append(end);

  // Settings are optional
  if (settings.length()) {
    buffer_.append(" ");
    buffer_.append(settings);
  }
  buffer_.append("\n");  // end of time & settings

  buffer_.append(payload);
  buffer_.append("\n");  // end of payload
  buffer_.append("\n");  // end of cue
}

Status WebVttOutputHandler::WriteSegmentToFile(const std::string& filename) {
  // Need blank line between "WEBVTT" and the first cue
  const std::string WEBVTT_HEADER = "WEBVTT\n\n";

  File* file = File::Open(filename.c_str(), "w");

  if (file == nullptr) {
    return Status(error::FILE_FAILURE, "Failed to open " + filename);
  }

  size_t written;
  written = file->Write(WEBVTT_HEADER.c_str(), WEBVTT_HEADER.size());
  if (written != WEBVTT_HEADER.size()) {
    return Status(error::FILE_FAILURE, "Failed to write webvtt header to file");
  }

  written = file->Write(buffer_.c_str(), buffer_.size());
  if (written != buffer_.size()) {
    return Status(error::FILE_FAILURE,
                  "Failed to write webvtt cotnent to file");
  }

  // Since all the cues have been written to disk, there is no reason to hold
  // onto that information anymore.
  buffer_.clear();

  bool closed = file->Close();
  if (!closed) {
    return Status(error::FILE_FAILURE, "Failed to close " + filename);
  }

  return Status::OK;
}

Status WebVttOutputHandler::InitializeInternal() {
  return Status::OK;
}

Status WebVttOutputHandler::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return OnStreamInfo(*stream_data->stream_info);
    case StreamDataType::kSegmentInfo:
      return OnSegmentInfo(*stream_data->segment_info);
    case StreamDataType::kTextSample:
      return OnTextSample(*stream_data->text_sample);
    default:
      return Status(error::INTERNAL_ERROR,
                    "Invalid stream data type for this handler");
  }
}

Status WebVttOutputHandler::OnFlushRequest(size_t input_stream_index) {
  OnStreamEnd();
  return Status::OK;
}

WebVttSegmentedOutputHandler::WebVttSegmentedOutputHandler(
    const MuxerOptions& muxer_options,
    std::unique_ptr<MuxerListener> muxer_listener)
    : muxer_options_(muxer_options),
      muxer_listener_(std::move(muxer_listener)) {}

Status WebVttSegmentedOutputHandler::OnStreamInfo(const StreamInfo& info) {
  muxer_listener_->OnMediaStart(muxer_options_, info, info.time_scale(),
                                MuxerListener::kContainerText);
  return Status::OK;
}

Status WebVttSegmentedOutputHandler::OnSegmentInfo(const SegmentInfo& info) {
  total_duration_ms_ += info.duration;

  const std::string& segment_template = muxer_options_.segment_template;
  const uint32_t index = segment_index_++;
  const uint64_t start = info.start_timestamp;
  const uint64_t duration = info.duration;
  const uint32_t bandwidth = 0;

  // Write all the samples to the file.
  const std::string filename =
      GetSegmentName(segment_template, start, index, bandwidth);

  // Write everything to the file before telling the manifest so that the
  // file will exist on disk.
  Status write_status = WriteSegmentToFile(filename);
  if (!write_status.ok()) {
    return write_status;
  }

  // Update the manifest with our new file.
  const uint64_t size = File::GetFileSize(filename.c_str());
  muxer_listener_->OnNewSegment(filename, start, duration, size);

  return Status::OK;
}

Status WebVttSegmentedOutputHandler::OnTextSample(const TextSample& sample) {
  const std::string& id = sample.id();
  const uint64_t start_ms = sample.start_time();
  const uint64_t end_ms = sample.EndTime();
  const std::string& settings = sample.settings();
  const std::string& payload = sample.payload();

  WriteCue(id, start_ms, end_ms, settings, payload);
  return Status::OK;
}

Status WebVttSegmentedOutputHandler::OnStreamEnd() {
  const float duration_ms = static_cast<float>(total_duration_ms_);
  const float duration_seconds = duration_ms / 1000.0f;

  MuxerListener::MediaRanges empty_ranges;
  muxer_listener_->OnMediaEnd(empty_ranges, duration_seconds);

  return Status::OK;
}

}  // namespace media
}  // namespace shaka
