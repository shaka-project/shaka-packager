// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/hls/base/tag.h>

#include <cinttypes>

#include <absl/strings/str_format.h>

namespace shaka {
namespace hls {

Tag::Tag(const std::string& name, std::string* buffer) : buffer_(buffer) {
  absl::StrAppendFormat(buffer_, "%s:", name.c_str());
}

void Tag::AddString(const std::string& key, const std::string& value) {
  NextField();
  absl::StrAppendFormat(buffer_, "%s=%s", key.c_str(), value.c_str());
}

void Tag::AddQuotedString(const std::string& key, const std::string& value) {
  NextField();
  absl::StrAppendFormat(buffer_, "%s=\"%s\"", key.c_str(), value.c_str());
}

void Tag::AddNumber(const std::string& key, uint64_t value) {
  NextField();
  absl::StrAppendFormat(buffer_, "%s=%" PRIu64, key.c_str(), value);
}

void Tag::AddFloat(const std::string& key, float value) {
  NextField();
  absl::StrAppendFormat(buffer_, "%s=%.3f", key.c_str(), value);
}

void Tag::AddNumberPair(const std::string& key,
                        uint64_t number1,
                        char separator,
                        uint64_t number2) {
  NextField();
  absl::StrAppendFormat(buffer_, "%s=%" PRIu64 "%c%" PRIu64, key.c_str(),
                        number1, separator, number2);
}

void Tag::AddQuotedNumberPair(const std::string& key,
                              uint64_t number1,
                              char separator,
                              uint64_t number2) {
  NextField();
  absl::StrAppendFormat(buffer_, "%s=\"%" PRIu64 "%c%" PRIu64 "\"", key.c_str(),
                        number1, separator, number2);
}

void Tag::NextField() {
  if (fields++) {
    buffer_->append(",");
  }
}

}  // namespace hls
}  // namespace shaka
