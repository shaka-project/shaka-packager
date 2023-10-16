// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/utils/absl_flag_hexbytes.h>

#include <iostream>
#include <vector>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/str_format.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include <packager/utils/hex_parser.h>

namespace shaka {

// Custom flag parser for HexBytes
bool AbslParseFlag(absl::string_view text, HexBytes* flag, std::string* error) {
  std::string hexString(text);
  absl::RemoveExtraAsciiWhitespace(&hexString);

  if (hexString.empty()) {
    flag->bytes = std::vector<uint8_t>(0, 0);
    return true;
  }

  std::string bytesRaw;
  if (!shaka::ValidHexStringToBytes(hexString, &bytesRaw)) {
    *error = "Invalid hex string";
    return false;
  }

  absl::string_view hex_str_view(bytesRaw);
  absl::Span<const uint8_t> span(
      reinterpret_cast<const uint8_t*>(hex_str_view.data()),
      hex_str_view.size());
  flag->bytes = std::vector<uint8_t>(span.begin(), span.end());

  return true;
}

std::string AbslUnparseFlag(const HexBytes& flag) {
  std::string result;
  for (const auto byte : flag.bytes) {
    absl::StrAppendFormat(&result, "%02x", byte);
  }
  return result;
}

}  // namespace shaka
