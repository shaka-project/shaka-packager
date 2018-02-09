// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/media_sample.h"
#include "packager/media/codecs/nal_unit_to_byte_stream_converter.h"
#include "packager/media/formats/mp4/box_definitions_comparison.h"

namespace shaka {
namespace media {

namespace {

// This should be valud AVCDecoderConfigurationRecord that can be parsed by
// NalUnitToByteStreamConverter.
const uint8_t kTestAVCDecoderConfigurationRecord[] = {
    0x01,        // configuration version (must be 1)
    0x00,        // AVCProfileIndication (bogus)
    0x00,        // profile_compatibility (bogus)
    0x00,        // AVCLevelIndication (bogus)
    0xFF,        // Length size minus 1 == 3
    0xE1,        // 1 sps.
    0x00, 0x1D,  // SPS length == 29
    // Some valid SPS data.
    0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
    0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
    0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
    0x60, 0x0F, 0x16, 0x2D, 0x96,
    0x01,        // 1 pps.
    0x00, 0x0A,  // PPS length == 10
    0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,
};
const uint8_t kTestAVCDecoderConfigurationRecordNaluLengthSize2[] = {
    0x01,        // configuration version (must be 1)
    0x00,        // AVCProfileIndication (bogus)
    0x00,        // profile_compatibility (bogus)
    0x00,        // AVCLevelIndication (bogus)
    0xFD,        // Length size minus 1 == 1
    0xE1,        // 1 sps.
    0x00, 0x1D,  // SPS length == 29
    // Some valid SPS data.
    0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
    0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
    0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
    0x60, 0x0F, 0x16, 0x2D, 0x96,
    0x01,        // 1 pps.
    0x00, 0x0A,  // PPS length == 10
    0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,
};

const bool kIsKeyFrame = true;
const bool kEscapeEncryptedNalu = true;

}  // namespace

class NalUnitToByteStreamConverterTest : public ::testing::Test {
 public:
  NalUnitToByteStreamConverter converter_;
};

// Expect a valid AVCDecoderConfigurationRecord to pass.
TEST(NalUnitToByteStreamConverterTest, ParseAVCDecoderConfigurationRecord) {
  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));
}

// Empty AVCDecoderConfigurationRecord should return false.
TEST(NalUnitToByteStreamConverterTest, EmptyAVCDecoderConfigurationRecord) {
  NalUnitToByteStreamConverter converter;
  EXPECT_FALSE(converter.Initialize(nullptr, 102));
  EXPECT_FALSE(converter.Initialize(kTestAVCDecoderConfigurationRecord, 0));
}

// If there is no SPS, Initialize() should fail.
TEST(NalUnitToByteStreamConverterTest, NoSps) {
  NalUnitToByteStreamConverter converter;
  const uint8_t kNoSps[] = {
      0x01,        // configuration version (must be 1)
      0x00,        // AVCProfileIndication (bogus)
      0x00,        // profile_compatibility (bogus)
      0x00,        // AVCLevelIndication (bogus)
      0xFF,        // Length size minus 1 == 3
      0xE0,        // 0 sps.
      // The rest doesn't really matter, Initialize() should fail.
      0x01,        // 1 pps.
      0x00, 0x0A,  // PPS length == 10
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,
  };

  EXPECT_FALSE(converter.Initialize(kNoSps, arraysize(kNoSps)));
}

// If there is no PPS, Initialize() should fail.
TEST(NalUnitToByteStreamConverterTest, NoPps) {
  NalUnitToByteStreamConverter converter;
  const uint8_t kNoPps[] = {
      0x01,        // configuration version (must be 1)
      0x00,        // AVCProfileIndication (bogus)
      0x00,        // profile_compatibility (bogus)
      0x00,        // AVCLevelIndication (bogus)
      0xFF,        // Length size minus 1 == 3
      0xE1,        // 1 sps.
      0x00, 0x1D,  // SPS length == 29
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00,  // 0 pps.
  };

  EXPECT_FALSE(converter.Initialize(kNoPps, arraysize(kNoPps)));
}

// If the length of SPS is 0 then Initialize() should fail.
TEST(NalUnitToByteStreamConverterTest, ZeroLengthSps) {
  NalUnitToByteStreamConverter converter;
  const uint8_t kZeroLengthSps[] = {
      0x01,        // configuration version (must be 1)
      0x00,        // AVCProfileIndication (bogus)
      0x00,        // profile_compatibility (bogus)
      0x00,        // AVCLevelIndication (bogus)
      0xFF,        // Length size minus 1 == 3
      0xE1,        // 1 sps.
      0x00, 0x00,  // SPS length == 0
      0x01,        // 1 pps.
      0x00, 0x0A,  // PPS length == 10
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,
  };

  EXPECT_FALSE(converter.Initialize(kZeroLengthSps, arraysize(kZeroLengthSps)));
}

