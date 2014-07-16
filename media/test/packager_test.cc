// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "media/base/demuxer.h"
#include "media/base/encryption_key_source.h"
#include "media/base/media_stream.h"
#include "media/base/muxer.h"
#include "media/base/status_test_util.h"
#include "media/base/stream_info.h"
#include "media/formats/mp4/mp4_muxer.h"
#include "media/test/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ValuesIn;

namespace media {
namespace {

const char* kMediaFiles[] = {"bear-1280x720.mp4", "bear-1280x720-av_frag.mp4",
                             "bear-1280x720.ts"};

// Muxer options.
const double kSegmentDurationInSeconds = 1.0;
const double kFragmentDurationInSecodns = 0.1;
const bool kSegmentSapAligned = true;
const bool kFragmentSapAligned = true;
const int kNumSubsegmentsPerSidx = 2;

const char kOutputVideo[] = "output_video";
const char kOutputVideo2[] = "output_video_2";
const char kOutputAudio[] = "output_audio";
const char kOutputAudio2[] = "output_audio_2";
const char kOutputNone[] = "";

const char kSegmentTemplate[] = "template$Number$.m4s";
const char kSegmentTemplateOutputPattern[] = "template%d.m4s";

const bool kSingleSegment = true;
const bool kMultipleSegments = false;
const bool kEnableEncryption = true;
const bool kDisableEncryption = false;

// Encryption constants.
const char kKeyIdHex[] = "e5007e6e9dcd5ac095202ed3758382cd";
const char kKeyHex[] = "6fc96fe628a265b13aeddec0bc421f4d";
const char kPsshHex[] =
    "08011210e5007e6e9dcd5ac095202ed3"
    "758382cd1a0d7769646576696e655f746573742211544553545f"
    "434f4e54454e545f49445f312a025344";
const double kClearLeadInSeconds = 1.5;
const double kCryptoDurationInSeconds = 0;  // Key rotation is disabled.

MediaStream* FindFirstStreamOfType(const std::vector<MediaStream*>& streams,
                                   StreamType stream_type) {
  typedef std::vector<MediaStream*>::const_iterator StreamIterator;
  for (StreamIterator it = streams.begin(); it != streams.end(); ++it) {
    if ((*it)->info()->stream_type() == stream_type)
      return *it;
  }
  return NULL;
}
MediaStream* FindFirstVideoStream(const std::vector<MediaStream*>& streams) {
  return FindFirstStreamOfType(streams, kStreamVideo);
}
MediaStream* FindFirstAudioStream(const std::vector<MediaStream*>& streams) {
  return FindFirstStreamOfType(streams, kStreamAudio);
}

}  // namespace

class FakeClock : public base::Clock {
 public:
  // Fake the clock to return NULL time.
  virtual base::Time Now() OVERRIDE { return base::Time(); }
};

class PackagerTestBasic : public ::testing::TestWithParam<const char*> {
 public:
  PackagerTestBasic() : decryptor_source_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Create a test directory for testing, will be deleted after test.
    ASSERT_TRUE(base::CreateNewTempDirectory("packager_", &test_directory_));

    // Copy the input to test directory for easy reference.
    ASSERT_TRUE(base::CopyFile(GetTestDataFilePath(GetParam()),
                               test_directory_.AppendASCII(GetParam())));
  }

  virtual void TearDown() OVERRIDE { base::DeleteFile(test_directory_, true); }

  std::string GetFullPath(const std::string& file_name);
  // Check if |file1| and |file2| are the same.
  bool ContentsEqual(const std::string& file1, const std::string file2);

  MuxerOptions SetupOptions(const std::string& output, bool single_segment);
  void Remux(const std::string& input,
             const std::string& video_output,
             const std::string& audio_output,
             bool single_segment,
             bool enable_encryption);

 protected:
  base::FilePath test_directory_;
  DecryptorSource* decryptor_source_;
  FakeClock fake_clock_;
};

std::string PackagerTestBasic::GetFullPath(const std::string& file_name) {
  return test_directory_.AppendASCII(file_name).value();
}

