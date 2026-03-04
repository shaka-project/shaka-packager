// Copyright 2024 Google LLC. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webm/webm_video_client.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/media/base/video_stream_info.h>
#include <packager/media/formats/webm/webm_constants.h>

namespace shaka {
namespace media {

class WebMVideoClientTest : public testing::Test {
 public:
  WebMVideoClientTest() { Reset(); }

  void Reset() { client_.Reset(); }

  void SetColorRange(int64_t value) {
    client_.OnUInt(kWebMIdColorRange, value);
  }

  void SetColorPrimaries(int64_t value) {
    client_.OnUInt(kWebMIdColorPrimaries, value);
  }

  void SetTransferCharacteristics(int64_t value) {
    client_.OnUInt(kWebMIdColorTransferCharacteristics, value);
  }

  void SetMatrixCoefficients(int64_t value) {
    client_.OnUInt(kWebMIdColorMatrixCoefficients, value);
  }

  void SetPixelDimensions(int64_t width, int64_t height) {
    client_.OnUInt(kWebMIdPixelWidth, width);
    client_.OnUInt(kWebMIdPixelHeight, height);
  }

 protected:
  WebMVideoClient client_;
};

TEST_F(WebMVideoClientTest, HasColorInfo_NoColorInfo) {
  // When no color information is set, HasColorInfo should return false
  EXPECT_FALSE(client_.HasColorInfo());
}

TEST_F(WebMVideoClientTest, HasColorInfo_WithColorRange) {
  SetColorRange(0);  // Limited range
  EXPECT_TRUE(client_.HasColorInfo());
}

TEST_F(WebMVideoClientTest, HasColorInfo_WithColorPrimaries) {
  SetColorPrimaries(1);  // BT.709
  EXPECT_TRUE(client_.HasColorInfo());
}

TEST_F(WebMVideoClientTest, HasColorInfo_WithTransferCharacteristics) {
  SetTransferCharacteristics(1);  // BT.709
  EXPECT_TRUE(client_.HasColorInfo());
}

TEST_F(WebMVideoClientTest, HasColorInfo_WithMatrixCoefficients) {
  SetMatrixCoefficients(1);  // BT.709
  EXPECT_TRUE(client_.HasColorInfo());
}

TEST_F(WebMVideoClientTest, GenerateColrBoxData_DefaultValues) {
  // When no color info is set, should use default values
  std::vector<uint8_t> colr_data = client_.GenerateColrBoxData();

  // Check that data is not empty
  ASSERT_FALSE(colr_data.empty());

  // Colr box should be 19 bytes:
  // 4 bytes size + 4 bytes 'colr' + 4 bytes 'nclx' +
  // 2 bytes primaries + 2 bytes transfer + 2 bytes matrix + 1 byte full_range
  EXPECT_EQ(19u, colr_data.size());

  // Check box size (19 in big-endian)
  EXPECT_EQ(0x00, colr_data[0]);
  EXPECT_EQ(0x00, colr_data[1]);
  EXPECT_EQ(0x00, colr_data[2]);
  EXPECT_EQ(0x13, colr_data[3]);

  // Check box type 'colr'
  EXPECT_EQ('c', colr_data[4]);
  EXPECT_EQ('o', colr_data[5]);
  EXPECT_EQ('l', colr_data[6]);
  EXPECT_EQ('r', colr_data[7]);

  // Check color parameter type 'nclx'
  EXPECT_EQ('n', colr_data[8]);
  EXPECT_EQ('c', colr_data[9]);
  EXPECT_EQ('l', colr_data[10]);
  EXPECT_EQ('x', colr_data[11]);

  // Check default primaries (1 = BT.709)
  EXPECT_EQ(0x00, colr_data[12]);
  EXPECT_EQ(0x01, colr_data[13]);

  // Check default transfer characteristics (1 = BT.709)
  EXPECT_EQ(0x00, colr_data[14]);
  EXPECT_EQ(0x01, colr_data[15]);

  // Check default matrix coefficients (1 = BT.709)
  EXPECT_EQ(0x00, colr_data[16]);
  EXPECT_EQ(0x01, colr_data[17]);

  // Check default full_range_flag (0 = limited range)
  EXPECT_EQ(0x00, colr_data[18]);
}

TEST_F(WebMVideoClientTest, GenerateColrBoxData_LimitedColorRange) {
  SetColorRange(0);  // 0 = limited range in WebM

  std::vector<uint8_t> colr_data = client_.GenerateColrBoxData();

  // Check full_range_flag is 0 (limited range)
  ASSERT_EQ(19u, colr_data.size());
  EXPECT_EQ(0x00, colr_data[18]);
}

TEST_F(WebMVideoClientTest, GenerateColrBoxData_FullColorRange) {
  SetColorRange(1);  // 1 = full range in WebM

  std::vector<uint8_t> colr_data = client_.GenerateColrBoxData();

  // Check full_range_flag is 1 (full range)
  ASSERT_EQ(19u, colr_data.size());
  EXPECT_EQ(0x01, colr_data[18]);
}

TEST_F(WebMVideoClientTest, GenerateColrBoxData_CustomColorParameters) {
  SetColorPrimaries(9);            // BT.2020
  SetTransferCharacteristics(16);  // ST 2084 (PQ)
  SetMatrixCoefficients(9);        // BT.2020
  SetColorRange(0);                // Limited range

  std::vector<uint8_t> colr_data = client_.GenerateColrBoxData();

  ASSERT_EQ(19u, colr_data.size());

  // Check custom primaries (9 = BT.2020)
  EXPECT_EQ(0x00, colr_data[12]);
  EXPECT_EQ(0x09, colr_data[13]);

  // Check custom transfer characteristics (16 = ST 2084)
  EXPECT_EQ(0x00, colr_data[14]);
  EXPECT_EQ(0x10, colr_data[15]);

  // Check custom matrix coefficients (9 = BT.2020)
  EXPECT_EQ(0x00, colr_data[16]);
  EXPECT_EQ(0x09, colr_data[17]);

  // Check full_range_flag (0 = limited range)
  EXPECT_EQ(0x00, colr_data[18]);
}

TEST_F(WebMVideoClientTest, GetVideoStreamInfo_VP9WithColorInfo) {
  // Set up video dimensions
  SetPixelDimensions(1920, 1080);

  // Set color information
  SetColorRange(0);               // Limited range
  SetColorPrimaries(1);           // BT.709
  SetTransferCharacteristics(1);  // BT.709
  SetMatrixCoefficients(1);       // BT.709

  std::vector<uint8_t> codec_private;
  auto video_info = client_.GetVideoStreamInfo(1,        // track_num
                                               "V_VP9",  // codec_id
                                               codec_private,
                                               false  // is_encrypted
  );

  ASSERT_NE(nullptr, video_info);
  EXPECT_EQ(kCodecVP9, video_info->codec());

  // Check that colr_data was set
  const std::vector<uint8_t>& colr_data = video_info->colr_data();
  EXPECT_FALSE(colr_data.empty());

  // Verify it's a proper colr box
  ASSERT_EQ(19u, colr_data.size());

  // Check box type 'colr'
  EXPECT_EQ('c', colr_data[4]);
  EXPECT_EQ('o', colr_data[5]);
  EXPECT_EQ('l', colr_data[6]);
  EXPECT_EQ('r', colr_data[7]);

  // Check full_range_flag is 0 (limited range)
  EXPECT_EQ(0x00, colr_data[18]);
}

TEST_F(WebMVideoClientTest, GetVideoStreamInfo_VP8WithColorInfo) {
  // Set up video dimensions
  SetPixelDimensions(1280, 720);

  // Set color information
  SetColorRange(1);  // Full range

  std::vector<uint8_t> codec_private;
  auto video_info = client_.GetVideoStreamInfo(1,        // track_num
                                               "V_VP8",  // codec_id
                                               codec_private,
                                               false  // is_encrypted
  );

  ASSERT_NE(nullptr, video_info);
  EXPECT_EQ(kCodecVP8, video_info->codec());

  // Check that colr_data was set
  const std::vector<uint8_t>& colr_data = video_info->colr_data();
  EXPECT_FALSE(colr_data.empty());

  // Verify full_range_flag is 1 (full range)
  ASSERT_EQ(19u, colr_data.size());
  EXPECT_EQ(0x01, colr_data[18]);
}

TEST_F(WebMVideoClientTest, GetVideoStreamInfo_VP9NoColorInfo) {
  // Set up video dimensions but no color info
  SetPixelDimensions(1920, 1080);

  std::vector<uint8_t> codec_private;
  auto video_info = client_.GetVideoStreamInfo(1,        // track_num
                                               "V_VP9",  // codec_id
                                               codec_private,
                                               false  // is_encrypted
  );

  ASSERT_NE(nullptr, video_info);
  EXPECT_EQ(kCodecVP9, video_info->codec());

  // When no color info is available, colr_data should be empty
  const std::vector<uint8_t>& colr_data = video_info->colr_data();
  EXPECT_TRUE(colr_data.empty());
}

TEST_F(WebMVideoClientTest, GetVideoStreamInfo_AV1WithColorInfo) {
  // Set up video dimensions
  SetPixelDimensions(1920, 1080);

  // Set color information
  SetColorRange(0);  // Limited range

  // AV1 requires a valid codec_private
  // This is a minimal AV1 codec configuration record
  std::vector<uint8_t> codec_private = {
      0x81,  // marker and version
      0x08,  // profile and level
      0x00,  // tier, bitdepth, monochrome, chroma subsampling
      0x00   // initial presentation delay
  };

  auto video_info = client_.GetVideoStreamInfo(1,        // track_num
                                               "V_AV1",  // codec_id
                                               codec_private,
                                               false  // is_encrypted
  );

  ASSERT_NE(nullptr, video_info);
  EXPECT_EQ(kCodecAV1, video_info->codec());

  // For AV1, colr_data should NOT be set even with color info
  // (AV1 handles color info differently)
  const std::vector<uint8_t>& colr_data = video_info->colr_data();
  EXPECT_TRUE(colr_data.empty());
}

TEST_F(WebMVideoClientTest, GetVpCodecConfig_ColorRange) {
  std::vector<uint8_t> codec_private;

  // Test limited range
  SetColorRange(0);
  VPCodecConfigurationRecord config = client_.GetVpCodecConfig(codec_private);
  EXPECT_FALSE(config.video_full_range_flag());

  // Reset and test full range
  Reset();
  SetColorRange(1);
  config = client_.GetVpCodecConfig(codec_private);
  EXPECT_TRUE(config.video_full_range_flag());

  // Reset and test unset (should not affect the config)
  Reset();
  config = client_.GetVpCodecConfig(codec_private);
  // Default value in VPCodecConfigurationRecord
  EXPECT_FALSE(config.video_full_range_flag());
}

TEST_F(WebMVideoClientTest, GetVpCodecConfig_AllColorParameters) {
  std::vector<uint8_t> codec_private;

  SetColorPrimaries(9);            // BT.2020
  SetTransferCharacteristics(16);  // ST 2084 (PQ)
  SetMatrixCoefficients(9);        // BT.2020
  SetColorRange(1);                // Full range

  VPCodecConfigurationRecord config = client_.GetVpCodecConfig(codec_private);

  EXPECT_EQ(9u, config.color_primaries());
  EXPECT_EQ(16u, config.transfer_characteristics());
  EXPECT_EQ(9u, config.matrix_coefficients());
  EXPECT_TRUE(config.video_full_range_flag());
}

}  // namespace media
}  // namespace shaka
