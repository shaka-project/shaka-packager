// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/utils/hex_parser.h"

#include "absl/strings/escaping.h"

namespace shaka {

bool ValidHexStringToBytes(const std::string& hex, std::string* bytes) {
  // absl::HexStringToBytes will not validate the string!  Any invalid byte
  // sequence will be converted into NUL characters silently.  So we do our own
  // validation here.
  for (char c : hex) {
    c = std::tolower(c);
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
