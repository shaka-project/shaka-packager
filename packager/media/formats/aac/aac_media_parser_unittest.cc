// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// @steered SNARE-2 2026-09-22

#include <packager/media/formats/aac/aac_media_parser.h>

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/stream_info.h>
#include <packager/utils/hex_parser.h>

namespace shaka {
namespace media {
namespace aac {

namespace {

// A single valid ADTS frame: AAC-LC, 44100 Hz, 2 channels.
// (Reused from the mp2t AdtsHeader unit test.)
const char kValidAdtsFrame[] =
    "fff15080429ffcda004c61766335332e33352e30004258892a5361062403"
    "d040000000001ff9055e9fe77ac56eb1677484e0ef0c3102a39daa8355a5"
    "37ecab2b156e4ba73ceedb24ea51194e57c9385fa67eca8edc914902e852"
    "3185b52299516e679fb3768aa9f13ccac5b257410080282c9a50318ec94e"
    "ba24ea305bafab2b2beab16557ef9ecaa8f17bedea84788c8d42e4b3c65b"
    "1e7ecae7528b909bc46c76cca73b906ec980ed9f32b25ecd28f43f9516de"
    "3ff249f23bb9c93e64c4808195f284653c40592c1a8dc847f5f11791fd80"
    "b18e02c1e1ed9f82c62a1f8ea0f5b6dbf2112c2202973b00de71bb49f906"
    "ed1bc63768dda378c8f9c6ed1bb48f68dda378c9f68dda3768dda3768de3"
    "23da3768de31bb492a5361062403d040000000001ff9055e9fe77ac56eb1"
    "677484e0ef0c3102a39daa8355a537ecab2b156e4ba73ceedb24ea51194e"
    "57c9385fa67eca8edc914902e8523185b52299516e679fb3768aa9f13cca"
    "c5b257410080282c9a50318ec94eba24ea305bafab2b2beab16557ef9eca"
    "a8f17bedea84788c8d42e4b3c65b1e7ecae7528b909bc46c76cca73b906e"
    "c980ed9f32b25ecd28f43f9516de3ff249f23bb9c93e64c4808195f28465"
    "3c40592c1a8dc847f5f11791fd80b18e02c1e1ed9f82c62a1f8ea0f5b6db"
    "f2112c2202973b00de71bb49f906ed1bc63768dda378c8f9c6ed1bb48f68"
    "dda378c9f68dda3768dda3768de323da3768de31bb4e";

const uint32_t kExpectedSamplingFrequency = 44100;
const uint8_t kExpectedNumChannels = 2;

}  // namespace

class AacMediaParserTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(shaka::ValidHexStringToBytes(kValidAdtsFrame, &adts_frame_));
    parser_ = std::make_unique<AacMediaParser>();
    parser_->Init(
        [this](const std::vector<std::shared_ptr<StreamInfo>>& stream_infos) {
          stream_infos_ = stream_infos;
        },
        [this](uint32_t track_id, std::shared_ptr<MediaSample> sample) {
          samples_.push_back(sample);
          return true;
        },
        [](uint32_t, std::shared_ptr<TextSample>) { return true; },
        /*decryption_key_source=*/nullptr);
  }

