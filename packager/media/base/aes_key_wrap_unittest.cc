// Copyright 2026 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/aes_key_wrap.h>

#include <cstdint>
#include <string>
#include <vector>

#include <absl/strings/escaping.h>
#include <gtest/gtest.h>

namespace shaka {
namespace media {
namespace {

std::vector<uint8_t> HexStringToVector(const std::string& hex_str) {
  std::string raw_str;
  EXPECT_TRUE(absl::HexStringToBytes(hex_str, &raw_str));
  return std::vector<uint8_t>(raw_str.begin(), raw_str.end());
}

// Test vectors from RFC 3394 section 4.
// Section 4.1: 128 bits of key data with a 128-bit KEK.
const char kKek128Hex[] = "000102030405060708090a0b0c0d0e0f";
const char kData128Hex[] = "00112233445566778899aabbccddeeff";
const char kWrapped128With128Hex[] =
    "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5";
// Section 4.3: 128 bits of key data with a 256-bit KEK.
const char kKek256Hex[] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
const char kWrapped128With256Hex[] =
    "64e8c3f9ce0f5ba263e9777905818a2a93c8191e7d6e8ae7";

}  // namespace

TEST(AesKeyWrapTest, WrapRfc3394Vectors) {
  std::vector<uint8_t> wrapped;
  ASSERT_TRUE(AesKeyWrap(HexStringToVector(kKek128Hex),
                         HexStringToVector(kData128Hex), &wrapped));
  EXPECT_EQ(HexStringToVector(kWrapped128With128Hex), wrapped);

  ASSERT_TRUE(AesKeyWrap(HexStringToVector(kKek256Hex),
                         HexStringToVector(kData128Hex), &wrapped));
  EXPECT_EQ(HexStringToVector(kWrapped128With256Hex), wrapped);
}

TEST(AesKeyWrapTest, UnwrapRfc3394Vectors) {
  std::vector<uint8_t> data;
  ASSERT_TRUE(AesKeyUnwrap(HexStringToVector(kKek128Hex),
                           HexStringToVector(kWrapped128With128Hex), &data));
  EXPECT_EQ(HexStringToVector(kData128Hex), data);

  ASSERT_TRUE(AesKeyUnwrap(HexStringToVector(kKek256Hex),
                           HexStringToVector(kWrapped128With256Hex), &data));
  EXPECT_EQ(HexStringToVector(kData128Hex), data);
}

TEST(AesKeyWrapTest, UnwrapWithWrongKeyFails) {
  std::vector<uint8_t> data;
  EXPECT_FALSE(AesKeyUnwrap(HexStringToVector(kKek128Hex),
                            HexStringToVector(kWrapped128With256Hex), &data));
}

TEST(AesKeyWrapTest, UnwrapTamperedDataFails) {
  std::vector<uint8_t> wrapped = HexStringToVector(kWrapped128With128Hex);
  wrapped[3] ^= 0xff;
  std::vector<uint8_t> data;
  EXPECT_FALSE(AesKeyUnwrap(HexStringToVector(kKek128Hex), wrapped, &data));
}

TEST(AesKeyWrapTest, InvalidKeySizeFails) {
  std::vector<uint8_t> output;
  EXPECT_FALSE(AesKeyWrap(HexStringToVector("0102"),
                          HexStringToVector(kData128Hex), &output));
}

}  // namespace media
}  // namespace shaka
