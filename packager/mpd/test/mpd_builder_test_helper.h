// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_TEST_MPD_BUILDER_TEST_HELPER_H_
#define MPD_TEST_MPD_BUILDER_TEST_HELPER_H_

#include <string>

#include "packager/base/files/file_path.h"
#include "packager/mpd/base/media_info.pb.h"

namespace shaka {

class MediaInfo;

// File names that could be used to call GetTestDataFilePath().
// TODO(rkuroiwa): Seems like too may indirection. Maybe put the definition
// of the proto instance in this file. Or just remove this and put it in the
// test.
const char kFileNameVideoMediaInfo1[] = "video_media_info1.txt";
const char kFileNameVideoMediaInfo2[] = "video_media_info2.txt";
const char kFileNameAudioMediaInfo1[] = "audio_media_info1.txt";

// These are the expected output files.
const char kFileNameExpectedMpdOutputVideo1[] =
    "video_media_info1_expected_mpd_output.txt";

const char kFileNameExpectedMpdOutputVideo1And2[] =
    "video_media_info1and2_expected_mpd_output.txt";

const char kFileNameExpectedMpdOutputAudio1[] =
    "audio_media_info1_expected_mpd_output.txt";

const char kFileNameExpectedMpdOutputAudio1AndVideo1[] =
    "audio_media_info1_video_media_info1_expected_mpd_output.txt";

// Returns the path to test data with |file_name|. Use constants above to get
// path to the test files.
base::FilePath GetTestDataFilePath(const std::string& file_name);

// Get path to DASH MPD schema.
base::FilePath GetSchemaPath();

// Get the content of |file_path|. Returns empty string on error.
std::string GetPathContent(const base::FilePath& file_path);

// Convert |media_info_string| to MediaInfo.
MediaInfo ConvertToMediaInfo(const std::string& media_info_string);

// This is equivalent to
// return input_file >>= GetTestDataFilePath >>= GetPathContent >>= GetMediaInfo
// i.e. calling the functions with |media_info_file_name|.
MediaInfo GetTestMediaInfo(const std::string& media_info_file_name);

// Return true if |mpd| is a valid MPD.
bool ValidateMpdSchema(const std::string& mpd);

// |expected_output_file| should be the file name that has expected output in
// test data dir.
// This checks if |mpd_string| and the content of |expected_output_file| are
// equal.
void ExpectMpdToEqualExpectedOutputFile(
    const std::string& mpd_string,
    const std::string& expected_output_file);

}  // namespace shaka

#endif  // MPD_TEST_MPD_BUILDER_TEST_HELPER_H_
