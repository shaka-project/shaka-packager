// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/packed_audio/packed_audio_segmenter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/id3_tag.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/codecs/aac_audio_specific_config.h"
#include "packager/status_test_util.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::Test;

namespace shaka {
namespace media {
namespace {

constexpr uint32_t kZeroTransportStreamTimestampOffset = 0;
constexpr uint32_t kTimescale = 5625;
constexpr double kExpectedTimescaleScale = kPackedAudioTimescale / kTimescale;
static_assert(kExpectedTimescaleScale == 16.0, "");

const int kTrackId = 0;
const uint64_t kDuration = 180000;
const char kCodecString[] = "codec-string";
const char kLanguage[] = "eng";
const bool kIsEncrypted = true;

const uint8_t kCodecConfig[] = {0x2B, 0x92, 8, 0};

const uint8_t kSampleBits = 16;
const uint8_t kNumChannels = 2;
const uint32_t kSamplingFrequency = 44100;
const uint64_t kSeekPreroll = 0;
const uint64_t kCodecDelay = 0;
const uint32_t kMaxBitrate = 320000;
const uint32_t kAverageBitrate = 256000;

const char kSample1Data[] = "sample 1 data";
const char kAdtsSample1Data[] = "adts sample 1 data";
const char kSample2Data[] = "sample 2 data";
const int64_t kPts1 = 0x12345;
const int64_t kDts1 = 0x12000;
const int64_t kPts2 = 0x12445;
const int64_t kDts2 = 0x12100;

// String form of kPts1 * kExpectedTimescaleScale.
const char kScaledPts1[] = {0, 0, 0, 0, 0, 0x12, 0x34, 0x50};
// String form of kPts2 * kExpectedTimescaleScale.
const char kScaledPts2[] = {0, 0, 0, 0, 0, 0x12, 0x44, 0x50};

const char kSegment1Data[] = "segment 1 data";
const char kSegment2Data[] = "segment 2 data";

std::vector<uint8_t> StringToVector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::shared_ptr<AudioStreamInfo> CreateAudioStreamInfo(Codec codec) {
  std::shared_ptr<AudioStreamInfo> stream_info(new AudioStreamInfo(
      kTrackId, kTimescale, kDuration, codec, kCodecString, kCodecConfig,
      sizeof(kCodecConfig), kSampleBits, kNumChannels, kSamplingFrequency,
      kSeekPreroll, kCodecDelay, kMaxBitrate, kAverageBitrate, kLanguage,
      kIsEncrypted));
  return stream_info;
}

std::shared_ptr<MediaSample> CreateSample(int64_t pts,
                                          int64_t dts,
                                          const std::string& sample_data) {
  const bool kIsKeyFrame = true;
  std::shared_ptr<MediaSample> sample = MediaSample::CopyFrom(
      reinterpret_cast<const uint8_t*>(sample_data.data()), sample_data.size(),
      kIsKeyFrame);
  sample->set_pts(pts);
  sample->set_dts(dts);
  return sample;
}

std::shared_ptr<MediaSample> CreateEncryptedSample(
    int64_t pts,
    int64_t dts,
    const std::string& sample_data) {
  auto sample = CreateSample(pts, dts, sample_data);
  sample->set_is_encrypted(true);
  return sample;
}

class MockAACAudioSpecificConfig : public AACAudioSpecificConfig {
 public:
  MOCK_METHOD1(Parse, bool(const std::vector<uint8_t>& data));
  MOCK_CONST_METHOD1(ConvertToADTS, bool(std::vector<uint8_t>* buffer));
};

class MockId3Tag : public Id3Tag {
 public:
  MOCK_METHOD2(AddPrivateFrame,
               void(const std::string&, const std::string& data));
  MOCK_METHOD1(WriteToBuffer, bool(BufferWriter* buffer_writer));
};

class TestablePackedAudioSegmenter : public PackedAudioSegmenter {
 public:
  TestablePackedAudioSegmenter()
      : PackedAudioSegmenter(kZeroTransportStreamTimestampOffset) {}

  MOCK_METHOD0(CreateAdtsConverter, std::unique_ptr<AACAudioSpecificConfig>());
  MOCK_METHOD0(CreateId3Tag, std::unique_ptr<Id3Tag>());
};

}  // namespace

class PackedAudioSegmenterTest : public ::testing::Test {
 public:
  PackedAudioSegmenterTest()
      : mock_adts_converter_(new MockAACAudioSpecificConfig) {}