// If the length of PPS is 0 then Initialize() should fail.
TEST(NalUnitToByteStreamConverterTest, ZeroLengthPps) {
  NalUnitToByteStreamConverter converter;
  const uint8_t kZeroLengthPps[] = {
    0x01,        // configuration version (must be 1)
    0x00,        // AVCProfileIndication (bogus)
    0x00,        // profile_compatibility (bogus)
    0x00,        // AVCLevelIndication (bogus)
    0xFF,        // Length size minus 1 == 3
    0xE1,        // 1 sps.
    0x00, 0x05,  // SPS length == 5
    0x00, 0x00, 0x00, 0x01, 0x02,
    0x01,        // 1 pps.
    0x00, 0x00,  // PPS length == 0
  };

  EXPECT_FALSE(converter.Initialize(kZeroLengthPps, arraysize(kZeroLengthPps)));
}

TEST(NalUnitToByteStreamConverterTest, ConvertUnitToByteStream) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
  };
  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStream(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, &output));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x09,                                // AUD type.
      0xF0,                                // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
  };

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
}

// Verify that if it is not a key frame then SPS and PPS from decoder
// configuration is not used.
TEST(NalUnitToByteStreamConverterTest, NonKeyFrameSample) {
  const uint8_t kNonKeyFrameStream[] = {
      0x00, 0x00, 0x00, 0x03,  // Size 3 NALU.
      0x06,                    // NAL unit type.
      0x33, 0x88,
  };
  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStream(kNonKeyFrameStream,
                                                arraysize(kNonKeyFrameStream),
                                                !kIsKeyFrame, &output));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // Anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU.
      0x06,  // NALU type.
      0x33, 0x88,
  };

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
}

// Bug found during unit testing.
// The zeros aren't contiguous but the escape byte was inserted.
TEST(NalUnitToByteStreamConverterTest, DispersedZeros) {
  const uint8_t kDispersedZeros[] = {
      0x00, 0x00, 0x00, 0x08,  // Size 8 NALU.
      0x06,                    // NAL unit type.
      // After 2 zeros (including the first byte of the NALU followed by 0, 1,
      // 2, or 3 caused it to insert the escape byte.
      0x11, 0x00,
      0x01, 0x00, 0x02, 0x00, 0x44,
  };
  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStream(
      kDispersedZeros, arraysize(kDispersedZeros), !kIsKeyFrame, &output));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // Anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU.
      0x06,  // NAL unit type.
      0x11, 0x00, 0x01, 0x00, 0x02, 0x00, 0x44,
  };

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
}

// Verify that ConvertUnitToByteStream() with escape_data = false works.
TEST(NalUnitToByteStreamConverterTest, DoNotEscape) {
  // This has sequences that should be escaped if escape_data = true.
  const uint8_t kNotEscaped[] = {
      0x00, 0x00, 0x00, 0x0C,  // Size 12 NALU.
      0x06,                    // NAL unit type.
      0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03,
  };

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStream(
      kNotEscaped, arraysize(kNotEscaped), !kIsKeyFrame, &output));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // Anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // Should be the same as the input.
      0x06,
      0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x03,
  };

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
}

