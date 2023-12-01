// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/wvm/wvm_media_parser.h>

#include <algorithm>
#include <functional>
#include <string>

#include <absl/log/log.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/macros/classes.h>
#include <packager/macros/logging.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/raw_key_source.h>
#include <packager/media/base/request_signer.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/base/timestamp.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/test/test_data_util.h>

namespace {
const char kWvmFile[] = "bear-640x360.wvm";
// Constants associated with kWvmFile follows.
const uint32_t kExpectedStreams = 4;
const int kExpectedVideoFrameCount = 826;
const int kExpectedAudioFrameCount = 1184;
const int kExpectedEncryptedSampleCount = 554;
const uint8_t kExpectedAssetKey[] =
    "\x92\x48\xd2\x45\x39\x0e\x0a\x49\xd4\x83\xba\x9b\x43\xfc\x69\xc3";
const uint8_t k64ByteAssetKey[] =
    "\x92\x48\xd2\x45\x39\x0e\x0a\x49\xd4\x83\xba\x9b\x43\xfc\x69\xc3"
    "\x92\x48\xd2\x45\x39\x0e\x0a\x49\xd4\x83\xba\x9b\x43\xfc\x69\xc3"
    "\x92\x48\xd2\x45\x39\x0e\x0a\x49\xd4\x83\xba\x9b\x43\xfc\x69\xc3"
    "\x92\x48\xd2\x45\x39\x0e\x0a\x49\xd4\x83\xba\x9b\x43\xfc\x69\xc3";
const size_t kInitDataSize = 0x4000;
const char kMultiConfigWvmFile[] = "bear-multi-configs.wvm";
}  // namespace

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace shaka {
namespace media {

class MockKeySource : public RawKeySource {
 public:
  MockKeySource() {}
  ~MockKeySource() override {}

  MOCK_METHOD2(FetchKeys,
               Status(EmeInitDataType init_data_type,
                      const std::vector<uint8_t>& init_data));
  MOCK_METHOD2(GetKey,
               Status(const std::string& stream_label, EncryptionKey* key));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeySource);
};

namespace wvm {

class WvmMediaParserTest : public testing::Test {
 public:
  WvmMediaParserTest()
      : audio_frame_count_(0),
        video_frame_count_(0),
        encrypted_sample_count_(0),
        video_max_dts_(kNoTimestamp),
        current_track_id_(-1) {
    parser_.reset(new WvmMediaParser());
    key_source_.reset(new MockKeySource());
    encryption_key_.key.resize(16);
    encryption_key_.key.assign(kExpectedAssetKey, kExpectedAssetKey + 16);
  }

 protected:
  typedef std::map<int, std::shared_ptr<StreamInfo>> StreamMap;

  std::unique_ptr<WvmMediaParser> parser_;
  std::unique_ptr<MockKeySource> key_source_;
  StreamMap stream_map_;
  int audio_frame_count_;
  int video_frame_count_;
  int encrypted_sample_count_;
  int64_t video_max_dts_;
  int32_t current_track_id_;
  EncryptionKey encryption_key_;

  void OnInit(const std::vector<std::shared_ptr<StreamInfo>>& stream_infos) {
    DVLOG(1) << "OnInit: " << stream_infos.size() << " streams.";
    for (const auto& stream_info : stream_infos) {
      DVLOG(1) << stream_info->ToString();
      stream_map_[stream_info->track_id()] = stream_info;
    }
  }

  bool OnNewSample(uint32_t track_id, std::shared_ptr<MediaSample> sample) {
    std::string stream_type;
    if (static_cast<int32_t>(track_id) != current_track_id_) {
      // onto next track.
      video_max_dts_ = kNoTimestamp;
      current_track_id_ = track_id;
    }
    StreamMap::const_iterator stream = stream_map_.find(track_id);
    if (stream != stream_map_.end()) {
      if (stream->second->stream_type() == kStreamAudio) {
        ++audio_frame_count_;
        stream_type = "audio";
      } else if (stream->second->stream_type() == kStreamVideo) {
        ++video_frame_count_;
        stream_type = "video";
        // Verify timestamps are increasing.
        if (video_max_dts_ == kNoTimestamp) {
          video_max_dts_ = sample->dts();
        } else if (video_max_dts_ >= sample->dts()) {
          LOG(ERROR) << "Video DTS not strictly increasing for track = "
                     << track_id << ", video max dts = "
                     << video_max_dts_ << ", sample dts = "
                     << sample->dts();
          return false;
        }
        video_max_dts_ = sample->dts();
      } else {
        LOG(ERROR) << "Missing StreamInfo for track ID " << track_id;
        return false;
      }
    }

    if (sample->is_encrypted()) {
      ++encrypted_sample_count_;
    }
    return true;
  }

  bool OnNewTextSample(uint32_t track_id, std::shared_ptr<TextSample> sample) {
    return false;
  }

  void InitializeParser() {
    parser_->Init(
        std::bind(&WvmMediaParserTest::OnInit, this, std::placeholders::_1),
        std::bind(&WvmMediaParserTest::OnNewSample, this, std::placeholders::_1,
                  std::placeholders::_2),
        std::bind(&WvmMediaParserTest::OnNewTextSample, this,
                  std::placeholders::_1, std::placeholders::_2),
        key_source_.get());
  }

  void Parse(const std::string& filename) {
    InitializeParser();

    std::vector<uint8_t> buffer = ReadTestDataFile(filename);
    ASSERT_FALSE(buffer.empty());

    EXPECT_TRUE(parser_->Parse(buffer.data(), static_cast<int>(buffer.size())));
  }
};

TEST_F(WvmMediaParserTest, ParseWvmWithoutKeySource) {
  key_source_.reset();
  InitializeParser();

  std::vector<uint8_t> buffer = ReadTestDataFile(kWvmFile);
  ASSERT_FALSE(buffer.empty());

  EXPECT_TRUE(parser_->Parse(buffer.data(), static_cast<int>(buffer.size())));
  EXPECT_EQ(kExpectedStreams, stream_map_.size());
  EXPECT_EQ(kExpectedVideoFrameCount, video_frame_count_);
  EXPECT_EQ(kExpectedAudioFrameCount, audio_frame_count_);
  EXPECT_EQ(kExpectedEncryptedSampleCount, encrypted_sample_count_);

  // Also verify that the pixel width and height have the right values.
  // Track 0 and 2 are videos and they both have pixel_width = 8 and
  // pixel_height = 9.
  EXPECT_EQ(8u, reinterpret_cast<VideoStreamInfo*>(stream_map_[0].get())
                    ->pixel_width());
  EXPECT_EQ(8u, reinterpret_cast<VideoStreamInfo*>(stream_map_[2].get())
                    ->pixel_width());
  EXPECT_EQ(9u, reinterpret_cast<VideoStreamInfo*>(stream_map_[0].get())
                    ->pixel_height());
  EXPECT_EQ(9u, reinterpret_cast<VideoStreamInfo*>(stream_map_[2].get())
                    ->pixel_height());
}

TEST_F(WvmMediaParserTest, ParseWvmInitWithoutKeySource) {
  key_source_.reset();
  InitializeParser();

  std::vector<uint8_t> buffer = ReadTestDataFile(kWvmFile);
  ASSERT_FALSE(buffer.empty());

  EXPECT_TRUE(parser_->Parse(buffer.data(), kInitDataSize));
  EXPECT_EQ(kExpectedStreams, stream_map_.size());
}

TEST_F(WvmMediaParserTest, ParseWvm) {
  EXPECT_CALL(*key_source_, FetchKeys(_, _)).WillOnce(Return(Status::OK));
  EXPECT_CALL(*key_source_, GetKey(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key_), Return(Status::OK)));
  Parse(kWvmFile);
  EXPECT_EQ(kExpectedStreams, stream_map_.size());
  EXPECT_EQ(kExpectedVideoFrameCount, video_frame_count_);
  EXPECT_EQ(kExpectedAudioFrameCount, audio_frame_count_);
  EXPECT_EQ(0, encrypted_sample_count_);
}

TEST_F(WvmMediaParserTest, ParseWvmWith64ByteAssetKey) {
  EXPECT_CALL(*key_source_, FetchKeys(_, _)).WillOnce(Return(Status::OK));
  // WVM uses only the first 16 bytes of the asset key.
  encryption_key_.key.resize(64);
  encryption_key_.key.assign(k64ByteAssetKey, k64ByteAssetKey + 64);
  EXPECT_CALL(*key_source_, GetKey(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key_), Return(Status::OK)));
  Parse(kWvmFile);
  EXPECT_EQ(kExpectedStreams, stream_map_.size());
  EXPECT_EQ(kExpectedVideoFrameCount, video_frame_count_);
  EXPECT_EQ(kExpectedAudioFrameCount, audio_frame_count_);
}

