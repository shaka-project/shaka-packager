// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Test helpers for comparing byte vectors against hex strings.

#ifndef PACKAGER_MEDIA_BASE_TEST_HEX_TEST_UTIL_H_
#define PACKAGER_MEDIA_BASE_TEST_HEX_TEST_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include <absl/strings/escaping.h>
#include <gtest/gtest.h>

#define EXPECT_HEX_EQ(expected_hex, actual)                           \
  {                                                                   \
    std::string expected_str;                                         \
    ASSERT_TRUE(absl::HexStringToBytes(expected_hex, &expected_str)); \
    std::vector<uint8_t> expected_vector(expected_str.begin(),        \
                                         expected_str.end());         \
    EXPECT_EQ(expected_vector, (actual));                             \
  }

namespace shaka {
namespace media {

inline std::vector<uint8_t> HexStringToVector(const std::string& hex_str) {
  std::string raw_str;
  EXPECT_TRUE(absl::HexStringToBytes(hex_str, &raw_str));
  return std::vector<uint8_t>(raw_str.begin(), raw_str.end());
}

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TEST_HEX_TEST_UTIL_H_
