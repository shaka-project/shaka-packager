// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/utils/string_trim_split.h>

#include <absl/strings/str_split.h>

namespace shaka {
std::vector<std::string> SplitAndTrimSkipEmpty(const std::string& str,
                                               char delimiter) {
  auto tokens = absl::StrSplit(str, delimiter, absl::SkipEmpty());
  std::vector<std::string> results;
  for (const absl::string_view& token : tokens) {
    std::string trimmed = std::string(token);
    absl::StripAsciiWhitespace(&trimmed);
    if (!trimmed.empty()) {
      results.push_back(trimmed);
    }
  }

  return results;
}
}  // namespace shaka
