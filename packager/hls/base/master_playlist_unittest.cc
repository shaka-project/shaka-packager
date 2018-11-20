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
const char kDefaultAudioLanguage[] = "en";
const char kDefaultTextLanguage[] = "fr";
const uint32_t kWidth = 800;
const uint32_t kHeight = 600;

std::unique_ptr<MockMediaPlaylist> CreateVideoPlaylist(
    const std::string& filename,
    const std::string& codec,
    uint64_t max_bitrate,
    uint64_t avg_bitrate) {
  const char kNoName[] = "";
  const char kNoGroup[] = "";

  std::unique_ptr<MockMediaPlaylist> playlist(
      new MockMediaPlaylist(filename, kNoName, kNoGroup));

  playlist->SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kVideo);
  playlist->SetCodecForTesting(codec);

  EXPECT_CALL(*playlist, MaxBitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(max_bitrate));
  EXPECT_CALL(*playlist, AvgBitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(avg_bitrate));
  EXPECT_CALL(*playlist, GetDisplayResolution(NotNull(), NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<0>(kWidth), SetArgPointee<1>(kHeight),
                            Return(true)));

  return playlist;
}

std::unique_ptr<MockMediaPlaylist> CreateIframePlaylist(
    const std::string& filename,
    const std::string& codec,
    uint64_t max_bitrate,
    uint64_t avg_bitrate) {
  auto playlist =
      CreateVideoPlaylist(filename, codec, max_bitrate, avg_bitrate);
  playlist->SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kVideoIFramesOnly);
  return playlist;
}

std::unique_ptr<MockMediaPlaylist> CreateAudioPlaylist(
    const std::string& filename,
    const std::string& name,
    const std::string& group,
    const std::string& codec,
    const std::string& language,
    uint64_t channels,
    uint64_t max_bitrate,
    uint64_t avg_bitrate) {
  std::unique_ptr<MockMediaPlaylist> playlist(
      new MockMediaPlaylist(filename, name, group));

  EXPECT_CALL(*playlist, GetNumChannels()).WillRepeatedly(Return(channels));

  playlist->SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kAudio);
  playlist->SetCodecForTesting(codec);
  playlist->SetLanguageForTesting(language);

  EXPECT_CALL(*playlist, MaxBitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(max_bitrate));
  EXPECT_CALL(*playlist, AvgBitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(avg_bitrate));
  EXPECT_CALL(*playlist, GetDisplayResolution(NotNull(), NotNull())).Times(0);

  return playlist;
}

std::unique_ptr<MockMediaPlaylist> CreateTextPlaylist(
    const std::string& filename,
    const std::string& name,
    const std::string& group,
    const std::string& codec,
    const std::string& language) {
  std::unique_ptr<MockMediaPlaylist> playlist(
      new MockMediaPlaylist(filename, name, group));

  playlist->SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kSubtitle);
  playlist->SetCodecForTesting(codec);
  playlist->SetLanguageForTesting(language);

  return playlist;
}
}  // namespace

