// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MUXER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MUXER_H_

#include <cstdint>
#include <memory>

#include <packager/media/base/text_muxer.h>
#include <packager/media/formats/webvtt/webvtt_file_buffer.h>

namespace shaka {
namespace media {
namespace webvtt {

/// Implements WebVtt Muxer.
class WebVttMuxer : public TextMuxer {
 public:
  /// Create a WebMMuxer object from MuxerOptions.
  explicit WebVttMuxer(const MuxerOptions& options);
  ~WebVttMuxer() override;

 private:
  // TextMuxer implementation overrides.
  Status InitializeStream(TextStreamInfo* stream) override;
  Status AddTextSampleInternal(const TextSample& sample) override;
  Status WriteToFile(const std::string& filename, uint64_t* size) override;

  std::unique_ptr<WebVttFileBuffer> buffer_;
};

}  // namespace webvtt
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MUXER_H_