// All NAL units have both clear and ciper text
TEST(NalUnitToByteStreamConverterTest, NoClearNAL) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x02,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, // Slice data
      0x00, 0x00, 0x00, 0x08,  // Size 8 NALU.
      0x02,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77, // Slice data
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(5, 9),
                                         SubsampleEntry(5, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x09,                                // AUD type.
      0xF0,                                // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(58, 9), SubsampleEntry(5, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

// Some NAL units have all clear text
TEST(NalUnitToByteStreamConverterTest, WithSomeClearNAL) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x08,  // Size 8 NALU.
      0x02,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77, // Slice data
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(19, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x09,                                // AUD type.
      0xF0,                                // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(72, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

TEST(NalUnitToByteStreamConverterTest, WithSomeClearNALAndNaluLengthSize2) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x0A,  // Size 10 NALU.
      0x06,        // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11,
      0x29, 0x77,
      0x00, 0x08,                                // Size 8 NALU.
      0x02,                                      // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,  // Slice data
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(15, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(converter.Initialize(
      kTestAVCDecoderConfigurationRecordNaluLengthSize2,
      arraysize(kTestAVCDecoderConfigurationRecordNaluLengthSize2)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
      0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96, 0x00, 0x00, 0x00, 0x01,  // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 0x00, 0x00, 0x00,
      0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(72, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

TEST(NalUnitToByteStreamConverterTest, EscapeEncryptedNalu) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x06,                    // NAL unit type.
      // Unencrypted NALU with 0x000000 pattern (no need to escaped).
      0xFD, 0x00, 0x00, 0x00, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x08,  // Size 8 NALU.
      0x02,  // NAL unit type.
      // Encrypted NALU with 0x000000 pattern (need to escape).
      0xFD, 0x00, 0x00, 0x00, 0x62, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x09,  // Size 9 NALU.
      0x01,  // NAL unit types.
      // Partially encrypted NALU with 0x000000 pattern at the boundary (need to
      // escape).
      0xFD, 0x01, 0x02, 0x00, 0x00, 0x01, 0x02, 0x03,
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(19, 7),
                                         SubsampleEntry(9, 4)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  ASSERT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      !kIsKeyFrame, kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // primary pic type is anything.
      // The input NALU 1.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x06,                    // NALU type.
      0xFD, 0x00, 0x00, 0x00, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x00, 0x00, 0x03, 0x00, 0x62, 0x29, 0x77,
      // The input NALU 3.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x01,                    // NAL unit types.
      0xFD, 0x01, 0x02, 0x00, 0x00, 0x03, 0x01, 0x02, 0x03,
  };
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedOutput),
                                 std::end(kExpectedOutput)),
            output);
  // The result subsample does not include emulation prevention bytes.
  EXPECT_THAT(subsamples, ::testing::ElementsAre(SubsampleEntry(25, 7),
                                                 SubsampleEntry(9, 4)));
}

TEST(NalUnitToByteStreamConverterTest, EncryptedNaluEndingWithZero) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x06,  // Size 6 NALU.
      0x01,                    // NALU unit types.
      // Encrypted NALU with 0x0003 pattern in the end (need to escape).
      0xFD, 0x00, 0x01, 0x02, 0x00,
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(7, 3)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  ASSERT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      !kIsKeyFrame, kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x01,                    // NALU unit types.
      // Encrypted NALU with 0x0003 pattern in the end (need to escape).
      0xFD, 0x00, 0x01, 0x02, 0x00, 0x03,
  };
  EXPECT_EQ(std::vector<uint8_t>(std::begin(kExpectedOutput),
                                 std::end(kExpectedOutput)),
            output);
  // The result subsample does not include emulation prevention bytes.
  EXPECT_THAT(subsamples, ::testing::ElementsAre(SubsampleEntry(13, 3)));
}

// Not supposed to happen, just in case, make sure it is properly supported.
TEST(NalUnitToByteStreamConverterTest, EncryptedPps) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,                                // Size 10 NALU.
      0x06,                                                  // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,  // clear
      0x00, 0x00, 0x00, 0x0B,                                // Size 11 NALU.
      0x68,  // PPS, will remain as it is different to the one in decoder
             // configuration.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x12, 0x12, 0x13, 0x14, 0x15,  // cipher
      0x00, 0x00, 0x00, 0x08,                    // Size 8 NALU.
      0x02,                                      // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,  // Slice data, cipher
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(19, 10),
                                         SubsampleEntry(5, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  // clang-format off
  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x09,                                // AUD type.
      0xF0,                                // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // PPS from sample.
      0x68, 0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x12, 0x12, 0x13, 0x14, 0x15,  // cipher
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };
  // clang-format on

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(72, 10), SubsampleEntry(5, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_THAT(kExpectedOutputSubsamples, subsamples);
}

