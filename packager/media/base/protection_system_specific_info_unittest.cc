// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/protection_system_specific_info.h>

#include <iterator>

#include <gtest/gtest.h>

namespace shaka {
namespace media {

namespace {
const uint8_t kSystemId1V0BoxArray[] = {
    0x00, 0x00, 0x00, 0x21, 'p',  's',  's',  'h',   // Header
    0x00, 0x00, 0x00, 0x00,                          // Version = 0, flags = 0
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,  // System ID
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
    0x00, 0x00, 0x00, 0x01,  // Data size(1)
    0xFF,
};
const uint8_t kSystemId1V1BoxArray[] = {
    0x00, 0x00, 0x00, 0x35, 'p',  's',  's',  'h',   // Header
    0x01, 0x00, 0x00, 0x00,                          // Version = 1, flags = 0
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,  // System ID
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
    0x00, 0x00, 0x00, 0x01,                          // KID_count(1)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // First KID
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01,  // Data size(1)
    0xFF,
};
const uint8_t kSystemId2V0BoxArray[] = {
    0x00, 0x00, 0x00, 0x21, 'p',  's',  's',  'h',   // Header
    0x00, 0x00, 0x00, 0x00,                          // Version = 0, flags = 0
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,  // System ID
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
    0x00, 0x00, 0x00, 0x01,  // Data size(1)
    0xFF,
};

const uint8_t kTestSystemId1Array[] = {
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
};
const uint8_t kTestSystemId2Array[] = {
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
    0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11,
};
const uint8_t kTestKeyIdArray[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const uint8_t kTestPsshDataArray[] = {0xFF};

std::vector<uint8_t> ConcatVectors(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b) {
  std::vector<uint8_t> out;
  out.insert(out.end(), a.begin(), a.end());
  out.insert(out.end(), b.begin(), b.end());
  return out;
}

std::vector<uint8_t> ConcatVectors(const std::vector<uint8_t>& a,
                                   const std::vector<uint8_t>& b,
                                   const std::vector<uint8_t>& c) {
  return ConcatVectors(ConcatVectors(a, b), c);
}

}  // namespace

class PsshTest : public ::testing::Test {
 public:
  PsshTest()
      : system_id1_v0_box_(std::begin(kSystemId1V0BoxArray),
                           std::end(kSystemId1V0BoxArray)),
        system_id1_v1_box_(std::begin(kSystemId1V1BoxArray),
                           std::end(kSystemId1V1BoxArray)),
        system_id2_v0_box_(std::begin(kSystemId2V0BoxArray),
                           std::end(kSystemId2V0BoxArray)),
        test_system_id1_(std::begin(kTestSystemId1Array),
                         std::end(kTestSystemId1Array)),
        test_system_id2_(std::begin(kTestSystemId2Array),
                         std::end(kTestSystemId2Array)),
        test_key_id_(std::begin(kTestKeyIdArray), std::end(kTestKeyIdArray)),
        test_pssh_data_(std::begin(kTestPsshDataArray),
                        std::end(kTestPsshDataArray)) {}

