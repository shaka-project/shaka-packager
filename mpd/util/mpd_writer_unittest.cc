// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/file_util.h"
#include "base/path_service.h"
#include "mpd/test/mpd_builder_test_helper.h"
#include "mpd/util/mpd_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dash_packager {

TEST(MpdWriterTest, ReadMediaInfoFile_VideoMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file = GetTestDataFilePath(kFileNameVideoMediaInfo1);

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file.value().c_str()));
  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputVideo1));
}

TEST(MpdWriterTest, ReadMediaInfoFile_TwoVideoMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file1 =
      GetTestDataFilePath(kFileNameVideoMediaInfo1);
  base::FilePath media_info_file2 =
      GetTestDataFilePath(kFileNameVideoMediaInfo2);

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file1.value().c_str()));
  ASSERT_TRUE(mpd_writer.AddFile(media_info_file2.value().c_str()));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputVideo1And2));
}

TEST(MpdWriterTest, ReadMediaInfoFile_AudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file = GetTestDataFilePath(kFileNameAudioMediaInfo1);

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file.value().c_str()));
  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputAudio1));
}

TEST(MpdWriterTest, ReadMediaInfoFile_VideoAudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath audio_media_info =
      GetTestDataFilePath(kFileNameAudioMediaInfo1);
  base::FilePath video_media_info =
      GetTestDataFilePath(kFileNameVideoMediaInfo1);

  ASSERT_TRUE(mpd_writer.AddFile(audio_media_info.value().c_str()));
  ASSERT_TRUE(mpd_writer.AddFile(video_media_info.value().c_str()));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));
  ASSERT_TRUE(ValidateMpdSchema(generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectMpdToEqualExpectedOutputFile(
      generated_mpd,
      kFileNameExpectedMpdOutputAudio1AndVideo1));
}

}  // namespace dash_packager