// A clear PPS NALU follows a clear NALU, the PPS in the sample is the same as
// the PPS in decoder configuration, the PPS is dropped and subsample size is
// adjusted.
TEST(NalUnitToByteStreamConverterTest, ClearPpsSame) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0B,  // Size 11 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 0x88,  // clear
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x68,                    // PPS, same as in decoder configuration, so is
                               // removed.
      0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x08,                                // Size 8 NALU.
      0x02,                                                  // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,  // Slice data, cipher
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(34, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  // clang-format off
  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x09,                                // AUD type.
      0xF0,                                // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 0x88,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };
  // clang-format on

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(73, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

// A clear PPS NALU follows a clear NALU, the PPS in the sample is different to
// the PPS in decoder configuration, so both the PPS in the sample and the PPS
// in decoder configuration are written to output.
TEST(NalUnitToByteStreamConverterTest, ClearPpsDifferent) {
  // Only the type of the NAL units are checked.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0B,  // Size 11 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 0x88,  // clear
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x68,                    // PPS, different to the PPS in the decoder
                               // configuration, is also written to output.
      0xFE, 0xFD, 0xFC, 0xFB, 0x12, 0x12, 0x13, 0x14, 0x15,  // clear
      0x00, 0x00, 0x00, 0x08,                                // Size 8 NALU.
      0x02,                                                  // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,  // Slice data, cipher
  };

  std::vector<SubsampleEntry> subsamples{SubsampleEntry(34, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  // clang-format off
  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x09,                                // AUD type.
      0xF0,                                // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4,
      0x2F, 0xF9, 0x7F, 0xF0, 0x00, 0x80, 0x00, 0x91,
      0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 0x88,
      0x00, 0x00, 0x00, 0x01,              // Start code.
      // PPS should match the PPS above.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x12, 0x12, 0x13, 0x14, 0x15,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };
  // clang-format on

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(87, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

// One NAL unit has more than one subsamples. All subsample except the last
// be all-clear subsamples. This case is possible when the clear part is
// larger than 16-bit (64Kb), so that the clear part is split into two
// subsamples.
TEST(NalUnitToByteStreamConverterTest,
     MultipleSubsamplesInSingleNaluOnlyLastEncrypted) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x08,                    // Size 8 NALU.
      0x02,                                      // NAL unit type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,  // Slice data
  };

  std::vector<SubsampleEntry> subsamples{
      SubsampleEntry(6, 0), SubsampleEntry(8, 0), SubsampleEntry(5, 7)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
      0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96, 0x00, 0x00, 0x00, 0x01,  // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77, 0x00, 0x00, 0x00,
      0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      0xFD, 0x78, 0xA4, 0x82, 0x62, 0x29, 0x77,
  };

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(72, 7)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

// One NAL unit has more than one subsamples. All subsamples have cipher
// texts.
TEST(NalUnitToByteStreamConverterTest,
     MultipleSubsamplesInSingleNaluAllEncrypted) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSample[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x10,                          // Size 16 NALU.
      0x02,                                            // NAL unit type.
      // Slices data.
      0xFD, 0x78, 0xA4, 0x82, 0x62,  // Encrypted.
      0x29, 0x77, 0x27, 0xFD, 0x78, 0xA4,  // Clear.
      0xC3, 0x82, 0x62, 0x11,        // Encrypted.
  };

  // The 2nd (partially) and 3rd subsamples belong to the 2nd input NALU.
  std::vector<SubsampleEntry> subsamples{ SubsampleEntry(6, 0),
                                          SubsampleEntry(13, 5),
                                          SubsampleEntry(6, 4)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      kUnitStreamLikeMediaSample, arraysize(kUnitStreamLikeMediaSample),
      kIsKeyFrame, !kEscapeEncryptedNalu, &output, &subsamples));

  const uint8_t kExpectedOutput[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
      0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96, 0x00, 0x00, 0x00, 0x01,  // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
      // Slices data.
      0xFD, 0x78, 0xA4, 0x82, 0x62,  // Encrypted.
      0x29, 0x77, 0x27, 0xFD, 0x78, 0xA4,  // Clear.
      0xC3, 0x82, 0x62, 0x11, // Encrypted.
  };

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(72, 5), SubsampleEntry(6, 4)};

  EXPECT_EQ(std::vector<uint8_t>(kExpectedOutput,
                                 kExpectedOutput + arraysize(kExpectedOutput)),
            output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

// One NAL unit is larger than 2^16 bytes and the corresponding subsamples is
// split into small subsamples. All subsamples have cipher texts.
TEST(NalUnitToByteStreamConverterTest,
     LargeNaluWithMultipleSubsamples) {
  // Only the type of the NAL units are checked.
  // This does not contain AUD, SPS, nor PPS.
  const uint8_t kUnitStreamLikeMediaSamplePart1[] = {
      0x00, 0x00, 0x00, 0x0A,  // Size 10 NALU.
      0x06,                    // NAL unit type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,  // Encrypted.
      0x00, 0x01, 0x00, 0x0f,                          // Size 65551 NALU.
      0x02,                                            // NAL unit type.
  };

  const std::vector<uint8_t> kUnitStreamLikeMediaSamplePart2(65535, 0x01);

  const uint8_t kUnitStreamLikeMediaSamplePart3[] = {
      // Slices data.
      0xFD, 0x78, 0xA4, 0x82, 0x62,        // Encrypted.
      0x29, 0x77, 0x27, 0xFD, 0x78, 0xA4,  // Clear.
      0xC3, 0x82, 0x62, 0x11,              // Encrypted.
  };

  std::vector<uint8_t> unit_stream_like_media_sample(
      std::begin(kUnitStreamLikeMediaSamplePart1),
      std::end(kUnitStreamLikeMediaSamplePart1));

  unit_stream_like_media_sample.insert(
      unit_stream_like_media_sample.end(),
      std::begin(kUnitStreamLikeMediaSamplePart2),
      std::end(kUnitStreamLikeMediaSamplePart2));

  unit_stream_like_media_sample.insert(
      unit_stream_like_media_sample.end(),
      std::begin(kUnitStreamLikeMediaSamplePart3),
      std::end(kUnitStreamLikeMediaSamplePart3));

  std::vector<SubsampleEntry> subsamples{ SubsampleEntry(5, 9),
                                          SubsampleEntry(65535, 0),
                                          SubsampleEntry(5, 5),
                                          SubsampleEntry(6, 4)};

  NalUnitToByteStreamConverter converter;
  EXPECT_TRUE(
      converter.Initialize(kTestAVCDecoderConfigurationRecord,
                           arraysize(kTestAVCDecoderConfigurationRecord)));

  std::vector<uint8_t> output;
  EXPECT_TRUE(converter.ConvertUnitToByteStreamWithSubsamples(
      unit_stream_like_media_sample.data(),
      unit_stream_like_media_sample.size(), kIsKeyFrame, !kEscapeEncryptedNalu,
      &output, &subsamples));

  const uint8_t kExpectedOutputPart1[] = {
      0x00, 0x00, 0x00, 0x01,  // Start code.
      0x09,                    // AUD type.
      0xF0,                    // primary pic type is anything.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // Some valid SPS data.
      0x67, 0x64, 0x00, 0x1E, 0xAC, 0xD9, 0x40, 0xB4, 0x2F, 0xF9, 0x7F, 0xF0,
      0x00, 0x80, 0x00, 0x91, 0x00, 0x00, 0x03, 0x03, 0xE9, 0x00, 0x00, 0xEA,
      0x60, 0x0F, 0x16, 0x2D, 0x96, 0x00, 0x00, 0x00, 0x01,  // Start code.
      0x68, 0xFE, 0xFD, 0xFC, 0xFB, 0x11, 0x12, 0x13, 0x14, 0x15,  // PPS.
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 1.
      0x06,  // NALU type.
      0xFD, 0x78, 0xA4, 0xC3, 0x82, 0x62, 0x11, 0x29, 0x77,
      0x00, 0x00, 0x00, 0x01,  // Start code.
      // The input NALU 2.
      0x02,  // NALU type.
  };

  const std::vector<uint8_t> kExpectedOutputPart2 =
      kUnitStreamLikeMediaSamplePart2;

  const uint8_t kExpectedOutputPart3[] = {
      // Slices data.
      0xFD, 0x78, 0xA4, 0x82, 0x62,  // Encrypted.
      0x29, 0x77, 0x27, 0xFD, 0x78, 0xA4,  // Clear.
      0xC3, 0x82, 0x62, 0x11, // Encrypted.
  };

  std::vector<uint8_t> expected_output(std::begin(kExpectedOutputPart1),
                                       std::end(kExpectedOutputPart1));

  expected_output.insert(expected_output.end(),
                         std::begin(kExpectedOutputPart2),
                         std::end(kExpectedOutputPart2));

  expected_output.insert(expected_output.end(),
                         std::begin(kExpectedOutputPart3),
                         std::end(kExpectedOutputPart3));

  const std::vector<SubsampleEntry> kExpectedOutputSubsamples{
      SubsampleEntry(58, 9), SubsampleEntry(65535, 0), SubsampleEntry(5, 5),
      SubsampleEntry(6, 4)};

  EXPECT_EQ(expected_output, output);
  EXPECT_EQ(kExpectedOutputSubsamples, subsamples);
}

}  // namespace media
}  // namespace shaka
