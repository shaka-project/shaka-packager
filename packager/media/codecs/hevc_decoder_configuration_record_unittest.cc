// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/hevc_decoder_configuration_record.h"

#include <gtest/gtest.h>

namespace shaka {
namespace media {

TEST(HEVCDecoderConfigurationRecordTest, Success) {
  const uint8_t kHevcDecoderConfigurationData[] = {
      0x01,  // Version
      0x02,  // profile_indication
      0x20, 0x00, 0x00, 0x00,  // general_profile_compatibility_flags
      // general_constraint_indicator_flags
      0x90, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x3F,  // general_level_idc
      0xF0, 0x00, 0xFC, 0xFD, 0xFA, 0xFA, 0x00, 0x00,
      0x0F,  // length_size_minus_one
      0x02,  // num_of_arrays
        // array 1
        0x20,  // nal type
        0x00, 0x01,  // num nalus
          // nalu 1
          0x00, 0x18,  // nal unit length
          0x40, 0x01, 0x0C, 0x01, 0xFF, 0xFF, 0x02, 0x20,
          0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
          0x00, 0x3F, 0x99, 0x98, 0x09,
        // array 2
        0x21,  // nal type
        0x00, 0x01,  // num nalus
          // Nalu 1
          0x00, 0x0f,  // nal unit length
          0x42, 0x01, 0x01, 0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x90,
          0x00, 0x00, 0x03, 0x00, 0x00,
  };

  HEVCDecoderConfigurationRecord hevc_config;
  ASSERT_TRUE(hevc_config.Parse(kHevcDecoderConfigurationData,
                                arraysize(kHevcDecoderConfigurationData)));

  EXPECT_EQ(4u, hevc_config.nalu_length_size());

  EXPECT_EQ("hev1.2.4.L63.90", hevc_config.GetCodecString(FOURCC_hev1));
  EXPECT_EQ("hvc1.2.4.L63.90", hevc_config.GetCodecString(FOURCC_hvc1));

  EXPECT_EQ(2u, hevc_config.nalu_count());
  EXPECT_EQ(0x16u, hevc_config.nalu(0).payload_size());
  EXPECT_EQ(0x40, hevc_config.nalu(0).data()[0]);
}

TEST(HEVCDecoderConfigurationRecordTest, FailOnInsufficientData) {
  const uint8_t kHevcDecoderConfigurationData[] = {0x01, 0x02, 0x20, 0x00};

  HEVCDecoderConfigurationRecord hevc_config;
  ASSERT_FALSE(hevc_config.Parse(kHevcDecoderConfigurationData,
                                 arraysize(kHevcDecoderConfigurationData)));
}

}  // namespace media
}  // namespace shaka
