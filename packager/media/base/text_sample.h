// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_
#define PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_

#include <stdint.h>

#include <string>

namespace shaka {
namespace media {

class TextSample {
 public:
  TextSample() = default;

  const std::string& id() const { return id_; }
  int64_t start_time() const { return start_time_; }
  int64_t duration() const { return duration_; }
  const std::string& settings() const { return settings_; }
  const std::string& payload() const { return payload_; }
  int64_t EndTime() const;

  void set_id(const std::string& id) { id_ = id; }
  void SetTime(int64_t start_time, int64_t end_time);
  void AppendStyle(const std::string& style);
  void AppendPayload(const std::string& payload);

 private:
  // Allow the compiler generated copy constructor and assignment operator
  // intentionally. Since the text data is typically small, the performance
  // impact is minimal.

  std::string id_;
  int64_t start_time_ = 0;
  int64_t duration_ = 0;
  std::string settings_;
  std::string payload_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_