bool PackagerTestBasic::ContentsEqual(const std::string& file1,
                                      const std::string file2) {
  return base::ContentsEqual(test_directory_.AppendASCII(file1),
                             test_directory_.AppendASCII(file2));
}

MuxerOptions PackagerTestBasic::SetupOptions(const std::string& output,
                                             bool single_segment) {
  MuxerOptions options;
  options.single_segment = single_segment;

  options.segment_duration = kSegmentDurationInSeconds;
  options.fragment_duration = kFragmentDurationInSecodns;
  options.segment_sap_aligned = kSegmentSapAligned;
  options.fragment_sap_aligned = kFragmentSapAligned;
  options.num_subsegments_per_sidx = kNumSubsegmentsPerSidx;

  options.output_file_name = GetFullPath(output);
  options.segment_template = GetFullPath(kSegmentTemplate);
  options.temp_dir = test_directory_.value();
  return options;
}

void PackagerTestBasic::Remux(const std::string& input,
                              const std::string& video_output,
                              const std::string& audio_output,
                              bool single_segment,
                              bool enable_encryption) {
  CHECK(!video_output.empty() || !audio_output.empty());

  Demuxer demuxer(GetFullPath(input), decryptor_source_);
  ASSERT_OK(demuxer.Initialize());

  scoped_ptr<EncryptionKeySource> encryption_key_source(
      EncryptionKeySource::CreateFromHexStrings(
          kKeyIdHex, kKeyHex, kPsshHex, ""));
  DCHECK(encryption_key_source);

  scoped_ptr<Muxer> muxer_video;
  if (!video_output.empty()) {
    muxer_video.reset(
        new mp4::MP4Muxer(SetupOptions(video_output, single_segment)));
    muxer_video->set_clock(&fake_clock_);

    muxer_video->AddStream(FindFirstVideoStream(demuxer.streams()));

    if (enable_encryption) {
      muxer_video->SetEncryptionKeySource(encryption_key_source.get(),
                                          EncryptionKeySource::TRACK_TYPE_SD,
                                          kClearLeadInSeconds,
                                          kCryptoDurationInSeconds);
    }
  }

  scoped_ptr<Muxer> muxer_audio;
  if (!audio_output.empty()) {
    muxer_audio.reset(
        new mp4::MP4Muxer(SetupOptions(audio_output, single_segment)));
    muxer_audio->set_clock(&fake_clock_);

    muxer_audio->AddStream(FindFirstAudioStream(demuxer.streams()));

    if (enable_encryption) {
      muxer_audio->SetEncryptionKeySource(encryption_key_source.get(),
                                          EncryptionKeySource::TRACK_TYPE_SD,
                                          kClearLeadInSeconds,
                                          kCryptoDurationInSeconds);
    }
  }

  // Start remuxing process.
  ASSERT_OK(demuxer.Run());
}

TEST_P(PackagerTestBasic, MP4MuxerSingleSegmentUnencryptedVideo) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputVideo,
                                kOutputNone,
                                kSingleSegment,
                                kDisableEncryption));
}

TEST_P(PackagerTestBasic, MP4MuxerSingleSegmentUnencryptedAudio) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputNone,
                                kOutputAudio,
                                kSingleSegment,
                                kDisableEncryption));
}

TEST_P(PackagerTestBasic, MP4MuxerSingleSegmentEncryptedVideo) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputVideo,
                                kOutputNone,
                                kSingleSegment,
                                kEnableEncryption));

  // Expect the output to be encrypted.
  Demuxer demuxer(GetFullPath(kOutputVideo), decryptor_source_);
  ASSERT_OK(demuxer.Initialize());
  ASSERT_EQ(1u, demuxer.streams().size());
  EXPECT_TRUE(demuxer.streams()[0]->info()->is_encrypted());
}

TEST_P(PackagerTestBasic, MP4MuxerSingleSegmentEncryptedAudio) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputNone,
                                kOutputAudio,
                                kSingleSegment,
                                kEnableEncryption));

  // Expect the output to be encrypted.
  Demuxer demuxer(GetFullPath(kOutputAudio), decryptor_source_);
  ASSERT_OK(demuxer.Initialize());
  ASSERT_EQ(1u, demuxer.streams().size());
  EXPECT_TRUE(demuxer.streams()[0]->info()->is_encrypted());
}

