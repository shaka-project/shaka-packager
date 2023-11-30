// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/raw_key_source.h>

#include <absl/strings/escaping.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/status/status_test_util.h>

#define EXPECT_HEX_EQ(expected_hex, actual)                          \
  {                                                                  \
    std::string expected_str = absl::HexStringToBytes(expected_hex); \
    std::vector<uint8_t> expected_vector(expected_str.begin(),       \
                                         expected_str.end());        \
    EXPECT_EQ(expected_vector, (actual));                            \
  }

namespace shaka {
namespace media {

namespace {
const char kKeyIdHex[] = "0101020305080d1522375990e9000000";
const char kKeyHex[] = "00100100200300500801302103405500";
const char kKeyId2Hex[] = "1111121315180d1522375990e9000000";
const char kKey2Hex[] = "10201110300300500801302103405500";
const char kIvHex[] = "000102030405060708090a0b0c0d0e0f";
// PSSH boxes generated manually according to PSSH box syntax specified in
// ISO/IEC 23001-7:2016 8.1.2.
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
const char kDrmLabel[] = "SomeDrmLabel";
const char kAnotherDrmLabel[] = "AnotherDrmLabel";
const char kEmptyDrmLabel[] = "";

std::vector<uint8_t> HexStringToVector(const std::string& hex_str) {
  std::string raw_str = absl::HexStringToBytes(hex_str);
  return std::vector<uint8_t>(raw_str.begin(), raw_str.end());
}
}  // namespace

TEST(RawKeySourceTest, Success) {
  RawKeyParams raw_key_params;
  raw_key_params.key_map[kDrmLabel].key_id = HexStringToVector(kKeyIdHex);
  raw_key_params.key_map[kDrmLabel].key = HexStringToVector(kKeyHex);
  raw_key_params.key_map[kEmptyDrmLabel].key_id = HexStringToVector(kKeyId2Hex);
  raw_key_params.key_map[kEmptyDrmLabel].key = HexStringToVector(kKey2Hex);
  raw_key_params.iv = HexStringToVector(kIvHex);
  raw_key_params.pssh =
      HexStringToVector(std::string(kPsshBox1Hex) + kPsshBox2Hex);
  std::unique_ptr<RawKeySource> key_source =
      RawKeySource::Create(raw_key_params);
  ASSERT_NE(nullptr, key_source);

  EncryptionKey key_from_drm_label;
  ASSERT_OK(key_source->GetKey(kDrmLabel, &key_from_drm_label));
  EXPECT_HEX_EQ(kKeyIdHex, key_from_drm_label.key_id);
  EXPECT_HEX_EQ(kKeyHex, key_from_drm_label.key);
  EXPECT_HEX_EQ(kIvHex, key_from_drm_label.iv);
  ASSERT_EQ(2u, key_from_drm_label.key_system_info.size());
  EXPECT_HEX_EQ(kPsshBox1Hex, key_from_drm_label.key_system_info[0].psshs);
  EXPECT_HEX_EQ(kPsshBox2Hex, key_from_drm_label.key_system_info[1].psshs);

  // Using Key ID.
  EncryptionKey key_from_key_id;
  ASSERT_OK(key_source->GetKey(HexStringToVector(kKeyIdHex), &key_from_key_id));
  EXPECT_EQ(key_from_key_id.key_id, key_from_drm_label.key_id);

  // |kAnotherDrmLabel| is not present in the key source, but |kEmptyDrmLabel|
  // is in the key source, the associated key for |kEmptyDrmLabel| will be
  // returned.
  ASSERT_OK(key_source->GetKey(kAnotherDrmLabel, &key_from_drm_label));
  EXPECT_HEX_EQ(kKeyId2Hex, key_from_drm_label.key_id);
  EXPECT_HEX_EQ(kKey2Hex, key_from_drm_label.key);
  EXPECT_HEX_EQ(kIvHex, key_from_drm_label.iv);
  ASSERT_EQ(2u, key_from_drm_label.key_system_info.size());
  EXPECT_HEX_EQ(kPsshBox1Hex, key_from_drm_label.key_system_info[0].psshs);
  EXPECT_HEX_EQ(kPsshBox2Hex, key_from_drm_label.key_system_info[1].psshs);
}

TEST(RawKeySourceTest, Failure) {
  // Invalid key id size.
  RawKeyParams raw_key_params;
  raw_key_params.key_map[kEmptyDrmLabel].key_id =
      HexStringToVector("000102030405");
  raw_key_params.key_map[kEmptyDrmLabel].key = HexStringToVector(kKeyHex);
  raw_key_params.pssh = HexStringToVector(kPsshBox1Hex);
  raw_key_params.iv = HexStringToVector(kIvHex);
  std::unique_ptr<RawKeySource> key_source =
      RawKeySource::Create(raw_key_params);
  EXPECT_EQ(nullptr, key_source);

  // Invalid pssh box.
  raw_key_params.key_map[kEmptyDrmLabel].key_id = HexStringToVector(kKeyIdHex);
  raw_key_params.pssh = HexStringToVector("000102030405");
  key_source = RawKeySource::Create(raw_key_params);
  EXPECT_EQ(nullptr, key_source);
}

}  // namespace media
}  // namespace shaka