  const std::vector<uint8_t> system_id1_v0_box_;
  const std::vector<uint8_t> system_id1_v1_box_;
  const std::vector<uint8_t> system_id2_v0_box_;
  const std::vector<uint8_t> test_system_id1_;
  const std::vector<uint8_t> test_system_id2_;
  const std::vector<uint8_t> test_key_id_;
  const std::vector<uint8_t> test_pssh_data_;
};

TEST_F(PsshTest, ParseBoxes_SupportsV0) {
  std::vector<ProtectionSystemSpecificInfo> info;
  ASSERT_TRUE(ProtectionSystemSpecificInfo::ParseBoxes(
      system_id1_v0_box_.data(), system_id1_v0_box_.size(), &info));

  ASSERT_EQ(1u, info.size());
  EXPECT_EQ(test_system_id1_, info[0].system_id);

  std::unique_ptr<PsshBoxBuilder> pssh_builder =
      PsshBoxBuilder::ParseFromBox(info[0].psshs.data(), info[0].psshs.size());
  ASSERT_TRUE(pssh_builder);

  ASSERT_EQ(0u, pssh_builder->key_ids().size());
  EXPECT_EQ(test_system_id1_, pssh_builder->system_id());
  EXPECT_EQ(test_pssh_data_, pssh_builder->pssh_data());
  EXPECT_EQ(0, pssh_builder->pssh_box_version());
}

TEST_F(PsshTest, ParseBoxes_SupportsV1) {
  std::vector<ProtectionSystemSpecificInfo> info;
  ASSERT_TRUE(ProtectionSystemSpecificInfo::ParseBoxes(
      system_id1_v1_box_.data(), system_id1_v1_box_.size(), &info));

  ASSERT_EQ(1u, info.size());
  EXPECT_EQ(test_system_id1_, info[0].system_id);

  std::unique_ptr<PsshBoxBuilder> pssh_builder =
      PsshBoxBuilder::ParseFromBox(info[0].psshs.data(), info[0].psshs.size());
  ASSERT_TRUE(pssh_builder);

  ASSERT_EQ(1u, pssh_builder->key_ids().size());
  EXPECT_EQ(test_system_id1_, pssh_builder->system_id());
  EXPECT_EQ(test_key_id_, pssh_builder->key_ids()[0]);
  EXPECT_EQ(test_pssh_data_, pssh_builder->pssh_data());
  EXPECT_EQ(1, pssh_builder->pssh_box_version());
}

TEST_F(PsshTest, ParseBoxes_SupportsConcatenatedBoxes) {
  std::vector<uint8_t> data =
      ConcatVectors(system_id1_v0_box_, system_id2_v0_box_, system_id1_v1_box_);

  std::vector<ProtectionSystemSpecificInfo> info;
  ASSERT_TRUE(ProtectionSystemSpecificInfo::ParseBoxes(data.data(), data.size(),
                                                       &info));
  // The PSSHs are grouped by system id. Since there are only two system ids,
  // there are two ProtectionSystemSpecificInfo.
  ASSERT_EQ(2u, info.size());
  EXPECT_EQ(ConcatVectors(system_id1_v0_box_, system_id1_v1_box_),
            info[0].psshs);
  EXPECT_EQ(system_id2_v0_box_, info[1].psshs);

  std::unique_ptr<PsshBoxBuilder> pssh_builder =
      PsshBoxBuilder::ParseFromBox(info[0].psshs.data(), info[0].psshs.size());
  ASSERT_TRUE(pssh_builder);

  ASSERT_EQ(0u, pssh_builder->key_ids().size());
  EXPECT_EQ(test_system_id1_, pssh_builder->system_id());
  EXPECT_EQ(test_pssh_data_, pssh_builder->pssh_data());
  EXPECT_EQ(0, pssh_builder->pssh_box_version());

  pssh_builder =
      PsshBoxBuilder::ParseFromBox(info[1].psshs.data(), info[1].psshs.size());
  ASSERT_TRUE(pssh_builder);

  ASSERT_EQ(0u, pssh_builder->key_ids().size());
  EXPECT_EQ(test_system_id2_, pssh_builder->system_id());
  EXPECT_EQ(test_pssh_data_, pssh_builder->pssh_data());
  EXPECT_EQ(0, pssh_builder->pssh_box_version());
}

TEST_F(PsshTest, CreateBox_MakesV0Boxes) {
  PsshBoxBuilder pssh_builder;
  pssh_builder.set_system_id(kTestSystemId1Array,
                             std::size(kTestSystemId1Array));
  pssh_builder.set_pssh_data(test_pssh_data_);
  pssh_builder.set_pssh_box_version(0);

  EXPECT_EQ(system_id1_v0_box_, pssh_builder.CreateBox());
}

TEST_F(PsshTest, CreateBox_MakesV1Boxes) {
  PsshBoxBuilder pssh_builder;
  pssh_builder.add_key_id(test_key_id_);
  pssh_builder.set_system_id(kTestSystemId1Array,
                             std::size(kTestSystemId1Array));
  pssh_builder.set_pssh_data(test_pssh_data_);
  pssh_builder.set_pssh_box_version(1);

  EXPECT_EQ(system_id1_v1_box_, pssh_builder.CreateBox());
}

}  // namespace media
}  // namespace shaka
