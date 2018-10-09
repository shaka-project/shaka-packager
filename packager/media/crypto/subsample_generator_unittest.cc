// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/crypto/subsample_generator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/av1_parser.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vpx_parser.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Test;
using ::testing::Values;
using ::testing::WithParamInterface;

const bool kVP9SubsampleEncryption = true;
const uint8_t kH264CodecConfig[] = {
    // clang-format off
    // Header
    0x01, 0x64, 0x00, 0x1e, 0xff,
    // SPS count (ignore top three bits)
    0xe1,
    // SPS
    0x00, 0x19,  // Size
    0x67, 0x64, 0x00, 0x1e, 0xac, 0xd9, 0x40, 0xa0, 0x2f, 0xf9, 0x70, 0x11,
    0x00, 0x00, 0x03, 0x03, 0xe9, 0x00, 0x00, 0xea, 0x60, 0x0f, 0x16, 0x2d,
    0x96,
    // PPS count
    0x01,
    // PPS
    0x00, 0x06,  // Size
    0x68, 0xeb, 0xe3, 0xcb, 0x22, 0xc0,
    // clang-format on
};
const uint8_t kAV1CodecConfig[] = {0x00, 0x01, 0x02, 0x03};
const int kTrackId = 1;
const uint32_t kTimeScale = 1000;
const uint64_t kDuration = 10000;
const char kCodecString[] = "codec string";
const char kLanguage[] = "eng";
const bool kEncrypted = true;

VideoStreamInfo GetVideoStreamInfo(Codec codec) {
  const uint16_t kWidth = 10u;
  const uint16_t kHeight = 20u;
  const uint32_t kPixelWidth = 2u;
  const uint32_t kPixelHeight = 3u;
  const int16_t kTrickPlayFactor = 0;
  const uint8_t kNaluLengthSize = 1u;

  const uint8_t* codec_config = nullptr;
  size_t codec_config_size = 0;
  switch (codec) {
    case kCodecH264:
      codec_config = kH264CodecConfig;
      codec_config_size = sizeof(kH264CodecConfig);
      break;
    case kCodecAV1:
      codec_config = kAV1CodecConfig;
      codec_config_size = sizeof(kAV1CodecConfig);
      break;
    default:
      // We do not care about the codec configs for other codecs in this file.
      break;
  }
  return VideoStreamInfo(kTrackId, kTimeScale, kDuration, codec,
                         H26xStreamFormat::kUnSpecified, kCodecString,
                         codec_config, codec_config_size, kWidth, kHeight,
                         kPixelWidth, kPixelHeight, kTrickPlayFactor,
                         kNaluLengthSize, kLanguage, !kEncrypted);
}

AudioStreamInfo GetAudioStreamInfo(Codec codec) {
  const uint8_t kSampleBits = 1;
  const uint8_t kNumChannels = 2;
  const uint32_t kSamplingFrequency = 48000;
  const uint64_t kSeekPrerollNs = 12345;
  const uint64_t kCodecDelayNs = 56789;
  const uint32_t kMaxBitrate = 13579;
  const uint32_t kAvgBitrate = 13000;
  const uint8_t kCodecConfig[] = {0x00};

  return AudioStreamInfo(kTrackId, kTimeScale, kDuration, codec, kCodecString,
                         kCodecConfig, sizeof(kCodecConfig), kSampleBits,
                         kNumChannels, kSamplingFrequency, kSeekPrerollNs,
                         kCodecDelayNs, kMaxBitrate, kAvgBitrate, kLanguage,
                         !kEncrypted);
}

}  // namespace

inline bool operator==(const SubsampleEntry& lhs, const SubsampleEntry& rhs) {
  return lhs.clear_bytes == rhs.clear_bytes &&
         lhs.cipher_bytes == rhs.cipher_bytes;
}

class MockVPxParser : public VPxParser {
 public:
  MOCK_METHOD3(Parse,
               bool(const uint8_t* data,
                    size_t data_size,
                    std::vector<VPxFrameInfo>* vpx_frames));
};

class MockVideoSliceHeaderParser : public VideoSliceHeaderParser {
 public:
  MOCK_METHOD1(Initialize,
               bool(const std::vector<uint8_t>& decoder_configuration));
  MOCK_METHOD1(GetHeaderSize, int64_t(const Nalu& nalu));
};

