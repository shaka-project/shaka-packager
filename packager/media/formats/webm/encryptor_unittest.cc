// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/webm/encryptor.h"

#include <gtest/gtest.h>
#include <memory>
#include "packager/media/base/media_sample.h"
#include "packager/media/formats/webm/webm_constants.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace webm {
namespace {

const uint8_t kKeyId[] = {
    0x4c, 0x6f, 0x72, 0x65, 0x6d, 0x20, 0x69, 0x70,
    0x73, 0x75, 0x6d, 0x20, 0x64, 0x6f, 0x6c, 0x6f,
};
const uint8_t kIv[] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x23, 0x45,
};
// Some dummy data for testing.
const uint8_t kData[] = {0x11, 0x22, 0x33, 0x44, 0x55};
const bool kKeyFrame = true;

}  // namespace

TEST(EncryptionUtilTest, UpdateTrack) {
  unsigned int seed = 0;
  mkvmuxer::VideoTrack video_track(&seed);
  ASSERT_OK(UpdateTrackForEncryption(
      std::vector<uint8_t>(kKeyId, kKeyId + sizeof(kKeyId)), &video_track));
}

TEST(EncryptionUtilTest, UpdateTrackWithEmptyKeyId) {
  unsigned int seed = 0;
  mkvmuxer::VideoTrack video_track(&seed);
  const std::vector<uint8_t> empty_key_id;
  Status status = UpdateTrackForEncryption(empty_key_id, &video_track);
  EXPECT_EQ(error::INTERNAL_ERROR, status.error_code());
}

TEST(EncryptionUtilTest, SampleNotEncrypted) {
  auto sample = MediaSample::CopyFrom(kData, sizeof(kData), kKeyFrame);
  UpdateFrameForEncryption(sample.get());
  ASSERT_EQ(sizeof(kData) + 1, sample->data_size());
  EXPECT_EQ(0u, sample->data()[0]);
  EXPECT_EQ(std::vector<uint8_t>(kData, kData + sizeof(kData)),
            std::vector<uint8_t>(sample->data() + 1,
                                 sample->data() + sample->data_size()));
}

namespace {

const SubsampleEntry kSubsamples1[] = {
    SubsampleEntry(0x12, 0x100),
};
const uint8_t kSubsamplePartitionData1[] = {
    0x01, 0x00, 0x00, 0x00, 0x12,
};
const SubsampleEntry kSubsamples2[] = {
    SubsampleEntry(0x12, 0x100), SubsampleEntry(0x25, 0),
};
const uint8_t kSubsamplePartitionData2[] = {
    0x02, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x01, 0x12,
};
const SubsampleEntry kSubsamples3[] = {
    SubsampleEntry(0x12, 0x100), SubsampleEntry(0x25, 0x8000),
    SubsampleEntry(0x234, 0),
};
const uint8_t kSubsamplePartitionData3[] = {
    0x04, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x01, 0x12,
    0x00, 0x00, 0x01, 0x37, 0x00, 0x00, 0x81, 0x37,
};
const SubsampleEntry kSubsamples4[] = {
    SubsampleEntry(0x12, 0x100), SubsampleEntry(0x25, 0x8000),
    SubsampleEntry(0x234, 0x88000), SubsampleEntry(0x02, 0x20),
};
const uint8_t kSubsamplePartitionData4[] = {
    0x07, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x01, 0x12, 0x00,
    0x00, 0x01, 0x37, 0x00, 0x00, 0x81, 0x37, 0x00, 0x00, 0x83,
    0x6B, 0x00, 0x09, 0x03, 0x6B, 0x00, 0x09, 0x03, 0x6D,
};

}  // namespace

struct EncryptionTestCase {
  const SubsampleEntry* subsamples;
  size_t num_subsamples;
  const uint8_t* subsample_partition_data;
  size_t subsample_partition_data_size;
};

class EncryptionUtilEncryptedTest
    : public ::testing::TestWithParam<EncryptionTestCase> {};

TEST_P(EncryptionUtilEncryptedTest, SampleEncrypted) {
  const EncryptionTestCase& test_case = GetParam();

  auto sample = MediaSample::CopyFrom(kData, sizeof(kData), kKeyFrame);
  sample->set_is_encrypted(true);
  std::unique_ptr<DecryptConfig> decrypt_config(
      new DecryptConfig(std::vector<uint8_t>(kKeyId, kKeyId + sizeof(kKeyId)),
                        std::vector<uint8_t>(kIv, kIv + sizeof(kIv)),
                        std::vector<SubsampleEntry>(
                            test_case.subsamples,
                            test_case.subsamples + test_case.num_subsamples)));
  sample->set_decrypt_config(std::move(decrypt_config));

  UpdateFrameForEncryption(sample.get());
  ASSERT_EQ(
      sizeof(kData) + sizeof(kIv) + test_case.subsample_partition_data_size + 1,
      sample->data_size());
  if (test_case.num_subsamples > 0)
    EXPECT_EQ(kWebMEncryptedSignal | kWebMPartitionedSignal, sample->data()[0]);
  else
    EXPECT_EQ(kWebMEncryptedSignal, sample->data()[0]);
  EXPECT_EQ(std::vector<uint8_t>(kIv, kIv + sizeof(kIv)),
            std::vector<uint8_t>(sample->data() + 1,
                                 sample->data() + 1 + sizeof(kIv)));
  EXPECT_EQ(std::vector<uint8_t>(test_case.subsample_partition_data,
                                 test_case.subsample_partition_data +
                                     test_case.subsample_partition_data_size),
            std::vector<uint8_t>(sample->data() + 1 + sizeof(kIv),
                                 sample->data() + 1 + sizeof(kIv) +
                                     test_case.subsample_partition_data_size));
  EXPECT_EQ(std::vector<uint8_t>(kData, kData + sizeof(kData)),
            std::vector<uint8_t>(sample->data() + 1 + sizeof(kIv) +
                                     test_case.subsample_partition_data_size,
                                 sample->data() + sample->data_size()));
}

namespace {
EncryptionTestCase kEncryptionTestCases[] = {
    // Special case with no subsamples.
    {nullptr, 0, nullptr, 0},
    {kSubsamples1, arraysize(kSubsamples1), kSubsamplePartitionData1,
     arraysize(kSubsamplePartitionData1)},
    {kSubsamples2, arraysize(kSubsamples2), kSubsamplePartitionData2,
     arraysize(kSubsamplePartitionData2)},
    {kSubsamples3, arraysize(kSubsamples3), kSubsamplePartitionData3,
     arraysize(kSubsamplePartitionData3)},
    {kSubsamples4, arraysize(kSubsamples4), kSubsamplePartitionData4,
     arraysize(kSubsamplePartitionData4)},
};
}  // namespace

INSTANTIATE_TEST_CASE_P(Encryption,
                        EncryptionUtilEncryptedTest,
                        testing::ValuesIn(kEncryptionTestCases));

}  // namespace webm
}  // namespace media
}  // namespace shaka
