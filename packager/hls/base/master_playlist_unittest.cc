// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_util.h"
#include "packager/base/files/scoped_temp_dir.h"
#include "packager/hls/base/master_playlist.h"
#include "packager/hls/base/media_playlist.h"
#include "packager/hls/base/mock_media_playlist.h"
#include "packager/media/file/file.h"
#include "packager/version/version.h"

namespace shaka {
namespace hls {

using ::testing::AtLeast;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::_;
using base::FilePath;

namespace {
const char kDefaultMasterPlaylistName[] = "playlist.m3u8";
const uint32_t kWidth = 800;
const uint32_t kHeight = 600;
const MediaPlaylist::MediaPlaylistType kVodPlaylist =
    MediaPlaylist::MediaPlaylistType::kVod;
}  // namespace

class MasterPlaylistTest : public ::testing::Test {
 protected:
  MasterPlaylistTest() : master_playlist_(kDefaultMasterPlaylistName) {}

  void SetUp() override {
    SetPackagerVersionForTesting("test");
    GetOutputDir(&test_output_dir_path_, &test_output_dir_);
  }

  MasterPlaylist master_playlist_;
  FilePath test_output_dir_path_;
  std::string test_output_dir_;

 private:
  // Creates a path to the output directory for writing out playlists.
  // |temp_dir_path| is set to the temporary directory so that it can be opened
  // using base::File* related API.
  // |output_dir| is set to an equivalent value to |temp_dir_path| but formatted
  // so that media::File interface can Open it.
  void GetOutputDir(FilePath* temp_dir_path, std::string* output_dir) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(temp_dir_.IsValid());
    *temp_dir_path = temp_dir_.path();
    // TODO(rkuroiwa): Use memory file sys once prefix is exposed.
    *output_dir = media::kLocalFilePrefix + temp_dir_.path().AsUTF8Unsafe()
      + "/";
  }

  base::ScopedTempDir temp_dir_;
};

TEST_F(MasterPlaylistTest, AddMediaPlaylist) {
  MockMediaPlaylist mock_playlist(kVodPlaylist, "playlist1.m3u8", "somename",
                                  "somegroupid");
  master_playlist_.AddMediaPlaylist(&mock_playlist);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistOneVideo) {
  std::string codec = "avc1";
  MockMediaPlaylist mock_playlist(kVodPlaylist, "media1.m3u8", "somename",
                                  "somegroupid");
  mock_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo);
  mock_playlist.SetCodecForTesting(codec);
  EXPECT_CALL(mock_playlist, Bitrate()).WillOnce(Return(435889));
  EXPECT_CALL(mock_playlist, GetResolution(NotNull(), NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(kWidth),
                      SetArgPointee<1>(kHeight),
                      Return(true)));
  master_playlist_.AddMediaPlaylist(&mock_playlist);

  const char kBaseUrl[] = "http://myplaylistdomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  FilePath master_playlist_path =
    test_output_dir_path_.Append(FilePath::FromUTF8Unsafe(
        kDefaultMasterPlaylistName));
  ASSERT_TRUE(base::PathExists(master_playlist_path))
      << "Cannot find " << master_playlist_path.value();

  std::string actual;
  ASSERT_TRUE(base::ReadFileToString(master_playlist_path, &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=435889,CODECS=\"avc1\",RESOLUTION=800x600\n"
      "http://myplaylistdomain.com/media1.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistVideoAndAudio) {
  // First video, sd.m3u8.
  std::string sd_video_codec = "sdvideocodec";
  MockMediaPlaylist sd_video_playlist(kVodPlaylist, "sd.m3u8", "somename",
                                      "somegroupid");
  sd_video_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo);
  sd_video_playlist.SetCodecForTesting(sd_video_codec);
  EXPECT_CALL(sd_video_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(300000));
  EXPECT_CALL(sd_video_playlist, GetResolution(NotNull(), NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<0>(kWidth),
                            SetArgPointee<1>(kHeight),
                            Return(true)));
  master_playlist_.AddMediaPlaylist(&sd_video_playlist);

  // Second video, hd.m3u8.
  std::string hd_video_codec = "hdvideocodec";
  MockMediaPlaylist hd_video_playlist(kVodPlaylist, "hd.m3u8", "somename",
                                      "somegroupid");
  hd_video_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo);
  hd_video_playlist.SetCodecForTesting(hd_video_codec);
  EXPECT_CALL(hd_video_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(700000));
  EXPECT_CALL(hd_video_playlist, GetResolution(NotNull(), NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<0>(kWidth),
                            SetArgPointee<1>(kHeight),
                            Return(true)));
  master_playlist_.AddMediaPlaylist(&hd_video_playlist);