class MockAV1Parser : public AV1Parser {
 public:
  MOCK_METHOD3(Parse,
               bool(const uint8_t* data,
                    size_t data_size,
                    std::vector<Tile>* tiles));
};

class SubsampleGeneratorTest : public Test, public WithParamInterface<FourCC> {
 public:
  SubsampleGeneratorTest() : protection_scheme_(GetParam()) {}

 protected:
  FourCC protection_scheme_;
};

TEST_P(SubsampleGeneratorTest, VP9FullSampleEncryption) {
  SubsampleGenerator generator(!kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecVP9)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAre());
}

TEST_P(SubsampleGeneratorTest, VP9ParseFailed) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecVP9)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};

  std::unique_ptr<MockVPxParser> mock_vpx_parser(new MockVPxParser);
  EXPECT_CALL(*mock_vpx_parser, Parse(kFrame, kFrameSize, _))
      .WillOnce(Return(false));

  generator.InjectVpxParserForTesting(std::move(mock_vpx_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_NOT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
}

TEST_P(SubsampleGeneratorTest, VP9SubsampleEncryption) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecVP9)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};
  constexpr size_t kUncompressedHeaderSize = 20;
  // VP9 block align protected data for all protection schemes.
  const SubsampleEntry kExpectedSubsamples[] = {
      // {20,30} block aligned.
      {34, 16},
  };

  std::vector<VPxFrameInfo> vpx_frame_info(1);
  vpx_frame_info[0].frame_size = kFrameSize;
  vpx_frame_info[0].uncompressed_header_size = kUncompressedHeaderSize;

  std::unique_ptr<MockVPxParser> mock_vpx_parser(new MockVPxParser);
  EXPECT_CALL(*mock_vpx_parser, Parse(kFrame, kFrameSize, _))
      .WillOnce(DoAll(SetArgPointee<2>(vpx_frame_info), Return(true)));

  generator.InjectVpxParserForTesting(std::move(mock_vpx_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAreArray(kExpectedSubsamples));
}

TEST_P(SubsampleGeneratorTest, VP9SubsampleEncryptionWithSuperFrame) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecVP9)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};
  // Super frame with two subframes.
  constexpr size_t kSubFrameSizes[] = {10, 34};
  constexpr size_t kUncompressedHeaderSizes[] = {4, 1};
  // VP9 block align protected data for all protection schemes.
  const SubsampleEntry kExpectedSubsamples[] = {
      // {4,6},{1,33} block aligned => {10,0},{2,32}
      // Then merge consecutive clear-only subsamples.
      {12, 32},
      // Superframe index (50 - 10 - 34).
      {6, 0},
  };

  std::vector<VPxFrameInfo> vpx_frame_info(2);
  for (int i = 0; i < 2; i++) {
    vpx_frame_info[i].frame_size = kSubFrameSizes[i];
    vpx_frame_info[i].uncompressed_header_size = kUncompressedHeaderSizes[i];
  }

  std::unique_ptr<MockVPxParser> mock_vpx_parser(new MockVPxParser);
  EXPECT_CALL(*mock_vpx_parser, Parse(kFrame, kFrameSize, _))
      .WillOnce(DoAll(SetArgPointee<2>(vpx_frame_info), Return(true)));

  generator.InjectVpxParserForTesting(std::move(mock_vpx_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAreArray(kExpectedSubsamples));
}

TEST_P(SubsampleGeneratorTest, VP9SubsampleEncryptionWithLargeSuperFrame) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecVP9)));

  constexpr size_t kFrameSize = 0x23456;
  constexpr uint8_t kFrame[kFrameSize] = {};
  // Super frame with two subframes.
  constexpr size_t kSubFrameSizes[] = {0x10, 0x23000, 0x440};
  constexpr size_t kUncompressedHeaderSizes[] = {4, 0x21000, 2};
  // VP9 block align protected data for all protection schemes.
  const SubsampleEntry kExpectedSubsamples[] = {
      // {4,12},{1,0x23000-1} block aligned => {16,0},{0x21000,0x2000}
      // Then split big clear_bytes, merge consecutive clear-only subsamples.
      {0xffff, 0},
      {0xffff, 0},
      {0x1012, 0x2000},
      // {2,0x440-2} block aligned.
      {0x10, 0x430},
      // Superframe index.
      {6, 0},
  };

  std::vector<VPxFrameInfo> vpx_frame_info(3);
  for (int i = 0; i < 3; i++) {
    vpx_frame_info[i].frame_size = kSubFrameSizes[i];
    vpx_frame_info[i].uncompressed_header_size = kUncompressedHeaderSizes[i];
  }

  std::unique_ptr<MockVPxParser> mock_vpx_parser(new MockVPxParser);
  EXPECT_CALL(*mock_vpx_parser, Parse(kFrame, kFrameSize, _))
      .WillOnce(DoAll(SetArgPointee<2>(vpx_frame_info), Return(true)));

  generator.InjectVpxParserForTesting(std::move(mock_vpx_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAreArray(kExpectedSubsamples));
}

