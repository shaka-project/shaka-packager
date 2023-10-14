// Copyright 2018 Google LLC All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_FILE_BUFFER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_FILE_BUFFER_H_

#include <string>

#include <packager/file.h>

namespace shaka {
namespace media {

class TextSample;

// A class to abstract writing a webvtt file to disk. This class will handle
// all the formatting requirements for a webvtt file.
class WebVttFileBuffer {
 public:
  WebVttFileBuffer(int32_t transport_stream_timestamp_offset_ms,
                   const std::string& style_region_config);
  virtual ~WebVttFileBuffer() = default;

  void Reset();
  void Append(const TextSample& sample);

  bool WriteTo(File* file, uint64_t* size);

  // Get the number of samples that have been appended to this file.
  size_t sample_count() const { return sample_count_; }

 private:
  WebVttFileBuffer(const WebVttFileBuffer&) = delete;
  WebVttFileBuffer& operator=(const WebVttFileBuffer&) = delete;

  const int32_t transport_stream_timestamp_offset_ = 0;
  const std::string style_region_config_;
  std::string buffer_;
  size_t sample_count_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_FILE_BUFFER_H_