class PackagerTest : public PackagerTestBasic {
 public:
  virtual void SetUp() OVERRIDE {
    PackagerTestBasic::SetUp();

    ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                  kOutputVideo,
                                  kOutputNone,
                                  kSingleSegment,
                                  kDisableEncryption));

    ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                  kOutputNone,
                                  kOutputAudio,
                                  kSingleSegment,
                                  kDisableEncryption));
  }
};

TEST_P(PackagerTest, MP4MuxerSingleSegmentUnencryptedVideoAgain) {
  // Take the muxer output and feed into muxer again. The new muxer output
  // should contain the same contents as the previous muxer output.
  ASSERT_NO_FATAL_FAILURE(Remux(kOutputVideo,
                                kOutputVideo2,
                                kOutputNone,
                                kSingleSegment,
                                kDisableEncryption));
  EXPECT_TRUE(ContentsEqual(kOutputVideo, kOutputVideo2));
}

TEST_P(PackagerTest, MP4MuxerSingleSegmentUnencryptedAudioAgain) {
  // Take the muxer output and feed into muxer again. The new muxer output
  // should contain the same contents as the previous muxer output.
  ASSERT_NO_FATAL_FAILURE(Remux(kOutputAudio,
                                kOutputNone,
                                kOutputAudio2,
                                kSingleSegment,
                                kDisableEncryption));
  EXPECT_TRUE(ContentsEqual(kOutputAudio, kOutputAudio2));
}

TEST_P(PackagerTest, MP4MuxerSingleSegmentUnencryptedSeparateAudioVideo) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputVideo2,
                                kOutputAudio2,
                                kSingleSegment,
                                kDisableEncryption));

  // Compare the output with single muxer output. They should match.
  EXPECT_TRUE(ContentsEqual(kOutputVideo, kOutputVideo2));
  EXPECT_TRUE(ContentsEqual(kOutputAudio, kOutputAudio2));
}

TEST_P(PackagerTest, MP4MuxerMultiSegmentsUnencryptedVideo) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputVideo2,
                                kOutputNone,
                                kMultipleSegments,
                                kDisableEncryption));

  // Find and concatenates the segments.
  const std::string kOutputVideoSegmentsCombined =
      std::string(kOutputVideo) + "_combined";
  base::FilePath output_path =
      test_directory_.AppendASCII(kOutputVideoSegmentsCombined);
  ASSERT_TRUE(
      base::CopyFile(test_directory_.AppendASCII(kOutputVideo2), output_path));

  const int kStartSegmentIndex = 1;  // start from one.
  int segment_index = kStartSegmentIndex;
  while (true) {
    base::FilePath segment_path = test_directory_.AppendASCII(
        base::StringPrintf(kSegmentTemplateOutputPattern, segment_index));
    if (!base::PathExists(segment_path))
      break;

    std::string segment_content;
    ASSERT_TRUE(base::ReadFileToString(segment_path, &segment_content));
    int bytes_written = file_util::AppendToFile(
        output_path, segment_content.data(), segment_content.size());
    ASSERT_EQ(segment_content.size(), static_cast<size_t>(bytes_written));

    ++segment_index;
  }
  // We should have at least one segment.
  ASSERT_LT(kStartSegmentIndex, segment_index);

  // Feed the combined file into muxer again. The new muxer output should be
  // the same as by just feeding the input to muxer.
  ASSERT_NO_FATAL_FAILURE(Remux(kOutputVideoSegmentsCombined,
                                kOutputVideo2,
                                kOutputNone,
                                kSingleSegment,
                                kDisableEncryption));
  EXPECT_TRUE(ContentsEqual(kOutputVideo, kOutputVideo2));
}

INSTANTIATE_TEST_CASE_P(PackagerEndToEnd,
                        PackagerTestBasic,
                        ValuesIn(kMediaFiles));
INSTANTIATE_TEST_CASE_P(PackagerEndToEnd, PackagerTest, ValuesIn(kMediaFiles));

}  // namespace media