TEST_P(SubsampleGeneratorTest, H264ParseFailed) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecH264)));

  constexpr uint8_t kFrame[] = {
      // First NALU (nalu_size = 9).
      0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
  constexpr size_t kFrameSize = sizeof(kFrame);

  std::unique_ptr<MockVideoSliceHeaderParser> mock_video_slice_header_parser(
      new MockVideoSliceHeaderParser);
  EXPECT_CALL(*mock_video_slice_header_parser, GetHeaderSize(_))
      .WillOnce(Return(-1));

  generator.InjectVideoSliceHeaderParserForTesting(
      std::move(mock_video_slice_header_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_NOT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
}

TEST_P(SubsampleGeneratorTest, H264SubsampleEncryption) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecH264)));

  constexpr uint8_t kFrame[] = {
      // First NALU (nalu_size = 9).
      0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      // Second NALU (nalu_size = 0x25).
      0x27, 0x25, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27,
      // Third non-video-slice NALU (nalu_size = 0x32).
      0x32, 0x67, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
      0x30, 0x31, 0x32};
  constexpr size_t kFrameSize = sizeof(kFrame);
  // There are two video slices.
  const size_t kSliceHeaderSize[] = {4, 5};
  const SubsampleEntry kExpectedUnalignedSubsamples[] = {
      // clear_bytes = nalu_length_size (1) + type_size (1) + header_size (4).
      // encrypted_bytes = nalu_size (9) - type_size (1) - header_size (4).
      {6, 4},
      // clear_bytes = nalu_length_size (1) + type_size (1) + header_size (5).
      // encrypted_bytes = nalu_size (0x27) - type_size (1) - header_size (5).
      {7, 0x21},
      // Non-video slice, clear_bytes = nalu_length_size (1) + nalu_size (0x32).
      // encrypted_bytes = 0.
      {0x33, 0},
  };
  const SubsampleEntry kExpectedAlignedSubsamples[] = {
      // {6,4},{7,0x21} block aligned => {10,0},{8,0x20}
      // Then merge consecutive clear-only subsamples.
      {18, 0x20},
      {0x33, 0},
  };

  std::unique_ptr<MockVideoSliceHeaderParser> mock_video_slice_header_parser(
      new MockVideoSliceHeaderParser);
  EXPECT_CALL(*mock_video_slice_header_parser, GetHeaderSize(_))
      .WillOnce(Return(kSliceHeaderSize[0]))
      .WillOnce(Return(kSliceHeaderSize[1]));

  generator.InjectVideoSliceHeaderParserForTesting(
      std::move(mock_video_slice_header_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  // Align subsamples for all CENC protection schemes except for cbcs.
  if (protection_scheme_ == FOURCC_cbcs)
    EXPECT_THAT(subsamples, ElementsAreArray(kExpectedUnalignedSubsamples));
  else
    EXPECT_THAT(subsamples, ElementsAreArray(kExpectedAlignedSubsamples));
}

TEST_P(SubsampleGeneratorTest, AV1ParserFailed) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecAV1)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};

  std::unique_ptr<MockAV1Parser> mock_av1_parser(new MockAV1Parser);
  EXPECT_CALL(*mock_av1_parser, Parse(kFrame, kFrameSize, _))
      .WillOnce(Return(false));

  generator.InjectAV1ParserForTesting(std::move(mock_av1_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_NOT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
}

TEST_P(SubsampleGeneratorTest, AV1SubsampleEncryption) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetVideoStreamInfo(kCodecAV1)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};
  constexpr size_t kTileOffsets[] = {4, 11};
  constexpr size_t kTileSizes[] = {6, 33};
  // AV1 block align protected data for all protection schemes.
  const SubsampleEntry kExpectedSubsamples[] = {
      // {4,6},{11-4-6,33},{50-11-33,0} block aligned => {10,0},{2,32},{6,0}.
      // Then merge consecutive clear-only subsamples.
      {12, 32},
      {6, 0},
  };

  std::vector<AV1Parser::Tile> tiles(2);
  for (int i = 0; i < 2; i++) {
    tiles[i].start_offset_in_bytes = kTileOffsets[i];
    tiles[i].size_in_bytes = kTileSizes[i];
  }

  std::unique_ptr<MockAV1Parser> mock_av1_parser(new MockAV1Parser);
  EXPECT_CALL(*mock_av1_parser, Parse(kFrame, kFrameSize, _))
      .WillOnce(DoAll(SetArgPointee<2>(tiles), Return(true)));

  generator.InjectAV1ParserForTesting(std::move(mock_av1_parser));

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAreArray(kExpectedSubsamples));
}

