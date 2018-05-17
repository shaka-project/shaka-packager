// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/codecs/es_descriptor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

namespace shaka {
namespace media {

TEST(ESDescriptorTest, SingleByteLengthTest) {
  // clang-format off
  const uint8_t kBuffer[] = {
      // ESDescriptor tag with one byte size.
      0x03, 0x19,
        // ESDescriptor fields.
        0x00, 0x01, 0x00,
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
  EXPECT_EQ(es_desc.object_type(), ObjectType::kForbidden);
  EXPECT_TRUE(es_desc.Parse(data));

  EXPECT_EQ(es_desc.object_type(), ObjectType::kISO_14496_3);
  EXPECT_THAT(es_desc.decoder_specific_info(), ElementsAre(0x12, 0x10));
}

TEST(ESDescriptorTest, NonAACTest) {
  // clang-format off
  const uint8_t kBuffer[] = {
      // ESDescriptor tag with one byte size.
      0x03, 0x19,
        // ESDescriptor fields.
        0x00, 0x01, 0x00,
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

  EXPECT_EQ(static_cast<int>(es_desc.object_type()), 0x66);
  EXPECT_NE(es_desc.object_type(), ObjectType::kISO_14496_3);
  EXPECT_THAT(es_desc.decoder_specific_info(), ElementsAre(0x12, 0x10));
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

  EXPECT_EQ(static_cast<int>(es_desc.object_type()), 0x6b);
  EXPECT_EQ(es_desc.max_bitrate(), 0x28500u);
  EXPECT_EQ(es_desc.avg_bitrate(), 0x27100u);
  EXPECT_THAT(es_desc.decoder_specific_info(), ElementsAre());
}

TEST(ESDescriptorTest, MultiByteLengthTest) {
  // clang-format off
  const uint8_t kBuffer[] = {
      // ESDescriptor tag with two bytes size.
      0x03, 0x80, 0x1b,
        // ESDescriptor fields.
        0x00, 0x01, 0x00,
        // DecoderConfigDescriptor tag with three bytes size.
        0x04, 0x80, 0x80, 0x14,
          // Object Type.
          0x40,
          // Three 4-byte fields: dummy, max bitrate, avg bitrate.
          0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          // DecoderSpecificInfo tag with four bytes size.
          0x05, 0x80, 0x80, 0x80, 0x02,
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

  EXPECT_EQ(es_desc.object_type(), ObjectType::kISO_14496_3);
  EXPECT_THAT(es_desc.decoder_specific_info(), ElementsAre(0x12, 0x10));
}

}  // namespace media
}  // namespace shaka
