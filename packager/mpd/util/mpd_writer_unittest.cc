// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/file_util.h"
#include "packager/base/path_service.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/mpd/util/mpd_writer.h"

namespace edash_packager {

// Note that these tests look very similar to MpdBuilder tests but these can
// only handle MediaInfos with 1 stream in each file.
TEST(MpdWriterTest, VideoMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file = GetTestDataFilePath(kFileNameVideoMediaInfo1);

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file.value().c_str(), ""));
  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputVideo1));
}

TEST(MpdWriterTest, TwoVideoMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file1 =
      GetTestDataFilePath(kFileNameVideoMediaInfo1);
  base::FilePath media_info_file2 =
      GetTestDataFilePath(kFileNameVideoMediaInfo2);

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file1.value().c_str(), ""));
  ASSERT_TRUE(mpd_writer.AddFile(media_info_file2.value().c_str(), ""));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputVideo1And2));
}

TEST(MpdWriterTest, AudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file = GetTestDataFilePath(kFileNameAudioMediaInfo1);

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file.value().c_str(), ""));
  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputAudio1));
}

TEST(MpdWriterTest, VideoAudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath audio_media_info =
      GetTestDataFilePath(kFileNameAudioMediaInfo1);
  base::FilePath video_media_info =
      GetTestDataFilePath(kFileNameVideoMediaInfo1);

  ASSERT_TRUE(mpd_writer.AddFile(audio_media_info.value().c_str(), ""));
  ASSERT_TRUE(mpd_writer.AddFile(video_media_info.value().c_str(), ""));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputAudio1AndVideo1));
}

TEST(MpdWriterTest, EncryptedAudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath encrypted_audio_media_info =
      GetTestDataFilePath(kFileNameEncytpedAudioMediaInfo);

  ASSERT_TRUE(mpd_writer.AddFile(encrypted_audio_media_info.value().c_str(),
                                 ""));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd, kFileNameExpectedMpdOutputEncryptedAudio));
}

}  // namespace edash_packager
