// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/utils/hex_parser.h>

#include <absl/strings/escaping.h>
#include <absl/types/span.h>

namespace shaka {

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
  // absl::HexStringToBytes validates the input during processing and
  // aborts on invalid data, leaving "bytes" in an unspecified state.
  return absl::HexStringToBytes(hex, bytes);
}

}  // namespace shaka
