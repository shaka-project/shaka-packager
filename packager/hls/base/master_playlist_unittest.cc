// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_path.h"
#include "packager/file/file.h"
#include "packager/hls/base/master_playlist.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/hls/base/mock_media_playlist.h"
#include "packager/version/version.h"

namespace shaka {
namespace hls {

using base::FilePath;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::StrEq;

namespace {
const char kDefaultMasterPlaylistName[] = "playlist.m3u8";
const char kDefaultLanguage[] = "en";
const uint32_t kWidth = 800;
const uint32_t kHeight = 600;
const HlsPlaylistType kVodPlaylist = HlsPlaylistType::kVod;

std::unique_ptr<MockMediaPlaylist> CreateVideoPlaylist(
    const std::string& filename,
    const std::string& codec,
    uint64_t bitrate) {
  const char kNoName[] = "";
  const char kNoGroup[] = "";

  std::unique_ptr<MockMediaPlaylist> playlist(
      new MockMediaPlaylist(kVodPlaylist, filename, kNoName, kNoGroup));

  playlist->SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kVideo);
  playlist->SetCodecForTesting(codec);

  EXPECT_CALL(*playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(bitrate));
  EXPECT_CALL(*playlist, GetDisplayResolution(NotNull(), NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<0>(kWidth), SetArgPointee<1>(kHeight),
                            Return(true)));

  return playlist;
}

std::unique_ptr<MockMediaPlaylist> CreateAudioPlaylist(
    const std::string& filename,
    const std::string& name,
    const std::string& group,
    const std::string& codec,
    const std::string& language,
    uint64_t channels,
    uint64_t bitrate) {
  // Note that audiocodecs should match for different audio tracks with same
  // group ID.
  std::unique_ptr<MockMediaPlaylist> playlist(
      new MockMediaPlaylist(kVodPlaylist, filename, name, group));

  EXPECT_CALL(*playlist, GetLanguage()).WillRepeatedly(Return(language));
  EXPECT_CALL(*playlist, GetNumChannels()).WillRepeatedly(Return(channels));

  playlist->SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kAudio);
  playlist->SetCodecForTesting(codec);

  EXPECT_CALL(*playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(bitrate));
  EXPECT_CALL(*playlist, GetDisplayResolution(NotNull(), NotNull())).Times(0);

  return playlist;
}
}  // namespace

class MasterPlaylistTest : public ::testing::Test {
 protected:
  MasterPlaylistTest()
      : master_playlist_(kDefaultMasterPlaylistName, kDefaultLanguage),
        test_output_dir_("memory://test_dir"),
        master_playlist_path_(
            FilePath::FromUTF8Unsafe(test_output_dir_)
                .Append(FilePath::FromUTF8Unsafe(kDefaultMasterPlaylistName))
                .AsUTF8Unsafe()) {}

  void SetUp() override { SetPackagerVersionForTesting("test"); }

  MasterPlaylist master_playlist_;
  std::string test_output_dir_;
  std::string master_playlist_path_;
};

