// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/file_util.h"
#include "base/path_service.h"
#include "mpd/util/mpd_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(rkuroiwa): Move schema check function in mpd_builder_unittest.cc to
// another file so that this file can use it as well.
namespace dash_packager {

namespace {
base::FilePath GetTestDataFilePath(const std::string& file_name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("mpd"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .AppendASCII(file_name);
  return file_path;
}

void ExpectToEqualExpectedOutputFile(
    const std::string& mpd_string,
    const base::FilePath& expected_output_file) {
  std::string expected_mpd;
  ASSERT_TRUE(file_util::ReadFileToString(expected_output_file, &expected_mpd))
      << "Failed to read: " << expected_output_file.value();
  ASSERT_EQ(expected_mpd, mpd_string);
}
}  // namespace

TEST(MpdWriterTest, ReadMediaInfoFile_VideoMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file = GetTestDataFilePath("video_media_info1.txt");

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file.value().c_str()));
  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectToEqualExpectedOutputFile(
      generated_mpd,
      GetTestDataFilePath("video_media_info1_expected_mpd_output.txt")));
}

TEST(MpdWriterTest, ReadMediaInfoFile_TwoVideoMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file1 =
      GetTestDataFilePath("video_media_info1.txt");
  base::FilePath media_info_file2 =
      GetTestDataFilePath("video_media_info2.txt");

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file1.value().c_str()));
  ASSERT_TRUE(mpd_writer.AddFile(media_info_file2.value().c_str()));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectToEqualExpectedOutputFile(
      generated_mpd,
      GetTestDataFilePath("video_media_info1and2_expected_mpd_output.txt")));
}

TEST(MpdWriterTest, ReadMediaInfoFile_AudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath media_info_file = GetTestDataFilePath("audio_media_info1.txt");

  ASSERT_TRUE(mpd_writer.AddFile(media_info_file.value().c_str()));
  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectToEqualExpectedOutputFile(
      generated_mpd,
      GetTestDataFilePath("audio_media_info1_expected_mpd_output.txt")));
}

TEST(MpdWriterTest, ReadMediaInfoFile_VideoAudioMediaInfo) {
  MpdWriter mpd_writer;
  base::FilePath audio_media_info =
      GetTestDataFilePath("audio_media_info1.txt");
  base::FilePath video_media_info =
      GetTestDataFilePath("video_media_info1.txt");

  ASSERT_TRUE(mpd_writer.AddFile(audio_media_info.value().c_str()));
  ASSERT_TRUE(mpd_writer.AddFile(video_media_info.value().c_str()));

  std::string generated_mpd;
  ASSERT_TRUE(mpd_writer.WriteMpdToString(&generated_mpd));

  ASSERT_NO_FATAL_FAILURE(ExpectToEqualExpectedOutputFile(
      generated_mpd,
      GetTestDataFilePath(
          "audio_media_info1_video_media_info1_expected_mpd_output.txt")));
}

}  // namespace dash_packager
