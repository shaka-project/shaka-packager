// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef SHAKA_PACKAGER_ABSL_FLAG_HEXBYTES_H
#define SHAKA_PACKAGER_ABSL_FLAG_HEXBYTES_H

#include <cstdint>

#include <absl/flags/flag.h>
#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>

#include <packager/utils/hex_parser.h>

// Custom flag type for hexadecimal byte array
namespace shaka {

struct HexBytes {
  std::vector<uint8_t> bytes;
};

// Custom flag parser for HexBytes
bool AbslParseFlag(absl::string_view text, HexBytes* flag, std::string* error);
std::string AbslUnparseFlag(const HexBytes& flag);

}  // namespace shaka

#endif  // SHAKA_PACKAGER_ABSL_FLAG_HEXBYTES_H
