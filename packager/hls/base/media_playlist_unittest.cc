// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/file/file.h"
#include "packager/hls/base/media_playlist.h"

namespace shaka {
namespace hls {

using ::testing::_;
using ::testing::ReturnArg;

namespace {

const char kDefaultPlaylistFileName[] = "default_playlist.m3u8";

class MockFile : public media::File {
 public:
  MockFile() : File(kDefaultPlaylistFileName) {}
  MOCK_METHOD0(Close, bool());
  MOCK_METHOD2(Read, int64_t(void* buffer, uint64_t length));
  MOCK_METHOD2(Write,int64_t(const void* buffer, uint64_t length));
  MOCK_METHOD0(Size, int64_t());
  MOCK_METHOD0(Flush, bool());
  MOCK_METHOD1(Seek, bool(uint64_t position));
  MOCK_METHOD1(Tell, bool(uint64_t* position));

 private:
  MOCK_METHOD0(Open, bool());
};

MATCHER_P(MatchesString, expected_string, "") {
  const std::string arg_string(static_cast<const char*>(arg));
  *result_listener << "which is " << arg_string.size()
                   << " long and the content is " << arg_string;
  return expected_string == std::string(static_cast<const char*>(arg));
}

}  // namespace

class MediaPlaylistTest : public ::testing::Test {
 protected:
  MediaPlaylistTest()
      : default_file_name_(kDefaultPlaylistFileName),
        default_name_("default_name"),
        default_group_id_("default_group_id"),
        media_playlist_(MediaPlaylist::MediaPlaylistType::kVod,
                        default_file_name_,
                        default_name_,
                        default_group_id_) {}

  void SetUp() override {
    MediaInfo::VideoInfo* video_info =
        valid_video_media_info_.mutable_video_info();
    video_info->set_codec("avc1");
    video_info->set_time_scale(90000);
    video_info->set_frame_duration(3000);
    video_info->set_width(1280);
    video_info->set_height(720);
    video_info->set_pixel_width(1);
    video_info->set_pixel_height(1);
  }

  const std::string default_file_name_;
  const std::string default_name_;
  const std::string default_group_id_;
  MediaPlaylist media_playlist_;

  MediaInfo valid_video_media_info_;
};

// Verify that SetMediaInfo() fails if timescale is not present.
TEST_F(MediaPlaylistTest, NoTimeScale) {
  MediaInfo media_info;
  EXPECT_FALSE(media_playlist_.SetMediaInfo(media_info));
}

// The current implementation only handles video and audio.
TEST_F(MediaPlaylistTest, NoAudioOrVideo) {
  MediaInfo media_info;
  media_info.set_reference_time_scale(90000);
  MediaInfo::TextInfo* text_info = media_info.mutable_text_info();
  text_info->set_format("vtt");
  EXPECT_FALSE(media_playlist_.SetMediaInfo(media_info));
}

TEST_F(MediaPlaylistTest, SetMediaInfo) {
  MediaInfo media_info;
  media_info.set_reference_time_scale(90000);
  MediaInfo::VideoInfo* video_info = media_info.mutable_video_info();
  video_info->set_width(1280);
  video_info->set_height(720);
  EXPECT_TRUE(media_playlist_.SetMediaInfo(media_info));
}

// Verify that AddSegment works (not crash).
TEST_F(MediaPlaylistTest, AddSegment) {
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
}

// Verify that AddEncryptionInfo works (not crash).
TEST_F(MediaPlaylistTest, AddEncryptionInfo) {
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));
  media_playlist_.AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                                    "http://example.com", "0xabcedf", "", "");
}

TEST_F(MediaPlaylistTest, WriteToFile) {
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));
  const std::string kExpectedOutput =
      "#EXTM3U\n"
      "#EXT-X-VERSION:5\n"
      "#EXT-X-TARGETDURATION:0\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXT-X-ENDLIST\n";

  MockFile file;
  EXPECT_CALL(file,
              Write(MatchesString(kExpectedOutput), kExpectedOutput.size()))
      .WillOnce(ReturnArg<1>());
  EXPECT_TRUE(media_playlist_.WriteToFile(&file));
}

// If bitrate (bandwidth) is not set in the MediaInfo, use it.
TEST_F(MediaPlaylistTest, UseBitrateInMediaInfo) {
  valid_video_media_info_.set_bandwidth(8191);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));
  EXPECT_EQ(8191u, media_playlist_.Bitrate());
}

// If bitrate (bandwidth) is not set in the MediaInfo, then calculate from the
// segments.
TEST_F(MediaPlaylistTest, GetBitrateFromSegments) {
  valid_video_media_info_.clear_bandwidth();

  valid_video_media_info_.set_reference_time_scale(90000);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));

  // 10 seconds, 1MB.
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
  // 20 seconds, 5MB.
  media_playlist_.AddSegment("file2.ts", 1800000, 5000000);

  // 200KB per second which is 1600K bits / sec.
  EXPECT_EQ(1600000u, media_playlist_.Bitrate());
}

TEST_F(MediaPlaylistTest, GetLongestSegmentDuration) {
  valid_video_media_info_.set_reference_time_scale(90000);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));

  // 10 seconds.
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
  // 30 seconds.
  media_playlist_.AddSegment("file2.ts", 2700000, 5000000);
  // 14 seconds.
  media_playlist_.AddSegment("file3.ts", 1260000, 3000000);

  EXPECT_NEAR(30.0, media_playlist_.GetLongestSegmentDuration(), 0.01);
}

