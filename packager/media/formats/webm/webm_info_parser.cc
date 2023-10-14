// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_info_parser.h>

#include <ctime>

#include <absl/log/log.h>

#include <packager/macros/logging.h>
#include <packager/media/formats/webm/webm_constants.h>

namespace shaka {
namespace media {

// Default timecode scale if the TimecodeScale element is
// not specified in the INFO element.
static const int kWebMDefaultTimecodeScale = 1000000;

WebMInfoParser::WebMInfoParser()
    : timecode_scale_(-1),
      duration_(-1) {
}

WebMInfoParser::~WebMInfoParser() {}

int WebMInfoParser::Parse(const uint8_t* buf, int size) {
  timecode_scale_ = -1;
  duration_ = -1;

  WebMListParser parser(kWebMIdInfo, this);
  int result = parser.Parse(buf, size);

  if (result <= 0)
    return result;

  // For now we do all or nothing parsing.
  return parser.IsParsingComplete() ? result : 0;
}

WebMParserClient* WebMInfoParser::OnListStart(int /*id*/) {
  return this;
}

bool WebMInfoParser::OnListEnd(int id) {
  if (id == kWebMIdInfo && timecode_scale_ == -1) {
    // Set timecode scale to default value if it isn't present in
    // the Info element.
    timecode_scale_ = kWebMDefaultTimecodeScale;
  }
  return true;
}

bool WebMInfoParser::OnUInt(int id, int64_t val) {
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

bool WebMInfoParser::OnBinary(int id, const uint8_t* data, int size) {
  if (id == kWebMIdDateUTC) {
    if (size != 8)
      return false;

    int64_t date_in_nanoseconds = 0;
    for (int i = 0; i < size; ++i)
      date_in_nanoseconds = (date_in_nanoseconds << 8) | data[i];

    std::tm exploded_epoch;
    exploded_epoch.tm_year = 2001;
    exploded_epoch.tm_mon = 1;
    exploded_epoch.tm_mday = 1;
    exploded_epoch.tm_hour = 0;
    exploded_epoch.tm_min = 0;
    exploded_epoch.tm_sec = 0;

    date_utc_ =
        std::chrono::system_clock::from_time_t(std::mktime(&exploded_epoch)) +
        std::chrono::microseconds(date_in_nanoseconds / 1000);
  }
  return true;
}

bool WebMInfoParser::OnString(int /*id*/, const std::string& /*str*/) {
  return true;
}

}  // namespace media
}  // namespace shaka
