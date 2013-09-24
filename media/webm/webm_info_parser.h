// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBM_WEBM_INFO_PARSER_H_
#define MEDIA_WEBM_WEBM_INFO_PARSER_H_

#include "base/compiler_specific.h"
#include "media/base/media_export.h"
#include "media/webm/webm_parser.h"

namespace media {

// Parser for WebM Info element.
class MEDIA_EXPORT WebMInfoParser : public WebMParserClient {
 public:
  WebMInfoParser();
  virtual ~WebMInfoParser();

  // Parses a WebM Info element in |buf|.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int Parse(const uint8* buf, int size);

  int64 timecode_scale() const { return timecode_scale_; }
  double duration() const { return duration_; }

 private:
  // WebMParserClient methods
  virtual WebMParserClient* OnListStart(int id) OVERRIDE;
  virtual bool OnListEnd(int id) OVERRIDE;
  virtual bool OnUInt(int id, int64 val) OVERRIDE;
  virtual bool OnFloat(int id, double val) OVERRIDE;
  virtual bool OnBinary(int id, const uint8* data, int size) OVERRIDE;
  virtual bool OnString(int id, const std::string& str) OVERRIDE;

  int64 timecode_scale_;
  double duration_;

  DISALLOW_COPY_AND_ASSIGN(WebMInfoParser);
};

}  // namespace media

#endif  // MEDIA_WEBM_WEBM_INFO_PARSER_H_
