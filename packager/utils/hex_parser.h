// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_UTILS_HEX_PARSER_H_
#define PACKAGER_UTILS_HEX_PARSER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace shaka {

void HexStringToBytes(const std::string& hex, std::vector<uint8_t>* bytes);

// If you use absl::HexStringToBytes directly, any invalid byte sequence will
// be converted into NUL characters silently.  This function will validate the
// input.
bool ValidHexStringToBytes(const std::string& hex, std::string* bytes);

// same but output to a vector of uint8_t
bool ValidHexStringToBytes(const std::string& hex, std::vector<uint8_t>* bytes);

}  // namespace shaka
#endif  // PACKAGER_UTILS_HEX_PARSER_H_
