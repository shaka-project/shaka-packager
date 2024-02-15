// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/codecs/es_descriptor.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/media/base/buffer_writer.h>

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

namespace shaka {
namespace media {

TEST(ESDescriptorTest, SingleByteLengthTest) {
  // clang-format off
  const uint8_t kBuffer[] = {
      // ESDescriptor tag with one byte size.
      0x03, 0x19,
        // ESDescriptor fields.
        0x00, 0x00, 0x00,
        // DecoderConfigDescriptor tag with one byte size.
        0x04, 0x11,
          // Object Type.
          0x40,
          // Three 4-byte fields: dummy, max bitrate, avg bitrate.
          0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          // DecoderSpecificInfo tag with one byte size.
          0x05, 0x02,
            // DecoderSpecificInfo fields.
            0x12, 0x10,
        // SLConfig tag with one byte size.
        0x06, 0x01,
          // SLConfig fields.
          0x02,
  };
  // clang-format on
  std::vector<uint8_t> data(std::begin(kBuffer), std::end(kBuffer));

  ESDescriptor es_desc;
  const DecoderConfigDescriptor& decoder_config_descriptor =
      es_desc.decoder_config_descriptor();
  EXPECT_EQ(decoder_config_descriptor.object_type(), ObjectType::kForbidden);
  EXPECT_TRUE(es_desc.Parse(data));

  EXPECT_EQ(decoder_config_descriptor.object_type(), ObjectType::kISO_14496_3);
  EXPECT_THAT(
      decoder_config_descriptor.decoder_specific_info_descriptor().data(),
      ElementsAre(0x12, 0x10));

  BufferWriter writer;
  es_desc.Write(&writer);
  EXPECT_THAT(
      std::vector<uint8_t>(writer.Buffer(), writer.Buffer() + writer.Size()),
      ElementsAreArray(kBuffer));
  
  EXPECT_EQ(0u, es_desc.esid());
  const size_t kEsIdOffset = 3;
  const uint8_t kEsId = 5;
  data[kEsIdOffset] = kEsId;
  ASSERT_TRUE(es_desc.Parse(data));
  EXPECT_EQ(kEsId, es_desc.esid());

  writer.Clear();
  es_desc.Write(&writer);
  EXPECT_THAT(
      std::vector<uint8_t>(writer.Buffer(), writer.Buffer() + writer.Size()),
      ElementsAreArray(kBuffer));
}

TEST(ESDescriptorTest, NonAACTest) {
  // clang-format off
  const uint8_t kBuffer[] = {
      // ESDescriptor tag with one byte size.
      0x03, 0x19,
        // ESDescriptor fields.
        0x00, 0x00, 0x00,
        // DecoderConfigDescriptor tag with one byte size.
        0x04, 0x11,
          // Object Type.
          0x66,
          // Three 4-byte fields: dummy, max bitrate, avg bitrate.
          0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          // DecoderSpecificInfo tag with one byte size.
          0x05, 0x02,
            // DecoderSpecificInfo fields.
            0x12, 0x10,
        // SLConfig tag with one byte size.
        0x06, 0x01,
          // SLConfig fields.
          0x02,
  };
  // clang-format on
  std::vector<uint8_t> data(std::begin(kBuffer), std::end(kBuffer));

  ESDescriptor es_desc;
  EXPECT_TRUE(es_desc.Parse(data));

  const DecoderConfigDescriptor& decoder_config_descriptor =
      es_desc.decoder_config_descriptor();
  EXPECT_EQ(static_cast<int>(decoder_config_descriptor.object_type()), 0x66);
  EXPECT_NE(decoder_config_descriptor.object_type(), ObjectType::kISO_14496_3);
  EXPECT_THAT(
      decoder_config_descriptor.decoder_specific_info_descriptor().data(),
      ElementsAre(0x12, 0x10));

  BufferWriter writer;
  es_desc.Write(&writer);
  EXPECT_THAT(
      std::vector<uint8_t>(writer.Buffer(), writer.Buffer() + writer.Size()),
      ElementsAreArray(kBuffer));
}

TEST(ESDescriptorTest, NonAACWithoutDecoderSpecificInfoTagTest) {
  // clang-format off
  const uint8_t kBuffer[] = {
      // ESDescriptor tag with one byte size.
      0x03, 0x15,
        // ESDescriptor fields.
        0x00, 0x00, 0x00,
        // DecoderConfigDescriptor tag with one byte size.
        0x04, 0x0d,
          // Object Type.
          0x6b,
          // Three 4-byte fields: dummy, max bitrate, avg bitrate.
          0x15, 0x00, 0x01, 0xe0, 0x00, 0x02, 0x85, 0x00, 0x00, 0x02, 0x71, 0x00,
        // SLConfig tag with one byte size.
        0x06, 0x01,
          // SLConfig fields.
          0x02,
  };
  // clang-format on
  std::vector<uint8_t> data(std::begin(kBuffer), std::end(kBuffer));

  ESDescriptor es_desc;
  EXPECT_TRUE(es_desc.Parse(data));

  const DecoderConfigDescriptor& decoder_config_descriptor =
      es_desc.decoder_config_descriptor();
  EXPECT_EQ(static_cast<int>(decoder_config_descriptor.object_type()), 0x6b);
  EXPECT_EQ(decoder_config_descriptor.max_bitrate(), 0x28500u);
  EXPECT_EQ(decoder_config_descriptor.avg_bitrate(), 0x27100u);
  EXPECT_THAT(
      decoder_config_descriptor.decoder_specific_info_descriptor().data(),
      ElementsAre());
}

// https://github.com/shaka-project/shaka-packager/issues/536.
TEST(ESDescriptorTest, Issue536) {
  // clang-format off
  const uint8_t kInput[] = {
      // ESDescriptor tag with size.
      0x03, 0x80, 0x80, 0x80, 0x70,
        // ESDescriptor fields.
        0x00, 0x00, 0x00,
        // DecoderConfigDescriptor tag with size.
        0x04, 0x80, 0x80, 0x80, 0x62,
          // Object Type.
          0x40,
          // Three 4-byte fields: dummy, max bitrate, avg bitrate.
          0x15, 0x00, 0x30, 0x00, 0x00, 0x01, 0xF4, 0x00,
          0x00, 0x01, 0xF4, 0x00,
          // DecoderSpecificInfo tag with size.
          0x05, 0x80, 0x80, 0x80, 0x50,
            // DecoderSpecificInfo fields.
            0x11, 0x90, 0x08, 0xC4, 0x00, 0x00, 0x20, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // SLConfig tag with size.
        0x06, 0x80, 0x80, 0x80, 0x01,
          // SLConfig fields.
          0x02,
  };
  const uint8_t kOutput[] = {
      // ESDescriptor tag with size.
      0x03, 0x67,
        // ESDescriptor fields.
        0x00, 0x00, 0x00,
        // DecoderConfigDescriptor tag with size.
        0x04, 0x5F,
          // Object Type.
          0x40,
          // Three 4-byte fields: dummy, max bitrate, avg bitrate.
          0x15, 0x00, 0x30, 0x00, 0x00, 0x01, 0xF4, 0x00,
          0x00, 0x01, 0xF4, 0x00,
          // DecoderSpecificInfo tag with size.
          0x05, 0x50,
            // DecoderSpecificInfo fields.
            0x11, 0x90, 0x08, 0xC4, 0x00, 0x00, 0x20, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // SLConfig tag with size.
        0x06, 0x01,
          // SLConfig fields.
          0x02,
  };
  // clang-format on
  std::vector<uint8_t> data(std::begin(kInput), std::end(kInput));

  ESDescriptor es_desc;
  EXPECT_TRUE(es_desc.Parse(data));

  BufferWriter writer;
  es_desc.Write(&writer);
  EXPECT_THAT(
      std::vector<uint8_t>(writer.Buffer(), writer.Buffer() + writer.Size()),
      ElementsAreArray(kOutput));
}

class DescriptorLengthTest : public testing::Test {
 public:
  void TestReadWrite(const std::vector<uint8_t>& input,
                     const std::vector<uint8_t>& expected_output) {
    DecoderSpecificInfoDescriptor desc;
    EXPECT_TRUE(desc.Parse(input));

    BufferWriter writer;
    desc.Write(&writer);
    EXPECT_THAT(
        std::vector<uint8_t>(writer.Buffer(), writer.Buffer() + writer.Size()),
        ElementsAreArray(expected_output));
  }
};

// Use DecoderSpecificInfo descriptor for length testing.

TEST_F(DescriptorLengthTest, OneByteLengthData) {
  const uint8_t kBuffer[] = {0x05, 0x02, 0x12, 0x10};
  std::vector<uint8_t> data(std::begin(kBuffer), std::end(kBuffer));
  TestReadWrite(data, data);
}

TEST_F(DescriptorLengthTest, TwoBytesLengthForOneByteLengthData) {
  const uint8_t kInput[] = {0x05, 0x80, 0x02, 0x12, 0x10};
  const uint8_t kOutput[] = {0x05, 0x02, 0x12, 0x10};
  std::vector<uint8_t> input(std::begin(kInput), std::end(kInput));
  std::vector<uint8_t> output(std::begin(kOutput), std::end(kOutput));
  TestReadWrite(input, output);
}

TEST_F(DescriptorLengthTest, ThreeBytesLengthForOneByteLengthData) {
  const uint8_t kInput[] = {0x05, 0x80, 0x80, 0x02, 0x12, 0x10};
  const uint8_t kOutput[] = {0x05, 0x02, 0x12, 0x10};
  std::vector<uint8_t> input(std::begin(kInput), std::end(kInput));
  std::vector<uint8_t> output(std::begin(kOutput), std::end(kOutput));
  TestReadWrite(input, output);
}

TEST_F(DescriptorLengthTest, FourBytesLengthForOneByteLengthData) {
  const uint8_t kInput[] = {0x05, 0x80, 0x80, 0x80, 0x02, 0x12, 0x10};
  const uint8_t kOutput[] = {0x05, 0x02, 0x12, 0x10};
  std::vector<uint8_t> input(std::begin(kInput), std::end(kInput));
  std::vector<uint8_t> output(std::begin(kOutput), std::end(kOutput));
  TestReadWrite(input, output);
}

TEST_F(DescriptorLengthTest, TwoBytesLengthData) {
  const uint8_t kBuffer[] = {
      0x05, 0x81, 0x02, 0x12, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00,
  };
  std::vector<uint8_t> input(std::begin(kBuffer), std::end(kBuffer));
  std::vector<uint8_t> output(std::begin(kBuffer), std::end(kBuffer));
  TestReadWrite(input, output);
}

}  // namespace media
}  // namespace shaka