  std::string GetSegmentData() {
    const BufferWriter& buffer = *segmenter_.segment_buffer();
    return std::string(buffer.Buffer(), buffer.Buffer() + buffer.Size());
  }

 protected:
  TestablePackedAudioSegmenter segmenter_;
  std::unique_ptr<MockAACAudioSpecificConfig> mock_adts_converter_;
};

TEST_F(PackedAudioSegmenterTest, AacInitialize) {
  EXPECT_CALL(*mock_adts_converter_, Parse(ElementsAreArray(kCodecConfig)))
      .WillOnce(Return(true));
  EXPECT_CALL(segmenter_, CreateAdtsConverter())
      .WillOnce(Return(ByMove(std::move(mock_adts_converter_))));
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAAC)));
  EXPECT_EQ(kExpectedTimescaleScale, segmenter_.TimescaleScale());
}

TEST_F(PackedAudioSegmenterTest, AacInitializeFailed) {
  EXPECT_CALL(*mock_adts_converter_, Parse(ElementsAreArray(kCodecConfig)))
      .WillOnce(Return(false));
  EXPECT_CALL(segmenter_, CreateAdtsConverter())
      .WillOnce(Return(ByMove(std::move(mock_adts_converter_))));
  ASSERT_NOT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAAC)));
}

TEST_F(PackedAudioSegmenterTest, Ac3Initialize) {
  EXPECT_CALL(segmenter_, CreateAdtsConverter()).Times(0);
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAC3)));
  EXPECT_EQ(kExpectedTimescaleScale, segmenter_.TimescaleScale());
}

TEST_F(PackedAudioSegmenterTest, AacAddSample) {
  EXPECT_CALL(*mock_adts_converter_, Parse(ElementsAreArray(kCodecConfig)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_adts_converter_,
              ConvertToADTS(Pointee(Eq(StringToVector(kSample1Data)))))
      .WillOnce(DoAll(SetArgPointee<0>(StringToVector(kAdtsSample1Data)),
                      Return(true)));

  EXPECT_CALL(segmenter_, CreateAdtsConverter())
      .WillOnce(Return(ByMove(std::move(mock_adts_converter_))));
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAAC)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag,
              AddPrivateFrame(
                  kTimestampOwnerIdentifier,
                  std::string(std::begin(kScaledPts1), std::end(kScaledPts1))));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_))
      .WillOnce(Invoke([](BufferWriter* buffer) {
        buffer->AppendString(kSegment1Data);
        return true;
      }));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(segmenter_.AddSample(*CreateSample(kPts1, kDts1, kSample1Data)));
  EXPECT_EQ(std::string(kSegment1Data) + kAdtsSample1Data, GetSegmentData());
}

TEST_F(PackedAudioSegmenterTest, Ac3AddSample) {
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAC3)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag, AddPrivateFrame(kTimestampOwnerIdentifier, _));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_))
      .WillOnce(Invoke([](BufferWriter* buffer) {
        buffer->AppendString(kSegment1Data);
        return true;
      }));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(segmenter_.AddSample(*CreateSample(kPts1, kDts1, kSample1Data)));
  EXPECT_EQ(std::string(kSegment1Data) + kSample1Data, GetSegmentData());
}

TEST_F(PackedAudioSegmenterTest, Ac3AddSampleTwice) {
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAC3)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag, AddPrivateFrame(kTimestampOwnerIdentifier, _));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_))
      .WillOnce(Invoke([](BufferWriter* buffer) {
        buffer->AppendString(kSegment1Data);
        return true;
      }));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(segmenter_.AddSample(*CreateSample(kPts1, kDts1, kSample1Data)));
  ASSERT_OK(segmenter_.AddSample(*CreateSample(kPts2, kDts2, kSample2Data)));
  EXPECT_EQ(std::string(kSegment1Data) + kSample1Data + kSample2Data,
            GetSegmentData());
}