TEST_F(MasterPlaylistTest, AddMediaPlaylist) {
  MockMediaPlaylist mock_playlist(kVodPlaylist, "playlist1.m3u8", "somename",
                                  "somegroupid");
  master_playlist_.AddMediaPlaylist(&mock_playlist);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistOneVideo) {
  const uint64_t kBitRate = 435889;

  std::unique_ptr<MockMediaPlaylist> mock_playlist =
      CreateVideoPlaylist("media1.m3u8", "avc1", kBitRate);
  master_playlist_.AddMediaPlaylist(mock_playlist.get());

  const char kBaseUrl[] = "http://myplaylistdomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=435889,CODECS=\"avc1\",RESOLUTION=800x600\n"
      "http://myplaylistdomain.com/media1.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideoAndAudio) {
  const uint64_t kVideo1BitRate = 300000;
  const uint64_t kVideo2BitRate = 700000;

  const uint64_t kAudio1BitRate = 50000;
  const uint64_t kAudio2BitRate = 60000;

  const uint64_t kAudio1Channels = 2;
  const uint64_t kAudio2Channels = 5;

  // First video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> sd_video_playlist =
      CreateVideoPlaylist("sd.m3u8", "sdvideocodec", kVideo1BitRate);
  master_playlist_.AddMediaPlaylist(sd_video_playlist.get());

  // Second video, hd.m3u8.
  std::unique_ptr<MockMediaPlaylist> hd_video_playlist =
      CreateVideoPlaylist("hd.m3u8", "hdvideocodec", kVideo2BitRate);
  master_playlist_.AddMediaPlaylist(hd_video_playlist.get());

  // First audio, english.m3u8.
  std::unique_ptr<MockMediaPlaylist> english_playlist =
      CreateAudioPlaylist("eng.m3u8", "english", "audiogroup", "audiocodec",
                          "en", kAudio1Channels, kAudio1BitRate);
  master_playlist_.AddMediaPlaylist(english_playlist.get());

  // Second audio, spanish.m3u8.
  std::unique_ptr<MockMediaPlaylist> spanish_playlist =
      CreateAudioPlaylist("spa.m3u8", "espanol", "audiogroup", "audiocodec",
                          "es", kAudio2Channels, kAudio2BitRate);
  master_playlist_.AddMediaPlaylist(spanish_playlist.get());

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"audiogroup\",LANGUAGE=\"en\",NAME=\"english\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"2\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/spa.m3u8\","
      "GROUP-ID=\"audiogroup\",LANGUAGE=\"es\",NAME=\"espanol\","
      "AUTOSELECT=YES,CHANNELS=\"5\"\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=360000,CODECS=\"sdvideocodec,audiocodec\","
      "RESOLUTION=800x600,AUDIO=\"audiogroup\"\n"
      "http://playlists.org/sd.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=760000,CODECS=\"hdvideocodec,audiocodec\","
      "RESOLUTION=800x600,AUDIO=\"audiogroup\"\n"
      "http://playlists.org/hd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistMultipleAudioGroups) {
  const uint64_t kVideoBitRate = 300000;

  const uint64_t kAudio1BitRate = 50000;
  const uint64_t kAudio2BitRate = 100000;

  const uint64_t kAudio1Channels = 1;
  const uint64_t kAudio2Channels = 8;

  // First video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video_playlist =
      CreateVideoPlaylist("video.m3u8", "videocodec", kVideoBitRate);
  master_playlist_.AddMediaPlaylist(video_playlist.get());

  // First audio, eng_lo.m3u8.
  std::unique_ptr<MockMediaPlaylist> eng_lo_playlist = CreateAudioPlaylist(
      "eng_lo.m3u8", "english_lo", "audio_lo", "audiocodec_lo", "en",
      kAudio1Channels, kAudio1BitRate);
  master_playlist_.AddMediaPlaylist(eng_lo_playlist.get());

  // Second audio, eng_hi.m3u8.
  std::unique_ptr<MockMediaPlaylist> eng_hi_playlist = CreateAudioPlaylist(
      "eng_hi.m3u8", "english_hi", "audio_hi", "audiocodec_hi", "en",
      kAudio2Channels, kAudio2BitRate);
  master_playlist_.AddMediaPlaylist(eng_hi_playlist.get());

  const char kBaseUrl[] = "http://anydomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_hi.m3u8\","
      "GROUP-ID=\"audio_hi\",LANGUAGE=\"en\",NAME=\"english_hi\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"8\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_lo.m3u8\","
      "GROUP-ID=\"audio_lo\",LANGUAGE=\"en\",NAME=\"english_lo\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"1\"\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=400000,CODECS=\"videocodec,audiocodec_hi\","
      "RESOLUTION=800x600,AUDIO=\"audio_hi\"\n"
      "http://anydomain.com/video.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,CODECS=\"videocodec,audiocodec_lo\","
      "RESOLUTION=800x600,AUDIO=\"audio_lo\"\n"
      "http://anydomain.com/video.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistSameAudioGroupSameLanguage) {
  // First video, video.m3u8.
  std::unique_ptr<MockMediaPlaylist> video_playlist =
      CreateVideoPlaylist("video.m3u8", "videocodec", 300000);
  master_playlist_.AddMediaPlaylist(video_playlist.get());

  // First audio, eng_lo.m3u8.
  std::unique_ptr<MockMediaPlaylist> eng_lo_playlist = CreateAudioPlaylist(
      "eng_lo.m3u8", "english", "audio", "audiocodec", "en", 1, 50000);
  master_playlist_.AddMediaPlaylist(eng_lo_playlist.get());

  std::unique_ptr<MockMediaPlaylist> eng_hi_playlist = CreateAudioPlaylist(
      "eng_hi.m3u8", "english", "audio", "audiocodec", "en", 8, 100000);
  master_playlist_.AddMediaPlaylist(eng_hi_playlist.get());

  const char kBaseUrl[] = "http://anydomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_lo.m3u8\","
      "GROUP-ID=\"audio\",LANGUAGE=\"en\",NAME=\"english\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"1\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_hi.m3u8\","
      "GROUP-ID=\"audio\",LANGUAGE=\"en\",NAME=\"english\","
      "CHANNELS=\"8\"\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=400000,CODECS=\"videocodec,audiocodec\","
      "RESOLUTION=800x600,AUDIO=\"audio\"\n"
      "http://anydomain.com/video.m3u8\n";

  ASSERT_EQ(expected, actual);
}
}  // namespace hls
}  // namespace shaka