TEST_F(MediaPlaylistTest, SetTargetDuration) {
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));
  EXPECT_TRUE(media_playlist_.SetTargetDuration(20));
  const std::string kExpectedOutput =
      "#EXTM3U\n"
      "#EXT-X-VERSION:5\n"
      "#EXT-X-TARGETDURATION:20\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXT-X-ENDLIST\n";

  MockFile file;
  EXPECT_CALL(file,
              Write(MatchesString(kExpectedOutput), kExpectedOutput.size()))
      .WillOnce(ReturnArg<1>());
  EXPECT_TRUE(media_playlist_.WriteToFile(&file));

  // Cannot set target duration more than once.
  EXPECT_FALSE(media_playlist_.SetTargetDuration(20));
  EXPECT_FALSE(media_playlist_.SetTargetDuration(10));
}

TEST_F(MediaPlaylistTest, WriteToFileWithSegments) {
  valid_video_media_info_.set_reference_time_scale(90000);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));

  // 10 seconds.
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
  // 30 seconds.
  media_playlist_.AddSegment("file2.ts", 2700000, 5000000);
  const std::string kExpectedOutput =
      "#EXTM3U\n"
      "#EXT-X-VERSION:5\n"
      "#EXT-X-TARGETDURATION:30\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXTINF:10.000,\n"
      "file1.ts\n"
      "#EXTINF:30.000,\n"
      "file2.ts\n"
      "#EXT-X-ENDLIST\n";

  MockFile file;
  EXPECT_CALL(file,
              Write(MatchesString(kExpectedOutput), kExpectedOutput.size()))
      .WillOnce(ReturnArg<1>());
  EXPECT_TRUE(media_playlist_.WriteToFile(&file));
}

TEST_F(MediaPlaylistTest, WriteToFileWithEncryptionInfo) {
  valid_video_media_info_.set_reference_time_scale(90000);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));

  media_playlist_.AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                                    "http://example.com", "0x12345678",
                                    "com.widevine", "1/2/4");
  // 10 seconds.
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
  // 30 seconds.
  media_playlist_.AddSegment("file2.ts", 2700000, 5000000);
  const std::string kExpectedOutput =
      "#EXTM3U\n"
      "#EXT-X-VERSION:5\n"
      "#EXT-X-TARGETDURATION:30\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXT-X-KEY:METHOD=SAMPLE-AES,"
      "URI=\"http://example.com\",IV=0x12345678,KEYFORMATVERSIONS=\"1/2/4\","
      "KEYFORMAT=\"com.widevine\"\n"
      "#EXTINF:10.000,\n"
      "file1.ts\n"
      "#EXTINF:30.000,\n"
      "file2.ts\n"
      "#EXT-X-ENDLIST\n";

  MockFile file;
  EXPECT_CALL(file,
              Write(MatchesString(kExpectedOutput), kExpectedOutput.size()))
      .WillOnce(ReturnArg<1>());
  EXPECT_TRUE(media_playlist_.WriteToFile(&file));
}

TEST_F(MediaPlaylistTest, WriteToFileWithEncryptionInfoEmptyIv) {
  valid_video_media_info_.set_reference_time_scale(90000);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));

  media_playlist_.AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                                    "http://example.com", "", "com.widevine",
                                    "");
  // 10 seconds.
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
  // 30 seconds.
  media_playlist_.AddSegment("file2.ts", 2700000, 5000000);
  const std::string kExpectedOutput =
      "#EXTM3U\n"
      "#EXT-X-VERSION:5\n"
      "#EXT-X-TARGETDURATION:30\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXT-X-KEY:METHOD=SAMPLE-AES,"
      "URI=\"http://example.com\",KEYFORMAT=\"com.widevine\"\n"
      "#EXTINF:10.000,\n"
      "file1.ts\n"
      "#EXTINF:30.000,\n"
      "file2.ts\n"
      "#EXT-X-ENDLIST\n";

  MockFile file;
  EXPECT_CALL(file,
              Write(MatchesString(kExpectedOutput), kExpectedOutput.size()))
      .WillOnce(ReturnArg<1>());
  EXPECT_TRUE(media_playlist_.WriteToFile(&file));
}

TEST_F(MediaPlaylistTest, RemoveOldestSegment) {
  valid_video_media_info_.set_reference_time_scale(90000);
  ASSERT_TRUE(media_playlist_.SetMediaInfo(valid_video_media_info_));

  // 10 seconds.
  media_playlist_.AddSegment("file1.ts", 900000, 1000000);
  // 30 seconds.
  media_playlist_.AddSegment("file2.ts", 2700000, 5000000);
  media_playlist_.RemoveOldestSegment();

  const std::string kExpectedOutput =
      "#EXTM3U\n"
      "#EXT-X-VERSION:5\n"
      "#EXT-X-TARGETDURATION:30\n"
      "#EXT-X-PLAYLIST-TYPE:VOD\n"
      "#EXTINF:30.000,\n"
      "file2.ts\n"
      "#EXT-X-ENDLIST\n";

  MockFile file;
  EXPECT_CALL(file,
              Write(MatchesString(kExpectedOutput), kExpectedOutput.size()))
      .WillOnce(ReturnArg<1>());
  EXPECT_TRUE(media_playlist_.WriteToFile(&file));
}

}  // namespace hls
}  // namespace shaka