  // First audio, english.m3u8.
  // Note that audiocodecs should match for different audio tracks with same
  // group ID.
  std::string audio_codec = "audiocodec";
  MockMediaPlaylist english_playlist(kVodPlaylist, "eng.m3u8", "english",
                                     "audiogroup");
  EXPECT_CALL(english_playlist, GetLanguage()).WillRepeatedly(Return("en"));
  english_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListAudio);
  english_playlist.SetCodecForTesting(audio_codec);
  EXPECT_CALL(english_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(50000));
  EXPECT_CALL(english_playlist, GetResolution(NotNull(), NotNull()))
      .Times(0);
  master_playlist_.AddMediaPlaylist(&english_playlist);

  // Second audio, spanish.m3u8.
  MockMediaPlaylist spanish_playlist(kVodPlaylist, "spa.m3u8", "espanol",
                                     "audiogroup");
  EXPECT_CALL(spanish_playlist, GetLanguage()).WillRepeatedly(Return("es"));
  spanish_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListAudio);
  spanish_playlist.SetCodecForTesting(audio_codec);
  EXPECT_CALL(spanish_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(60000));
  EXPECT_CALL(spanish_playlist, GetResolution(NotNull(), NotNull()))
      .Times(0);
  master_playlist_.AddMediaPlaylist(&spanish_playlist);

  const char kBaseUrl[] = "http://playlists.org/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  FilePath master_playlist_path =
    test_output_dir_path_.Append(FilePath::FromUTF8Unsafe(
        kDefaultMasterPlaylistName));
  ASSERT_TRUE(base::PathExists(master_playlist_path))
      << "Cannot find " << master_playlist_path.value();

  std::string actual;
  ASSERT_TRUE(base::ReadFileToString(master_playlist_path, &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audiogroup\",NAME=\"english\","
      "LANGUAGE=\"en\",URI=\"http://playlists.org/eng.m3u8\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audiogroup\",NAME=\"espanol\","
      "LANGUAGE=\"es\",URI=\"http://playlists.org/spa.m3u8\"\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=360000,CODECS=\"sdvideocodec,audiocodec\""
      ",RESOLUTION=800x600,AUDIO=\"audiogroup\"\n"
      "http://playlists.org/sd.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=760000,CODECS=\"hdvideocodec,audiocodec\""
      ",RESOLUTION=800x600,AUDIO=\"audiogroup\"\n"
      "http://playlists.org/hd.m3u8\n";

  ASSERT_EQ(expected, actual);
}

TEST_F(MasterPlaylistTest, WriteMasterPlaylistMultipleAudioGroups) {
  // First video, sd.m3u8.
  std::string video_codec = "videocodec";
  MockMediaPlaylist video_playlist(kVodPlaylist, "video.m3u8", "somename",
                                   "somegroupid");
  video_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo);
  video_playlist.SetCodecForTesting(video_codec);
  EXPECT_CALL(video_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(300000));
  EXPECT_CALL(video_playlist, GetResolution(NotNull(), NotNull()))
      .WillRepeatedly(DoAll(SetArgPointee<0>(kWidth),
                            SetArgPointee<1>(kHeight),
                            Return(true)));
  master_playlist_.AddMediaPlaylist(&video_playlist);

  // First audio, eng_lo.m3u8.
  std::string audio_codec_lo = "audiocodec_lo";
  MockMediaPlaylist eng_lo_playlist(kVodPlaylist, "eng_lo.m3u8", "english_lo",
                                    "audio_lo");
  EXPECT_CALL(eng_lo_playlist, GetLanguage()).WillRepeatedly(Return("en"));
  eng_lo_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListAudio);
  eng_lo_playlist.SetCodecForTesting(audio_codec_lo);
  EXPECT_CALL(eng_lo_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(50000));
  EXPECT_CALL(eng_lo_playlist, GetResolution(NotNull(), NotNull()))
      .Times(0);
  master_playlist_.AddMediaPlaylist(&eng_lo_playlist);

  std::string audio_codec_hi = "audiocodec_hi";
  MockMediaPlaylist eng_hi_playlist(kVodPlaylist, "eng_hi.m3u8", "english_hi",
                                    "audio_hi");
  EXPECT_CALL(eng_hi_playlist, GetLanguage()).WillRepeatedly(Return("en"));
  eng_hi_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListAudio);
  eng_hi_playlist.SetCodecForTesting(audio_codec_hi);
  EXPECT_CALL(eng_hi_playlist, Bitrate())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(100000));
  EXPECT_CALL(eng_hi_playlist, GetResolution(NotNull(), NotNull()))
      .Times(0);
  master_playlist_.AddMediaPlaylist(&eng_hi_playlist);

  const char kBaseUrl[] = "http://anydomain.com/";
  EXPECT_TRUE(master_playlist_.WriteMasterPlaylist(kBaseUrl, test_output_dir_));

  FilePath master_playlist_path = test_output_dir_path_.Append(
      FilePath::FromUTF8Unsafe(kDefaultMasterPlaylistName));
  ASSERT_TRUE(base::PathExists(master_playlist_path))
      << "Cannot find " << master_playlist_path.value();

  std::string actual;
  ASSERT_TRUE(base::ReadFileToString(master_playlist_path, &actual));

  const std::string expected =
      "#EXTM3U\n"
      "## Generated with https://github.com/google/shaka-packager version "
      "test\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_hi\",NAME=\"english_hi\","
      "LANGUAGE=\"en\",URI=\"http://anydomain.com/eng_hi.m3u8\"\n"
      "#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID=\"audio_lo\",NAME=\"english_lo\","
      "LANGUAGE=\"en\",URI=\"http://anydomain.com/eng_lo.m3u8\"\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=400000,CODECS=\"videocodec,audiocodec_hi\""
      ",RESOLUTION=800x600,AUDIO=\"audio_hi\"\n"
      "http://anydomain.com/video.m3u8\n"
      "#EXT-X-STREAM-INF:BANDWIDTH=350000,CODECS=\"videocodec,audiocodec_lo\""
      ",RESOLUTION=800x600,AUDIO=\"audio_lo\"\n"
      "http://anydomain.com/video.m3u8\n";

  ASSERT_EQ(expected, actual);
}

