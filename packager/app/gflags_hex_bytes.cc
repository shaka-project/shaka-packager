// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/gflags_hex_bytes.h"

#include "packager/base/strings/string_number_conversions.h"

namespace shaka {

bool ValidateHexString(const char* flagname,
                       const std::string& value,
                       std::vector<uint8_t>* value_bytes) {
  std::vector<uint8_t> temp_value_bytes;
  if (!value.empty() && !base::HexStringToBytes(value, &temp_value_bytes)) {
    printf("Invalid hex string for --%s: %s\n", flagname, value.c_str());
    return false;
  }
  value_bytes->swap(temp_value_bytes);
  return true;
}

}  // namespace shaka