TEST_F(PackedAudioSegmenterTest, Ac3AddSampleTwiceWithFinalize) {
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAC3)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag,
              AddPrivateFrame(
                  kTimestampOwnerIdentifier,
                  std::string(std::begin(kScaledPts1), std::end(kScaledPts1))));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_))
      .WillOnce(Invoke([](BufferWriter* buffer) {
        buffer->AppendString(kSegment1Data);
        return true;
      }));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(segmenter_.AddSample(*CreateSample(kPts1, kDts1, kSample1Data)));
  ASSERT_OK(segmenter_.FinalizeSegment());
  EXPECT_EQ(std::string(kSegment1Data) + kSample1Data, GetSegmentData());

  std::unique_ptr<MockId3Tag> mock_id3_tag2(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag2,
              AddPrivateFrame(
                  kTimestampOwnerIdentifier,
                  std::string(std::begin(kScaledPts2), std::end(kScaledPts2))));
  EXPECT_CALL(*mock_id3_tag2, WriteToBuffer(_))
      .WillOnce(Invoke([](BufferWriter* buffer) {
        buffer->AppendString(kSegment2Data);
        return true;
      }));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag2))));

  ASSERT_OK(segmenter_.AddSample(*CreateSample(kPts2, kDts2, kSample2Data)));
  EXPECT_EQ(std::string(kSegment2Data) + kSample2Data, GetSegmentData());
}

TEST_F(PackedAudioSegmenterTest, AacAddEncryptedSample) {
  EXPECT_CALL(*mock_adts_converter_, Parse(ElementsAreArray(kCodecConfig)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_adts_converter_,
              ConvertToADTS(Pointee(Eq(StringToVector(kSample1Data)))))
      .WillOnce(DoAll(SetArgPointee<0>(StringToVector(kAdtsSample1Data)),
                      Return(true)));

  EXPECT_CALL(segmenter_, CreateAdtsConverter())
      .WillOnce(Return(ByMove(std::move(mock_adts_converter_))));
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAAC)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag, AddPrivateFrame(kTimestampOwnerIdentifier, _));
  // Derived from |kCodecConfig|.
  const char kExpectedAacSetup[] = "zach\x0\x0\x1\x4\x2B\x92\x8\x0";
  EXPECT_CALL(*mock_id3_tag,
              AddPrivateFrame(kAudioDescriptionOwnerIdentifier,
                              std::string(std::begin(kExpectedAacSetup),
                                          std::end(kExpectedAacSetup) - 1)));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_)).WillOnce(Return(true));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(
      segmenter_.AddSample(*CreateEncryptedSample(kPts1, kDts1, kSample1Data)));
}

TEST_F(PackedAudioSegmenterTest, Ac3AddEncryptedSample) {
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecAC3)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag, AddPrivateFrame(kTimestampOwnerIdentifier, _));
  // Derived from |kSample1Data|.
  const char kExpectedAc3Setup[] = "zac3\x0\x0\x1\xAsample 1 d";
  EXPECT_CALL(*mock_id3_tag,
              AddPrivateFrame(kAudioDescriptionOwnerIdentifier,
                              std::string(std::begin(kExpectedAc3Setup),
                                          std::end(kExpectedAc3Setup) - 1)));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_)).WillOnce(Return(true));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(
      segmenter_.AddSample(*CreateEncryptedSample(kPts1, kDts1, kSample1Data)));
}

TEST_F(PackedAudioSegmenterTest, Eac3AddEncryptedSample) {
  ASSERT_OK(segmenter_.Initialize(*CreateAudioStreamInfo(kCodecEAC3)));

  std::unique_ptr<MockId3Tag> mock_id3_tag(new MockId3Tag);
  EXPECT_CALL(*mock_id3_tag, AddPrivateFrame(kTimestampOwnerIdentifier, _));
  // Derived from |kCodecConfig|.
  const char kExpectedEac3Setup[] = "zec3\x0\x0\x1\x4\x2B\x92\x8\x0";
  EXPECT_CALL(*mock_id3_tag,
              AddPrivateFrame(kAudioDescriptionOwnerIdentifier,
                              std::string(std::begin(kExpectedEac3Setup),
                                          std::end(kExpectedEac3Setup) - 1)));
  EXPECT_CALL(*mock_id3_tag, WriteToBuffer(_)).WillOnce(Return(true));
  EXPECT_CALL(segmenter_, CreateId3Tag())
      .WillOnce(Return(ByMove(std::move(mock_id3_tag))));

  ASSERT_OK(
      segmenter_.AddSample(*CreateEncryptedSample(kPts1, kDts1, kSample1Data)));
}

}  // namespace media
}  // namespace shaka
