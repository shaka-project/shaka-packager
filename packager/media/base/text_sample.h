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

struct TextSettings {
  // TODO(modmaker): Convert to generic structure.
  std::string settings;
};

struct TextFragment {
  // TODO(modmaker): Fill with settings and sub-fragments.
  std::string body;

  bool is_empty() const;
};

class TextSample {
 public:
  TextSample(const std::string& id,
             int64_t start_time,
             int64_t end_time,
             const TextSettings& settings,
             const TextFragment& body);

  const std::string& id() const { return id_; }
  int64_t start_time() const { return start_time_; }
  int64_t duration() const { return duration_; }
  const TextSettings& settings() const { return settings_; }
  const TextFragment& body() const { return body_; }
  int64_t EndTime() const;

 private:
  // Allow the compiler generated copy constructor and assignment operator
  // intentionally. Since the text data is typically small, the performance
  // impact is minimal.

  const std::string id_;
  const int64_t start_time_ = 0;
  const int64_t duration_ = 0;
  const TextSettings settings_;
  const TextFragment body_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TEXT_SAMPLE_H_
