// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/hls/base/tag.h"

#include <inttypes.h>

#include "packager/base/strings/stringprintf.h"

namespace shaka {
namespace hls {

Tag::Tag(const std::string& name, std::string* buffer) : buffer_(buffer) {
  base::StringAppendF(buffer_, "%s:", name.c_str());
}

void Tag::AddString(const std::string& key, const std::string& value) {
  NextField();
  base::StringAppendF(buffer_, "%s=%s", key.c_str(), value.c_str());
}

void Tag::AddQuotedString(const std::string& key, const std::string& value) {
  NextField();
  base::StringAppendF(buffer_, "%s=\"%s\"", key.c_str(), value.c_str());
}

void Tag::AddNumber(const std::string& key, uint64_t value) {
  NextField();
  base::StringAppendF(buffer_, "%s=%" PRIu64, key.c_str(), value);
}

void Tag::AddResolution(const std::string& key,
                        uint32_t width,
                        uint32_t height) {
  NextField();
  base::StringAppendF(buffer_, "%s=%" PRIu32 "x%" PRIu32, key.c_str(), width,
                      height);
}

void Tag::NextField() {
  if (fields++) {
    buffer_->append(",");
  }
}

}  // namespace hls
}  // namespace shaka
