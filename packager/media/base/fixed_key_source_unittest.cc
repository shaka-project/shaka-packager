// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/strings/string_number_conversions.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/test/status_test_util.h"

#define EXPECT_HEX_EQ(expected, actual)                         \
  do {                                                          \
    std::vector<uint8_t> decoded_;                              \
    ASSERT_TRUE(base::HexStringToBytes((expected), &decoded_)); \
    EXPECT_EQ(decoded_, (actual));                              \
  } while (false)

namespace shaka {
namespace media {

namespace {
const char kKeyIdHex[] =   "0101020305080d1522375990e9000000";
const char kKeyHex[] =     "00100100200300500801302103405500";
const char kIvHex[] = "000102030405060708090a0b0c0d0e0f";
const char kPsshBox1Hex[] =
  "000000427073736801000000"
  "020305070b0d1113171d1f25292b2f35"
  "00000001"
  "0101020305080d1522375990e9000000"
  "0000000e"
  "6544617368207061636b61676572";
const char kPsshBox2Hex[] =
  "0000002e7073736800000000"
  "bbbbbbbbbaaaaaaaaaaaaddddddddddd"
  "0000000e"
  "fffffffff000000000000ddddddd";
const char kDefaultPsshBoxHex[] =
  "000000347073736801000000"
  "1077efecc0b24d02ace33c1e52e2fb4b"
  "00000001"
  "0101020305080d1522375990e9000000"
  "00000000";
}

TEST(FixedKeySourceTest, CreateFromHexStrings_Succes) {
  std::string pssh_boxes = std::string(kPsshBox1Hex) + kPsshBox2Hex;
  std::unique_ptr<FixedKeySource> key_source =
      FixedKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, pssh_boxes,
                                           kIvHex);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("SomeStreamLabel", &key));

  EXPECT_HEX_EQ(kKeyIdHex, key.key_id);
  EXPECT_HEX_EQ(kKeyHex, key.key);
  EXPECT_HEX_EQ(kIvHex, key.iv);

  ASSERT_EQ(2u, key.key_system_info.size());
  EXPECT_HEX_EQ(kPsshBox1Hex, key.key_system_info[0].CreateBox());
  EXPECT_HEX_EQ(kPsshBox2Hex, key.key_system_info[1].CreateBox());
}

TEST(FixedKeySourceTest, CreateFromHexStrings_EmptyPssh) {
  std::unique_ptr<FixedKeySource> key_source =
      FixedKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, "", kIvHex);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key;
  ASSERT_OK(key_source->GetKey("SomeStreamLabel", &key));

  EXPECT_HEX_EQ(kKeyIdHex, key.key_id);
  EXPECT_HEX_EQ(kKeyHex, key.key);
  EXPECT_HEX_EQ(kIvHex, key.iv);

  ASSERT_EQ(1u, key.key_system_info.size());
  EXPECT_HEX_EQ(kDefaultPsshBoxHex, key.key_system_info[0].CreateBox());
}

TEST(FixedKeySourceTest, CreateFromHexStrings_Failure) {
  std::unique_ptr<FixedKeySource> key_source =
      FixedKeySource::CreateFromHexStrings(kKeyIdHex, "invalid_hex_value",
                                           kPsshBox1Hex, kIvHex);
  EXPECT_EQ(nullptr, key_source);

  // Invalid key id size.
  key_source = FixedKeySource::CreateFromHexStrings("000102030405", kKeyHex,
                                                    kPsshBox1Hex, kIvHex);
  EXPECT_EQ(nullptr, key_source);

  // Invalid pssh box.
  key_source = FixedKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex,
                                                    "000102030405", kIvHex);
  EXPECT_EQ(nullptr, key_source);
}

}  // namespace media
}  // namespace shaka