 protected:
  std::vector<uint8_t> adts_frame_;
  std::unique_ptr<AacMediaParser> parser_;
  std::vector<std::shared_ptr<StreamInfo>> stream_infos_;
  std::vector<std::shared_ptr<MediaSample>> samples_;
};

TEST_F(AacMediaParserTest, SingleFrameStreamInfo) {
  ASSERT_TRUE(
      parser_->Parse(adts_frame_.data(), static_cast<int>(adts_frame_.size())));
  ASSERT_TRUE(parser_->Flush());

  ASSERT_EQ(1u, stream_infos_.size());
  EXPECT_EQ(kStreamAudio, stream_infos_[0]->stream_type());
  const AudioStreamInfo* audio_info =
      static_cast<const AudioStreamInfo*>(stream_infos_[0].get());
  EXPECT_EQ(kCodecAAC, audio_info->codec());
  EXPECT_EQ(kExpectedSamplingFrequency, audio_info->sampling_frequency());
  EXPECT_EQ(kExpectedNumChannels, audio_info->num_channels());

  ASSERT_EQ(1u, samples_.size());
  // The emitted sample should have the 7-byte ADTS header stripped.
  EXPECT_EQ(adts_frame_.size() - 7, samples_[0]->data_size());
  EXPECT_TRUE(samples_[0]->is_key_frame());
  EXPECT_EQ(0, samples_[0]->pts());
}

TEST_F(AacMediaParserTest, MultipleFramesTimestamps) {
  // Two identical frames back to back.
  std::vector<uint8_t> two_frames(adts_frame_);
  two_frames.insert(two_frames.end(), adts_frame_.begin(), adts_frame_.end());

  ASSERT_TRUE(
      parser_->Parse(two_frames.data(), static_cast<int>(two_frames.size())));
  ASSERT_TRUE(parser_->Flush());

  ASSERT_EQ(1u, stream_infos_.size());
  ASSERT_EQ(2u, samples_.size());

  // First frame starts at 0; second frame is offset by one AAC frame duration
  // of 1024 samples. The media timescale equals the sampling frequency, so this
  // is exactly 1024 ticks with no rounding.
  const int64_t kExpectedSecondPts = 1024;
  EXPECT_EQ(0, samples_[0]->pts());
  EXPECT_EQ(kExpectedSecondPts, samples_[1]->pts());
}

TEST_F(AacMediaParserTest, ResyncsPastLeadingGarbage) {
  // Prepend bytes that are not an ADTS syncword.
  std::vector<uint8_t> with_prefix = {0x00, 0x01, 0x02, 0x03};
  with_prefix.insert(with_prefix.end(), adts_frame_.begin(), adts_frame_.end());

  ASSERT_TRUE(
      parser_->Parse(with_prefix.data(), static_cast<int>(with_prefix.size())));
  ASSERT_TRUE(parser_->Flush());

  ASSERT_EQ(1u, stream_infos_.size());
  ASSERT_EQ(1u, samples_.size());
}

TEST_F(AacMediaParserTest, RejectsFalsePositiveSyncwordBeforeValidFrame) {
  // A false-positive ADTS syncword (0xFFF...) whose frame_size field points
  // into garbage that is NOT followed by a real syncword. The parser must not
  // emit a sample for it; it should resync to the genuine frame that follows.
  //
  // Bytes: syncword 0xFFF1, then a header claiming a small frame_size (8), then
  // filler that does not begin with a syncword at the claimed frame end.
  std::vector<uint8_t> fake = {0xff, 0xf1, 0x00, 0x00, 0x01, 0x1f,
                               0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
  std::vector<uint8_t> stream(fake);
  stream.insert(stream.end(), adts_frame_.begin(), adts_frame_.end());

  ASSERT_TRUE(parser_->Parse(stream.data(), static_cast<int>(stream.size())));
  ASSERT_TRUE(parser_->Flush());

  // Only the single genuine frame should be emitted.
  ASSERT_EQ(1u, stream_infos_.size());
  ASSERT_EQ(1u, samples_.size());
  EXPECT_EQ(adts_frame_.size() - 7, samples_[0]->data_size());
}

TEST_F(AacMediaParserTest, HandlesSplitFrameAcrossParseCalls) {
  const int split = static_cast<int>(adts_frame_.size()) / 2;
  ASSERT_TRUE(parser_->Parse(adts_frame_.data(), split));
  // Not enough data for a full frame yet.
  EXPECT_TRUE(samples_.empty());

  ASSERT_TRUE(parser_->Parse(adts_frame_.data() + split,
                             static_cast<int>(adts_frame_.size()) - split));
  ASSERT_TRUE(parser_->Flush());

  ASSERT_EQ(1u, stream_infos_.size());
  ASSERT_EQ(1u, samples_.size());
}

}  // namespace aac
}  // namespace media
}  // namespace shaka
