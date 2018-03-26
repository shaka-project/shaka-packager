// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/text_sample.h"

#include "packager/base/logging.h"

namespace shaka {
namespace media {

int64_t TextSample::EndTime() const {
  return start_time_ + duration_;
}

void TextSample::SetTime(int64_t start_time, int64_t end_time) {
  DCHECK_GE(start_time, 0);
  DCHECK_GT(end_time, 0);
  DCHECK_LT(start_time, end_time);
  start_time_ = start_time;
  duration_ = end_time - start_time;
}

void TextSample::AppendStyle(const std::string& style) {
  if (settings_.length()) {
    settings_ += " ";
  }
  settings_ += style;
}

void TextSample::AppendPayload(const std::string& payload) {
  if (payload_.length()) {
    payload_ += "\n";
  }
  payload_ += payload;
}

}  // namespace media
}  // namespace shaka
