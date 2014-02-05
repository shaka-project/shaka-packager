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

TEST(MpdBuilderTest, VOD_Video) {
  MpdBuilder mpd(MpdBuilder::kStatic);
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);

  AdaptationSet* video_adaptation_set = mpd.AddAdaptationSet();
  ASSERT_TRUE(video_adaptation_set);

  Representation* video_representation =
      video_adaptation_set->AddRepresentation(video_media_info);
  ASSERT_TRUE(video_adaptation_set);

  std::string mpd_doc;
  ASSERT_TRUE(mpd.ToString(&mpd_doc));
  ASSERT_TRUE(ValidateMpdSchema(mpd_doc));

  EXPECT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      mpd_doc, kFileNameExpectedMpdOutputVideo1));
}

TEST(MpdBuilderTest, VOD_VideoAndAudio) {
  MpdBuilder mpd(MpdBuilder::kStatic);
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  MediaInfo audio_media_info = GetTestMediaInfo(kFileNameAudioMediaInfo1);

  // The order matters here to check against expected output.
  // TODO(rkuroiwa): Investigate if I can deal with IDs and order elements
  // deterministically.
  AdaptationSet* video_adaptation_set = mpd.AddAdaptationSet();
  ASSERT_TRUE(video_adaptation_set);

  AdaptationSet* audio_adaptation_set = mpd.AddAdaptationSet();
  ASSERT_TRUE(audio_adaptation_set);

  Representation* audio_representation =
      audio_adaptation_set->AddRepresentation(audio_media_info);
  ASSERT_TRUE(audio_representation);

  Representation* video_representation =
      video_adaptation_set->AddRepresentation(video_media_info);
  ASSERT_TRUE(video_adaptation_set);

  std::string mpd_doc;
  ASSERT_TRUE(mpd.ToString(&mpd_doc));
  ASSERT_TRUE(ValidateMpdSchema(mpd_doc));

  EXPECT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      mpd_doc, kFileNameExpectedMpdOutputAudio1AndVideo1));
}

}  // namespace dash_packager
