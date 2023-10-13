// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webvtt/webvtt_muxer.h>

#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/macros/status.h>
#include <packager/media/formats/webvtt/webvtt_utils.h>

namespace shaka {
namespace media {
namespace webvtt {

WebVttMuxer::WebVttMuxer(const MuxerOptions& options) : TextMuxer(options) {}
WebVttMuxer::~WebVttMuxer() {}

Status WebVttMuxer::InitializeStream(TextStreamInfo* stream) {
  stream->set_codec(kCodecWebVtt);
  stream->set_codec_string("wvtt");

  const std::string preamble = WebVttGetPreamble(*stream);
  buffer_.reset(new WebVttFileBuffer(
      options().transport_stream_timestamp_offset_ms, preamble));
  return Status::OK;
}

Status WebVttMuxer::AddTextSampleInternal(const TextSample& sample) {
  if (sample.id().find('\n') != std::string::npos) {
    return Status(error::MUXER_FAILURE, "Text id cannot contain newlines");
  }

  buffer_->Append(sample);
  return Status::OK;
}

Status WebVttMuxer::WriteToFile(const std::string& filename, uint64_t* size) {
  // Write everything to the file before telling the manifest so that the
  // file will exist on disk.
  std::unique_ptr<File, FileCloser> file(File::Open(filename.c_str(), "w"));
  if (!file) {
    return Status(error::FILE_FAILURE, "Failed to open " + filename);
  }

  buffer_->WriteTo(file.get(), size);
  buffer_->Reset();
  if (!file.release()->Close()) {
    return Status(error::FILE_FAILURE, "Failed to close " + filename);
  }

  return Status::OK;
}

}  // namespace webvtt
}  // namespace media
}  // namespace shaka
