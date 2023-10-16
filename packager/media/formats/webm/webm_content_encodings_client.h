// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_

#include <memory>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/formats/webm/webm_content_encodings.h>
#include <packager/media/formats/webm/webm_parser.h>

namespace shaka {
namespace media {

typedef std::vector<std::unique_ptr<ContentEncoding>> ContentEncodings;

/// Parser for WebM ContentEncodings element.
class WebMContentEncodingsClient : public WebMParserClient {
 public:
  WebMContentEncodingsClient();
  ~WebMContentEncodingsClient() override;

  const ContentEncodings& content_encodings() const;

  /// WebMParserClient methods
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;

 private:
  std::unique_ptr<ContentEncoding> cur_content_encoding_;
  bool content_encryption_encountered_;
  ContentEncodings content_encodings_;

  // |content_encodings_| is ready. For debugging purpose.
  bool content_encodings_ready_;

  DISALLOW_COPY_AND_ASSIGN(WebMContentEncodingsClient);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_CONTENT_ENCODINGS_CLIENT_H_