MATCHER_P(FileNameMatches, expected_file_name, "") {
  const std::string& actual_filename = arg->file_name();
  *result_listener << "which is " << actual_filename;
  return expected_file_name == actual_filename;
}

// This test basically is WriteMasterPlaylist() and also make sure that
// the target duration is set for MediaPlaylist and
// MediaPlaylist::WriteToFile() is called.
TEST_F(MasterPlaylistTest, WriteAllPlaylists) {
  std::string codec = "avc1";
  MockMediaPlaylist mock_playlist(kVodPlaylist, "media1.m3u8", "somename",
                                  "somegroupid");
  mock_playlist.SetStreamTypeForTesting(
      MediaPlaylist::MediaPlaylistStreamType::kPlayListVideo);
  mock_playlist.SetCodecForTesting(codec);
  ON_CALL(mock_playlist, Bitrate()).WillByDefault(Return(435889));
  ON_CALL(mock_playlist, GetResolution(NotNull(), NotNull())).WillByDefault(
      DoAll(SetArgPointee<0>(kWidth),
            SetArgPointee<1>(kHeight),
            Return(true)));

  EXPECT_CALL(mock_playlist, GetLongestSegmentDuration()).WillOnce(Return(10));
  EXPECT_CALL(mock_playlist, SetTargetDuration(10)).WillOnce(Return(true));
  master_playlist_.AddMediaPlaylist(&mock_playlist);

  EXPECT_CALL(
      mock_playlist,
      WriteToFile(FileNameMatches(
          test_output_dir_path_.Append(FilePath::FromUTF8Unsafe("media1.m3u8"))
              .AsUTF8Unsafe())))
      .WillOnce(Return(true));

  const char kBaseUrl[] = "http://domain.com/";
  EXPECT_TRUE(master_playlist_.WriteAllPlaylists(kBaseUrl, test_output_dir_));
  FilePath master_playlist_path = test_output_dir_path_.Append(
      FilePath::FromUTF8Unsafe(kDefaultMasterPlaylistName));
  ASSERT_TRUE(base::PathExists(master_playlist_path))
      << "Cannot find master playlist at " << master_playlist_path.value();
}
}  // namespace hls
}  // namespace shaka
