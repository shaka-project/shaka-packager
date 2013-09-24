// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_info_parser.h"

#include "base/logging.h"
#include "media/webm/webm_constants.h"

namespace media {

// Default timecode scale if the TimecodeScale element is
// not specified in the INFO element.
static const int kWebMDefaultTimecodeScale = 1000000;

WebMInfoParser::WebMInfoParser()
    : timecode_scale_(-1),
      duration_(-1) {
}

WebMInfoParser::~WebMInfoParser() {}

int WebMInfoParser::Parse(const uint8* buf, int size) {
  timecode_scale_ = -1;
  duration_ = -1;

  WebMListParser parser(kWebMIdInfo, this);
  int result = parser.Parse(buf, size);

  if (result <= 0)
    return result;

  // For now we do all or nothing parsing.
  return parser.IsParsingComplete() ? result : 0;
}

WebMParserClient* WebMInfoParser::OnListStart(int id) { return this; }

bool WebMInfoParser::OnListEnd(int id) {
  if (id == kWebMIdInfo && timecode_scale_ == -1) {
    // Set timecode scale to default value if it isn't present in
    // the Info element.
    timecode_scale_ = kWebMDefaultTimecodeScale;
  }
  return true;
}

bool WebMInfoParser::OnUInt(int id, int64 val) {
  if (id != kWebMIdTimecodeScale)
    return true;

  if (timecode_scale_ != -1) {
    DVLOG(1) << "Multiple values for id " << std::hex << id << " specified";
    return false;
  }

  timecode_scale_ = val;
  return true;
}

bool WebMInfoParser::OnFloat(int id, double val) {
  if (id != kWebMIdDuration) {
    DVLOG(1) << "Unexpected float for id" << std::hex << id;
    return false;
  }

  if (duration_ != -1) {
    DVLOG(1) << "Multiple values for duration.";
    return false;
  }

  duration_ = val;
  return true;
}

bool WebMInfoParser::OnBinary(int id, const uint8* data, int size) {
  return true;
}

bool WebMInfoParser::OnString(int id, const std::string& str) {
  return true;
}

}  // namespace media
