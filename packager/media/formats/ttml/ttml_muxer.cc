// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/ttml/ttml_muxer.h>

#include <packager/file.h>
#include <packager/macros/status.h>

namespace shaka {
namespace media {
namespace ttml {

TtmlMuxer::TtmlMuxer(const MuxerOptions& options) : TextMuxer(options) {}
TtmlMuxer::~TtmlMuxer() {}

Status TtmlMuxer::InitializeStream(TextStreamInfo* stream) {
  stream->set_codec(kCodecTtml);
  stream->set_codec_string("ttml");
  generator_.Initialize(stream->regions(), stream->language(),
                        stream->time_scale());
  return Status::OK;
}

Status TtmlMuxer::AddTextSampleInternal(const TextSample& sample) {
  generator_.AddSample(sample);
  return Status::OK;
}

Status TtmlMuxer::WriteToFile(const std::string& filename, uint64_t* size) {
  std::string data;
  if (!generator_.Dump(&data))
    return Status(error::INTERNAL_ERROR, "Error generating XML");
  generator_.Reset();
  *size = data.size();

  if (!File::WriteStringToFile(filename.c_str(), data))
    return Status(error::FILE_FAILURE, "Failed to write " + filename);
  return Status::OK;
}

}  // namespace ttml
}  // namespace media
}  // namespace shaka