TEST_P(SubsampleGeneratorTest, AACIsFullSampleEncrypted) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(
      generator.Initialize(protection_scheme_, GetAudioStreamInfo(kCodecAAC)));

  constexpr size_t kFrameSize = 50;
  constexpr uint8_t kFrame[kFrameSize] = {};

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAre());
}

INSTANTIATE_TEST_CASE_P(
    CencProtectionSchemes,
    SubsampleGeneratorTest,
    Values(FOURCC_cenc, FOURCC_cens, FOURCC_cbc1, FOURCC_cbcs));

TEST(SampleAesSubsampleGeneratorTest, AAC) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(generator.Initialize(kAppleSampleAesProtectionScheme,
                                 GetAudioStreamInfo(kCodecAAC)));

  constexpr size_t kNumFrames = 4;
  constexpr size_t kMaxFrameSize = 100;
  constexpr size_t kFrameSizes[] = {6, 16, 17, 50};
  constexpr uint8_t kFrames[kNumFrames][kMaxFrameSize] = {};
  // 16 bytes clear lead.
  const SubsampleEntry kExpectedSubsamples[] = {
      {6, 0},
      {16, 0},
      {16, 1},
      {16, 34},
  };

  for (int i = 0; i < 4; i++) {
    std::vector<SubsampleEntry> subsamples;
    ASSERT_OK(
        generator.GenerateSubsamples(kFrames[i], kFrameSizes[i], &subsamples));
    EXPECT_THAT(subsamples, ElementsAre(kExpectedSubsamples[i]));
  }
}

TEST(SampleAesSubsampleGeneratorTest, H264) {
  SubsampleGenerator generator(kVP9SubsampleEncryption);
  ASSERT_OK(generator.Initialize(kAppleSampleAesProtectionScheme,
                                 GetVideoStreamInfo(kCodecH264)));

  constexpr uint8_t kFrame[] = {
      // First NALU (nalu_size = 9).
      0x09, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
      // Second NALU (nalu_size = 0x30).
      0x30, 0x25, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
      0x30,
      // Third NALU (nalu_size = 0x31).
      0x31, 0x25, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
      0x30, 0x31,
      // Fourth non-video-slice NALU (nalu_size = 6).
      0x32, 0x67, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
      0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
      0x30, 0x31, 0x32};
  constexpr size_t kFrameSize = sizeof(kFrame);
  const SubsampleEntry kExpectedSubsamples[] = {
      // NAL units with nalu_size <= 32+16 is not encrypted, so
      // the first two NALUs are left in clear {1+9,0},{1+48,0}.
      // The third NALUs has a fixed 32 bytes clear lead, +1 byte NALU length
      // size, so it is {1+32, 17}.
      // Then merge consecutive clear-only subsamples.
      {1 + 9 + 1 + 48 + 1 + 32, 17},
      // Non video slice is not encrypted.
      {0x33, 0},
  };

  std::vector<SubsampleEntry> subsamples;
  ASSERT_OK(generator.GenerateSubsamples(kFrame, kFrameSize, &subsamples));
  EXPECT_THAT(subsamples, ElementsAreArray(kExpectedSubsamples));
}

}  // namespace media
}  // namespace shaka
