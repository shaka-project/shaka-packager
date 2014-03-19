// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/file_util.h"
#include "base/logging.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/test/mpd_builder_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dash_packager {

class StaticMpdBuilderTest : public ::testing::Test {
 public:
  StaticMpdBuilderTest() : mpd_(MpdBuilder::kStatic) {}
  virtual ~StaticMpdBuilderTest() {}

  void CheckMpd(const std::string& expected_output_file) {
    std::string mpd_doc;
    ASSERT_TRUE(mpd_.ToString(&mpd_doc));
    ASSERT_TRUE(ValidateMpdSchema(mpd_doc));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMpdToEqualExpectedOutputFile(mpd_doc, expected_output_file));
  }

 protected:
  MpdBuilder mpd_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StaticMpdBuilderTest);
};

TEST_F(StaticMpdBuilderTest, Video) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);

  AdaptationSet* video_adaptation_set = mpd_.AddAdaptationSet();
  ASSERT_TRUE(video_adaptation_set);

  Representation* video_representation =
      video_adaptation_set->AddRepresentation(video_media_info);
  ASSERT_TRUE(video_adaptation_set);

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputVideo1));
}

TEST_F(StaticMpdBuilderTest, VideoAndAudio) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  MediaInfo audio_media_info = GetTestMediaInfo(kFileNameAudioMediaInfo1);

  // The order matters here to check against expected output.
  // TODO: Investigate if I can deal with IDs and order elements
  // deterministically.
  AdaptationSet* video_adaptation_set = mpd_.AddAdaptationSet();
  ASSERT_TRUE(video_adaptation_set);

  AdaptationSet* audio_adaptation_set = mpd_.AddAdaptationSet();
  ASSERT_TRUE(audio_adaptation_set);

  Representation* audio_representation =
      audio_adaptation_set->AddRepresentation(audio_media_info);
  ASSERT_TRUE(audio_representation);

  Representation* video_representation =
      video_adaptation_set->AddRepresentation(video_media_info);
  ASSERT_TRUE(video_adaptation_set);

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputAudio1AndVideo1));
}

// MPD schema has strict ordering. AudioChannelConfiguration must appear before
// ContentProtection.
TEST_F(StaticMpdBuilderTest, AudioChannelConfigurationWithContentProtection) {
  MediaInfo encrypted_audio_media_info =
      GetTestMediaInfo(kFileNameEncytpedAudioMediaInfo);

  AdaptationSet* audio_adaptation_set = mpd_.AddAdaptationSet();
  ASSERT_TRUE(audio_adaptation_set);

  Representation* audio_representation =
      audio_adaptation_set->AddRepresentation(encrypted_audio_media_info);
  ASSERT_TRUE(audio_representation);

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputEncryptedAudio));
}

}  // namespace dash_packager
