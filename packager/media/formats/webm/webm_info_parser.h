// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_

#include <chrono>

#include <packager/macros/classes.h>
#include <packager/media/formats/webm/webm_parser.h>

namespace shaka {
namespace media {

/// Parser for WebM Info element.
class WebMInfoParser : public WebMParserClient {
 public:
  WebMInfoParser();
  ~WebMInfoParser() override;

  /// Parses a WebM Info element in |buf|.
  /// @return -1 if the parse fails.
  /// @return 0 if more data is needed.
  /// @return The number of bytes parsed on success.
  int Parse(const uint8_t* buf, int size);

  int64_t timecode_scale() const { return timecode_scale_; }
  double duration() const { return duration_; }
  std::chrono::system_clock::time_point date_utc() const { return date_utc_; }

 private:
  // WebMParserClient methods
  WebMParserClient* OnListStart(int id) override;
  bool OnListEnd(int id) override;
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;
  bool OnBinary(int id, const uint8_t* data, int size) override;
  bool OnString(int id, const std::string& str) override;

  int64_t timecode_scale_;
  double duration_;
  std::chrono::system_clock::time_point date_utc_;

  DISALLOW_COPY_AND_ASSIGN(WebMInfoParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_INFO_PARSER_H_