class MasterPlaylistTest : public ::testing::Test {
 protected:
  MasterPlaylistTest()
      : master_playlist_(kDefaultMasterPlaylistName,
                         kDefaultAudioLanguage,
                         kDefaultTextLanguage),
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

TEST_F(MasterPlaylistTest, WriteMasterPlaylistOneVideo) {
  const uint64_t kMaxBitrate = 435889;
  const uint64_t kAvgBitrate = 235889;

  std::unique_ptr<MockMediaPlaylist> mock_playlist =
      CreateVideoPlaylist("media1.m3u8", "avc1", kMaxBitrate, kAvgBitrate);

  const char kBaseUrl[] = "http://myplaylistdomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_,
                                                   {mock_playlist.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=435889,AVERAGE-BANDWIDTH=235889,"
      "CODECS=\"avc1\",RESOLUTION=800x600\n"
      "http://myplaylistdomain.com/media1.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistOneIframePlaylist) {
  const uint64_t kMaxBitrate = 435889;
  const uint64_t kAvgBitrate = 235889;

  std::unique_ptr<MockMediaPlaylist> mock_playlist =
      CreateIframePlaylist("media1.m3u8", "avc1", kMaxBitrate, kAvgBitrate);

  const char kBaseUrl[] = "http://myplaylistdomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_,
                                                   {mock_playlist.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=435889,AVERAGE-BANDWIDTH=235889,"
      "CODECS=\"avc1\",RESOLUTION=800x600,"
      "URI=\"http://myplaylistdomain.com/media1.m3u8\"\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideoAndAudio) {
  const uint64_t kVideo1MaxBitrate = 300000;
  const uint64_t kVideo1AvgBitrate = 200000;
  const uint64_t kVideo2MaxBitrate = 700000;
  const uint64_t kVideo2AvgBitrate = 400000;

  const uint64_t kAudio1MaxBitrate = 50000;
  const uint64_t kAudio1AvgBitrate = 40000;
  const uint64_t kAudio2MaxBitrate = 60000;
  const uint64_t kAudio2AvgBitrate = 30000;

  const uint64_t kAudio1Channels = 2;
  const uint64_t kAudio2Channels = 5;

  // First video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> sd_video_playlist = CreateVideoPlaylist(
      "sd.m3u8", "sdvideocodec", kVideo1MaxBitrate, kVideo1AvgBitrate);

  // Second video, hd.m3u8.
  std::unique_ptr<MockMediaPlaylist> hd_video_playlist = CreateVideoPlaylist(
      "hd.m3u8", "hdvideocodec", kVideo2MaxBitrate, kVideo2AvgBitrate);

  // First audio, english.m3u8.
  std::unique_ptr<MockMediaPlaylist> english_playlist = CreateAudioPlaylist(
      "eng.m3u8", "english", "audiogroup", "audiocodec", "en", kAudio1Channels,
      kAudio1MaxBitrate, kAudio1AvgBitrate);

  // Second audio, spanish.m3u8.
  std::unique_ptr<MockMediaPlaylist> spanish_playlist = CreateAudioPlaylist(
      "spa.m3u8", "espanol", "audiogroup", "audiocodec", "es", kAudio2Channels,
      kAudio2MaxBitrate, kAudio2AvgBitrate);

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(
      kBaseUrl, test_output_dir_,
      {sd_video_playlist.get(), hd_video_playlist.get(), english_playlist.get(),
       spanish_playlist.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"audiogroup\",LANGUAGE=\"en\",NAME=\"english\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"2\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/spa.m3u8\","
      "GROUP-ID=\"audiogroup\",LANGUAGE=\"es\",NAME=\"espanol\","
      "AUTOSELECT=YES,CHANNELS=\"5\"\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=360000,AVERAGE-BANDWIDTH=240000,"
      "CODECS=\"sdvideocodec,audiocodec\","
      "RESOLUTION=800x600,AUDIO=\"audiogroup\"\n"
      "http://playlists.org/sd.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=760000,AVERAGE-BANDWIDTH=440000,"
      "CODECS=\"hdvideocodec,audiocodec\","
      "RESOLUTION=800x600,AUDIO=\"audiogroup\"\n"
      "http://playlists.org/hd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistMultipleAudioGroups) {
  const uint64_t kVideoMaxBitrate = 300000;
  const uint64_t kVideoAvgBitrate = 200000;

  const uint64_t kAudio1MaxBitrate = 50000;
  const uint64_t kAudio1AvgBitrate = 40000;
  const uint64_t kAudio2MaxBitrate = 100000;
  const uint64_t kAudio2AvgBitrate = 70000;

  const uint64_t kAudio1Channels = 1;
  const uint64_t kAudio2Channels = 8;

  // First video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video_playlist = CreateVideoPlaylist(
      "video.m3u8", "videocodec", kVideoMaxBitrate, kVideoAvgBitrate);

  // First audio, eng_lo.m3u8.
  std::unique_ptr<MockMediaPlaylist> eng_lo_playlist = CreateAudioPlaylist(
      "eng_lo.m3u8", "english_lo", "audio_lo", "audiocodec_lo", "en",
      kAudio1Channels, kAudio1MaxBitrate, kAudio1AvgBitrate);

  // Second audio, eng_hi.m3u8.
  std::unique_ptr<MockMediaPlaylist> eng_hi_playlist = CreateAudioPlaylist(
      "eng_hi.m3u8", "english_hi", "audio_hi", "audiocodec_hi", "en",
      kAudio2Channels, kAudio2MaxBitrate, kAudio2AvgBitrate);

  const char kBaseUrl[] = "http://anydomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(
      kBaseUrl, test_output_dir_,
      {video_playlist.get(), eng_lo_playlist.get(), eng_hi_playlist.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_hi.m3u8\","
      "GROUP-ID=\"audio_hi\",LANGUAGE=\"en\",NAME=\"english_hi\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"8\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_lo.m3u8\","
      "GROUP-ID=\"audio_lo\",LANGUAGE=\"en\",NAME=\"english_lo\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"1\"\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=400000,AVERAGE-BANDWIDTH=270000,"
      "CODECS=\"videocodec,audiocodec_hi\","
      "RESOLUTION=800x600,AUDIO=\"audio_hi\"\n"
      "http://anydomain.com/video.m3u8\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=240000,"
      "CODECS=\"videocodec,audiocodec_lo\","
      "RESOLUTION=800x600,AUDIO=\"audio_lo\"\n"
      "http://anydomain.com/video.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistSameAudioGroupSameLanguage) {
  // First video, video.m3u8.
  std::unique_ptr<MockMediaPlaylist> video_playlist =
      CreateVideoPlaylist("video.m3u8", "videocodec", 300000, 200000);

  // First audio, eng_lo.m3u8.
  std::unique_ptr<MockMediaPlaylist> eng_lo_playlist = CreateAudioPlaylist(
      "eng_lo.m3u8", "english", "audio", "audiocodec", "en", 1, 50000, 40000);

  std::unique_ptr<MockMediaPlaylist> eng_hi_playlist = CreateAudioPlaylist(
      "eng_hi.m3u8", "english", "audio", "audiocodec", "en", 8, 100000, 80000);

  const char kBaseUrl[] = "http://anydomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(
      kBaseUrl, test_output_dir_,
      {video_playlist.get(), eng_lo_playlist.get(), eng_hi_playlist.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_lo.m3u8\","
      "GROUP-ID=\"audio\",LANGUAGE=\"en\",NAME=\"english\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"1\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://anydomain.com/eng_hi.m3u8\","
      "GROUP-ID=\"audio\",LANGUAGE=\"en\",NAME=\"english\",CHANNELS=\"8\"\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=400000,AVERAGE-BANDWIDTH=280000,"
      "CODECS=\"videocodec,audiocodec\",RESOLUTION=800x600,AUDIO=\"audio\"\n"
      "http://anydomain.com/video.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideosAndTexts) {
  // Video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video1 =
      CreateVideoPlaylist("sd.m3u8", "sdvideocodec", 300000, 200000);

  // Video, hd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video2 =
      CreateVideoPlaylist("hd.m3u8", "sdvideocodec", 600000, 500000);

  // Text, eng.m3u8.
  std::unique_ptr<MockMediaPlaylist> text_eng =
      CreateTextPlaylist("eng.m3u8", "english", "textgroup", "textcodec", "en");

  // Text, fr.m3u8.
  std::unique_ptr<MockMediaPlaylist> text_fr =
      CreateTextPlaylist("fr.m3u8", "french", "textgroup", "textcodec", "fr");

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(
      kBaseUrl, test_output_dir_,
      {video1.get(), video2.get(), text_eng.get(), text_fr.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"textgroup\",LANGUAGE=\"en\",NAME=\"english\","
      "AUTOSELECT=YES\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/fr.m3u8\","
      "GROUP-ID=\"textgroup\",LANGUAGE=\"fr\",NAME=\"french\",DEFAULT=YES,"
      "AUTOSELECT=YES\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=300000,AVERAGE-BANDWIDTH=200000,"
      "CODECS=\"sdvideocodec,textcodec\",RESOLUTION=800x600,"
      "SUBTITLES=\"textgroup\"\n"
      "http://playlists.org/sd.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=600000,AVERAGE-BANDWIDTH=500000,"
      "CODECS=\"sdvideocodec,textcodec\",RESOLUTION=800x600,"
      "SUBTITLES=\"textgroup\"\n"
      "http://playlists.org/hd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideoAndTextWithCharacteritics) {
  // Video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video =
      CreateVideoPlaylist("sd.m3u8", "sdvideocodec", 300000, 200000);

  // Text, eng.m3u8.
  std::unique_ptr<MockMediaPlaylist> text =
      CreateTextPlaylist("eng.m3u8", "english", "textgroup", "textcodec", "en");
  text->SetCharacteristicsForTesting(std::vector<std::string>{
      "public.accessibility.transcribes-spoken-dialog", "public.easy-to-read"});

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_,
                                                   {video.get(), text.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"textgroup\",LANGUAGE=\"en\",NAME=\"english\",AUTOSELECT=YES,"
      "CHARACTERISTICS=\""
      "public.accessibility.transcribes-spoken-dialog,public.easy-to-read\"\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=300000,AVERAGE-BANDWIDTH=200000,"
      "CODECS=\"sdvideocodec,textcodec\",RESOLUTION=800x600,"
      "SUBTITLES=\"textgroup\"\n"
      "http://playlists.org/sd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideoAndTextGroups) {
  // Video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video =
      CreateVideoPlaylist("sd.m3u8", "sdvideocodec", 300000, 200000);

  // Text, eng.m3u8.
  std::unique_ptr<MockMediaPlaylist> text_eng = CreateTextPlaylist(
      "eng.m3u8", "english", "en-text-group", "textcodec", "en");

  // Text, fr.m3u8.
  std::unique_ptr<MockMediaPlaylist> text_fr = CreateTextPlaylist(
      "fr.m3u8", "french", "fr-text-group", "textcodec", "fr");

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(
      kBaseUrl, test_output_dir_,
      {video.get(), text_eng.get(), text_fr.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"en-text-group\",LANGUAGE=\"en\",NAME=\"english\","
      "AUTOSELECT=YES\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/fr.m3u8\","
      "GROUP-ID=\"fr-text-group\",LANGUAGE=\"fr\",NAME=\"french\","
      "DEFAULT=YES,AUTOSELECT=YES\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=300000,AVERAGE-BANDWIDTH=200000,"
      "CODECS=\"sdvideocodec,textcodec\",RESOLUTION=800x600,"
      "SUBTITLES=\"en-text-group\"\n"
      "http://playlists.org/sd.m3u8\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=300000,AVERAGE-BANDWIDTH=200000,"
      "CODECS=\"sdvideocodec,textcodec\",RESOLUTION=800x600,"
      "SUBTITLES=\"fr-text-group\"\n"
      "http://playlists.org/sd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideoAndAudioAndText) {
  // Video, sd.m3u8.
  std::unique_ptr<MockMediaPlaylist> video =
      CreateVideoPlaylist("sd.m3u8", "sdvideocodec", 300000, 200000);

  // Audio, english.m3u8.
  std::unique_ptr<MockMediaPlaylist> audio = CreateAudioPlaylist(
      "eng.m3u8", "english", "audiogroup", "audiocodec", "en", 2, 50000, 30000);

  // Text, english.m3u8.
  std::unique_ptr<MockMediaPlaylist> text =
      CreateTextPlaylist("eng.m3u8", "english", "textgroup", "textcodec", "en");

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(
      kBaseUrl, test_output_dir_, {video.get(), audio.get(), text.get()}));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"audiogroup\",LANGUAGE=\"en\",NAME=\"english\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"2\"\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/eng.m3u8\","
      "GROUP-ID=\"textgroup\",LANGUAGE=\"en\",NAME=\"english\","
      "AUTOSELECT=YES\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=230000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audiogroup\",SUBTITLES=\"textgroup\"\n"
      "http://playlists.org/sd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistMixedPlaylistsDifferentGroups) {
  const uint64_t kAudioChannels = 2;
  const uint64_t kAudioMaxBitrate = 50000;
  const uint64_t kAudioAvgBitrate = 30000;
  const uint64_t kVideoMaxBitrate = 300000;
  const uint64_t kVideoAvgBitrate = 100000;
  const uint64_t kIframeMaxBitrate = 100000;
  const uint64_t kIframeAvgBitrate = 80000;

  std::unique_ptr<MockMediaPlaylist> media_playlists[] = {
      // AUDIO
      CreateAudioPlaylist("audio-1.m3u8", "audio 1", "audio-group-1",
                          "audiocodec", "en", kAudioChannels, kAudioMaxBitrate,
                          kAudioAvgBitrate),
      CreateAudioPlaylist("audio-2.m3u8", "audio 2", "audio-group-2",
                          "audiocodec", "fr", kAudioChannels, kAudioMaxBitrate,
                          kAudioAvgBitrate),

      // SUBTITLES
      CreateTextPlaylist("text-1.m3u8", "text 1", "text-group-1", "textcodec",
                         "en"),
      CreateTextPlaylist("text-2.m3u8", "text 2", "text-group-2", "textcodec",
                         "fr"),

      // VIDEO
      CreateVideoPlaylist("video-1.m3u8", "sdvideocodec", kVideoMaxBitrate,
                          kVideoAvgBitrate),
      CreateVideoPlaylist("video-2.m3u8", "sdvideocodec", kVideoMaxBitrate,
                          kVideoAvgBitrate),

      // I-Frame
      CreateIframePlaylist("iframe-1.m3u8", "sdvideocodec", kIframeMaxBitrate,
                           kIframeAvgBitrate),
      CreateIframePlaylist("iframe-2.m3u8", "sdvideocodec", kIframeMaxBitrate,
                           kIframeAvgBitrate),
  };

  // Add all the media playlists to the master playlist.
  std::list<MediaPlaylist*> media_playlist_list;
  for (const auto& media_playlist : media_playlists) {
    media_playlist_list.push_back(media_playlist.get());
  }

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_,
                                                   media_playlist_list));

  std::string actual;
  ASSERT_TRUE(File::ReadFileToString(master_playlist_path_.c_str(), &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/audio-1.m3u8\","
      "GROUP-ID=\"audio-group-1\",LANGUAGE=\"en\",NAME=\"audio 1\","
      "DEFAULT=YES,AUTOSELECT=YES,CHANNELS=\"2\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,URI=\"http://playlists.org/audio-2.m3u8\","
      "GROUP-ID=\"audio-group-2\",LANGUAGE=\"fr\",NAME=\"audio 2\","
      "AUTOSELECT=YES,CHANNELS=\"2\"\n"
      "\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/text-1.m3u8\","
      "GROUP-ID=\"text-group-1\",LANGUAGE=\"en\",NAME=\"text 1\","
      "AUTOSELECT=YES\n"
      "#EXT-X-MEDIA:TYPE=SUBTITLES,URI=\"http://playlists.org/text-2.m3u8\","
      "GROUP-ID=\"text-group-2\",LANGUAGE=\"fr\",NAME=\"text 2\","
      "DEFAULT=YES,AUTOSELECT=YES\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-1\",SUBTITLES=\"text-group-1\"\n"
      "http://playlists.org/video-1.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-1\",SUBTITLES=\"text-group-1\"\n"
      "http://playlists.org/video-2.m3u8\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-1\",SUBTITLES=\"text-group-2\"\n"
      "http://playlists.org/video-1.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-1\",SUBTITLES=\"text-group-2\"\n"
      "http://playlists.org/video-2.m3u8\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-2\",SUBTITLES=\"text-group-1\"\n"
      "http://playlists.org/video-1.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-2\",SUBTITLES=\"text-group-1\"\n"
      "http://playlists.org/video-2.m3u8\n"
      "\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-2\",SUBTITLES=\"text-group-2\"\n"
      "http://playlists.org/video-1.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,AVERAGE-BANDWIDTH=130000,"
      "CODECS=\"sdvideocodec,audiocodec,textcodec\",RESOLUTION=800x600,"
      "AUDIO=\"audio-group-2\",SUBTITLES=\"text-group-2\"\n"
      "http://playlists.org/video-2.m3u8\n"
      "\n"
      "#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=100000,AVERAGE-BANDWIDTH=80000,"
      "CODECS=\"sdvideocodec\",RESOLUTION=800x600,"
      "URI=\"http://playlists.org/iframe-1.m3u8\"\n"
      "#EXT-X-I-FRAME-STREAM-INF:BANDWIDTH=100000,AVERAGE-BANDWIDTH=80000,"
      "CODECS=\"sdvideocodec\",RESOLUTION=800x600,"
      "URI=\"http://playlists.org/iframe-2.m3u8\"\n";

  ASSERT_EQ(expected, actual);
}
}  // namespace hls
}  // namespace shaka
