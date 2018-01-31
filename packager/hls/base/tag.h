// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_HLS_BASE_TAG_H_
#define PACKAGER_HLS_BASE_TAG_H_

#include <string>

namespace shaka {
namespace hls {

/// Tag is a string formatting class used to build HLS tags that contain
/// argument lists.
class Tag {
 public:
  Tag(const std::string& name, std::string* buffer);

  /// Add a non-quoted string value to the argument list.
  void AddString(const std::string& key, const std::string& value);

  /// Add a quoted string value to the argument list.
  void AddQuotedString(const std::string& key, const std::string& value);

  /// Add a non-quoted numeric value to the argument list.
  void AddNumber(const std::string& key, uint64_t value);

  /// Add a resolution value (AxB) to the argument list.
  void AddResolution(const std::string& key, uint32_t width, uint32_t height);

 private:
  Tag(const Tag&) = delete;
  Tag& operator=(const Tag&) = delete;

  std::string* const buffer_;
  size_t fields = 0;

  void NextField();
};

}  // namespace hls
}  // namespace shaka

#endif  // PACKAGER_HLS_BASE_TAG_H_
