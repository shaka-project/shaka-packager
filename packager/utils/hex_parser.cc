// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/utils/hex_parser.h>

#include <absl/strings/escaping.h>
#include <absl/types/span.h>

namespace shaka {

void HexStringToBytes(const std::string& hex, std::vector<uint8_t>* bytes) {
  std::string raw = absl::HexStringToBytes(hex);

  absl::string_view str_view(raw);
  absl::Span<const uint8_t> span(
      reinterpret_cast<const uint8_t*>(str_view.data()), str_view.size());
  *bytes = std::vector<uint8_t>(span.begin(), span.end());
}

bool ValidHexStringToBytes(const std::string& hex,
                           std::vector<uint8_t>* bytes) {
  std::string raw;
  if (!ValidHexStringToBytes(hex, &raw))
    return false;

  absl::string_view str_view(raw);
  absl::Span<const uint8_t> span(
      reinterpret_cast<const uint8_t*>(str_view.data()), str_view.size());
  *bytes = std::vector<uint8_t>(span.begin(), span.end());
  return true;
}

bool ValidHexStringToBytes(const std::string& hex, std::string* bytes) {
  // absl::HexStringToBytes will not validate the string!  Any invalid byte
  // sequence will be converted into NUL characters silently.  So we do our own
  // validation here.
  for (char c : hex) {
    c = static_cast<char>(std::tolower(c));
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
      // valid
    } else {
      return false;
    }
  }

  *bytes = absl::HexStringToBytes(hex);
  return true;
}

}  // namespace shaka
