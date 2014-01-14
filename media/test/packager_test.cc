// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/demuxer.h"
#include "media/base/fixed_encryptor_source.h"
#include "media/base/media_stream.h"
#include "media/base/muxer.h"
#include "media/base/status_test_util.h"
#include "media/base/stream_info.h"
#include "media/mp4/mp4_muxer.h"
#include "media/test/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ValuesIn;

namespace {
const char* kMediaFiles[] = {"bear-1280x720.mp4", "bear-1280x720-av_frag.mp4"};

// Muxer options.
const double kSegmentDurationInSeconds = 1.0;
const double kFragmentDurationInSecodns = 0.1;
const bool kSegmentSapAligned = true;
const bool kFragmentSapAligned = true;
const int kNumSubsegmentsPerSidx = 2;
const char kOutputFileName[] = "output_file";
const char kOutputFileName2[] = "output_file2";
const char kSegmentTemplate[] = "template$Number$.m4s";
const char kSegmentTemplateOutputFile[] = "template1.m4s";
const char kTempFileName[] = "temp_file";

// Encryption constants.
const char kKeyIdHex[] = "e5007e6e9dcd5ac095202ed3758382cd";
const char kKeyHex[] = "6fc96fe628a265b13aeddec0bc421f4d";
const char kPsshHex[] =
    "08011210e5007e6e9dcd5ac095202ed3"
    "758382cd1a0d7769646576696e655f746573742211544553545f"
    "434f4e54454e545f49445f312a025344";
const double kClearLeadInSeconds = 1.5;

}  // namespace

namespace media {

class PackagerTest : public ::testing::TestWithParam<const char*> {
 public:
  virtual void SetUp() OVERRIDE {
    // Create a test directory for testing, will be deleted after test.
    ASSERT_TRUE(
        file_util::CreateNewTempDirectory("packager_", &test_directory_));

    options_.segment_duration = kSegmentDurationInSeconds;
    options_.fragment_duration = kFragmentDurationInSecodns;
    options_.segment_sap_aligned = kSegmentSapAligned;
    options_.fragment_sap_aligned = kFragmentSapAligned;
    options_.num_subsegments_per_sidx = kNumSubsegmentsPerSidx;

    options_.output_file_name =
        test_directory_.AppendASCII(kOutputFileName).value();
    options_.segment_template =
        test_directory_.AppendASCII(kSegmentTemplate).value();
    options_.temp_file_name =
        test_directory_.AppendASCII(kTempFileName).value();
  }

  virtual void TearDown() OVERRIDE { base::DeleteFile(test_directory_, true); }

  void Remux(const std::string& input_file, Muxer* muxer) {
    DCHECK(muxer);

    Demuxer demuxer(input_file, NULL);
    ASSERT_OK(demuxer.Initialize());
    ASSERT_LE(1, demuxer.streams().size());

    VLOG(1) << "Num Streams: " << demuxer.streams().size();
    for (size_t i = 0; i < demuxer.streams().size(); ++i) {
      VLOG(1) << "Streams " << i << ": " << demuxer.streams()[i]->ToString();
    }

    ASSERT_OK(muxer->AddStream(demuxer.streams()[0]));
    ASSERT_OK(muxer->Initialize());

    // Starts remuxing process.
    ASSERT_OK(demuxer.Run());
    ASSERT_OK(muxer->Finalize());
  }

 protected:
  base::FilePath test_directory_;
  MuxerOptions options_;
};

TEST_P(PackagerTest, MP4MuxerSingleSegmentUnencrypted) {
  options_.single_segment = true;

  const std::string input_media_file = GetTestDataFilePath(GetParam()).value();
  scoped_ptr<Muxer> muxer(new mp4::MP4Muxer(options_));
  ASSERT_NO_FATAL_FAILURE(Remux(input_media_file, muxer.get()));

  // Take the muxer output and feed into muxer again. The new muxer output
  // should contain the same contents as the previous muxer output.
  const std::string new_input_media_file = options_.output_file_name;
  options_.output_file_name =
      test_directory_.AppendASCII(kOutputFileName2).value();
  muxer.reset(new mp4::MP4Muxer(options_));
  ASSERT_NO_FATAL_FAILURE(Remux(new_input_media_file, muxer.get()));

  EXPECT_TRUE(base::ContentsEqual(base::FilePath(new_input_media_file),
                                  base::FilePath(options_.output_file_name)));
}

TEST_P(PackagerTest, MP4MuxerSingleSegmentEncrypted) {
  options_.single_segment = true;

  FixedEncryptorSource encryptor_source(kKeyIdHex, kKeyHex, kPsshHex);
  ASSERT_OK(encryptor_source.Initialize());

  const std::string input_media_file = GetTestDataFilePath(GetParam()).value();
  scoped_ptr<Muxer> muxer(new mp4::MP4Muxer(options_));
  muxer->SetEncryptorSource(&encryptor_source, kClearLeadInSeconds);
  ASSERT_NO_FATAL_FAILURE(Remux(input_media_file, muxer.get()));

  // Expect the output to be encrypted.
  Demuxer demuxer(options_.output_file_name, NULL);
  ASSERT_OK(demuxer.Initialize());
  ASSERT_EQ(1, demuxer.streams().size());
  EXPECT_TRUE(demuxer.streams()[0]->info()->is_encrypted());
}

TEST_P(PackagerTest, MP4MuxerMultipleSegmentsUnencrypted) {
  options_.single_segment = false;

  const std::string input_media_file = GetTestDataFilePath(GetParam()).value();
  scoped_ptr<Muxer> muxer(new mp4::MP4Muxer(options_));
  ASSERT_NO_FATAL_FAILURE(Remux(input_media_file, muxer.get()));

  EXPECT_TRUE(base::PathExists(
      test_directory_.AppendASCII(kSegmentTemplateOutputFile)));
}

INSTANTIATE_TEST_CASE_P(PackagerE2ETest, PackagerTest, ValuesIn(kMediaFiles));

}  // namespace media