TEST_F(WvmMediaParserTest, ParseMultiConfigWvm) {
  EXPECT_CALL(*key_source_, FetchKeys(_, _)).WillOnce(Return(Status::OK));
  EXPECT_CALL(*key_source_, GetKey(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(encryption_key_), Return(Status::OK)));
  Parse(kMultiConfigWvmFile);
  ASSERT_EQ(4u, stream_map_.size());

  ASSERT_EQ(kStreamVideo, stream_map_[0]->stream_type());
  VideoStreamInfo* video_info = reinterpret_cast<VideoStreamInfo*>(
      stream_map_[0].get());
  EXPECT_EQ("avc1.64000d", video_info->codec_string());
  EXPECT_EQ(320u, video_info->width());
  EXPECT_EQ(180u, video_info->height());

  ASSERT_EQ(kStreamAudio, stream_map_[1]->stream_type());
  AudioStreamInfo* audio_info = reinterpret_cast<AudioStreamInfo*>(
      stream_map_[1].get());
  EXPECT_EQ("mp4a.40.2", audio_info->codec_string());
  EXPECT_EQ(2u, audio_info->num_channels());
  EXPECT_EQ(44100u, audio_info->sampling_frequency());

  ASSERT_EQ(kStreamVideo, stream_map_[2]->stream_type());
  video_info = reinterpret_cast<VideoStreamInfo*>(stream_map_[2].get());
  EXPECT_EQ("avc1.64001e", video_info->codec_string());
  EXPECT_EQ(640u, video_info->width());
  EXPECT_EQ(360u, video_info->height());

  ASSERT_EQ(kStreamAudio, stream_map_[3]->stream_type());
  audio_info = reinterpret_cast<AudioStreamInfo*>(stream_map_[3].get());
  EXPECT_EQ("mp4a.40.2", audio_info->codec_string());
  EXPECT_EQ(2u, audio_info->num_channels());
  EXPECT_EQ(44100u, audio_info->sampling_frequency());
}

}  // namespace wvm
}  // namespace media
}  // namespace shaka
