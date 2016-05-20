// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/macros.h"
#include "packager/media/base/protection_system_specific_info.h"

namespace shaka {
namespace media {

namespace {
const uint8_t kV0BoxArray[] = {
  0x00, 0x00, 0x00, 0x21, 'p', 's', 's', 'h',      // Header
  0x00, 0x00, 0x00, 0x00,                          // Version = 0, flags = 0
  0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,  // System ID
  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
  0x00, 0x00, 0x00, 0x01,                          // Data size(1)
  0xFF
};
const uint8_t kV1BoxArray[] = {
  0x00, 0x00, 0x00, 0x35, 'p', 's', 's', 'h',      // Header
  0x01, 0x00, 0x00, 0x00,                          // Version = 1, flags = 0
  0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,  // System ID
  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
  0x00, 0x00, 0x00, 0x01,                          // KID_count(1)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First KID
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01,                          // Data size(1)
  0xFF
};

const uint8_t kTestSystemIdArray[] = {
  0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,  // System ID
  0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
};
const uint8_t kTestKeyIdArray[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First KID
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t kTestPsshDataArray[] = {0xFF};
}  // namespace

class PsshTest : public ::testing::Test {
 public:
  PsshTest()
      : v0_box_(kV0BoxArray, kV0BoxArray + arraysize(kV0BoxArray)),
        v1_box_(kV1BoxArray, kV1BoxArray + arraysize(kV1BoxArray)),
        test_system_id_(kTestSystemIdArray,
                        kTestSystemIdArray + arraysize(kTestSystemIdArray)),
        test_key_id_(kTestKeyIdArray,
                     kTestKeyIdArray + arraysize(kTestKeyIdArray)),
        test_pssh_data_(kTestPsshDataArray,
                        kTestPsshDataArray + arraysize(kTestPsshDataArray)) {}

  const std::vector<uint8_t> v0_box_;
  const std::vector<uint8_t> v1_box_;
  const std::vector<uint8_t> test_system_id_;
  const std::vector<uint8_t> test_key_id_;
  const std::vector<uint8_t> test_pssh_data_;
};

TEST_F(PsshTest, ParseBoxes_SupportsV0) {
  std::vector<ProtectionSystemSpecificInfo> info;
  ASSERT_TRUE(ProtectionSystemSpecificInfo::ParseBoxes(
      v0_box_.data(), v0_box_.size(), &info));
  ASSERT_EQ(1u, info.size());

  ASSERT_EQ(0u, info[0].key_ids().size());
  EXPECT_EQ(test_system_id_, info[0].system_id());
  EXPECT_EQ(test_pssh_data_, info[0].pssh_data());
  EXPECT_EQ(0, info[0].pssh_box_version());
}

TEST_F(PsshTest, ParseBoxes_SupportsV1) {
  std::vector<ProtectionSystemSpecificInfo> info;
  ASSERT_TRUE(ProtectionSystemSpecificInfo::ParseBoxes(
      v1_box_.data(), v1_box_.size(), &info));
  ASSERT_EQ(1u, info.size());

  ASSERT_EQ(1u, info[0].key_ids().size());
  EXPECT_EQ(test_system_id_, info[0].system_id());
  EXPECT_EQ(test_key_id_, info[0].key_ids()[0]);
  EXPECT_EQ(test_pssh_data_, info[0].pssh_data());
  EXPECT_EQ(1, info[0].pssh_box_version());
}

TEST_F(PsshTest, ParseBoxes_SupportsConcatenatedBoxes) {
  std::vector<uint8_t> data;
  data.insert(data.end(), v1_box_.begin(), v1_box_.end());
  data.insert(data.end(), v0_box_.begin(), v0_box_.end());
  data.insert(data.end(), v1_box_.begin(), v1_box_.end());

  std::vector<ProtectionSystemSpecificInfo> info;
  ASSERT_TRUE(ProtectionSystemSpecificInfo::ParseBoxes(data.data(),
                                                         data.size(), &info));
  ASSERT_EQ(3u, info.size());

  ASSERT_EQ(1u, info[0].key_ids().size());
  EXPECT_EQ(test_system_id_, info[0].system_id());
  EXPECT_EQ(test_key_id_, info[0].key_ids()[0]);
  EXPECT_EQ(test_pssh_data_, info[0].pssh_data());
  EXPECT_EQ(1, info[0].pssh_box_version());

  ASSERT_EQ(0u, info[1].key_ids().size());
  EXPECT_EQ(test_system_id_, info[1].system_id());
  EXPECT_EQ(test_pssh_data_, info[1].pssh_data());
  EXPECT_EQ(0, info[1].pssh_box_version());

  ASSERT_EQ(1u, info[2].key_ids().size());
  EXPECT_EQ(test_system_id_, info[2].system_id());
  EXPECT_EQ(test_key_id_, info[2].key_ids()[0]);
  EXPECT_EQ(test_pssh_data_, info[2].pssh_data());
  EXPECT_EQ(1, info[2].pssh_box_version());
}

TEST_F(PsshTest, CreateBox_MakesV0Boxes) {
  ProtectionSystemSpecificInfo info;
  info.set_system_id(kTestSystemIdArray, arraysize(kTestSystemIdArray));
  info.set_pssh_data(test_pssh_data_);
  info.set_pssh_box_version(0);

  EXPECT_EQ(v0_box_, info.CreateBox());
}

TEST_F(PsshTest, CreateBox_MakesV1Boxes) {
  ProtectionSystemSpecificInfo info;
  info.add_key_id(test_key_id_);
  info.set_system_id(kTestSystemIdArray, arraysize(kTestSystemIdArray));
  info.set_pssh_data(test_pssh_data_);
  info.set_pssh_box_version(1);

  EXPECT_EQ(v1_box_, info.CreateBox());
}

}  // namespace media
}  // namespace shaka
