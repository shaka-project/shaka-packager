// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/files/file_util.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/time/clock.h"
#include "packager/media/base/demuxer.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/fourccs.h"
#include "packager/media/base/muxer.h"
#include "packager/media/base/muxer_util.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/base/test/status_test_util.h"
#include "packager/media/chunking/chunking_handler.h"
#include "packager/media/formats/mp4/mp4_muxer.h"
#include "packager/media/test/test_data_util.h"

using ::testing::ValuesIn;

namespace shaka {
namespace media {
namespace {

const char* kMediaFiles[] = {"bear-640x360.mp4", "bear-640x360-av_frag.mp4",
                             "bear-640x360.ts"};

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
const double kClearLeadInSeconds = 1.5;
const double kCryptoDurationInSeconds = 0;  // Key rotation is disabled.

// Track resolution constants.
const uint32_t kMaxSDPixels = 640 * 480;
const uint32_t kMaxHDPixels = 1920 * 1080;
const uint32_t kMaxUHD1Pixels = 4096 * 2160;

}  // namespace

class FakeClock : public base::Clock {
 public:
  // Fake the clock to return NULL time.
  base::Time Now() override { return base::Time(); }
};

class PackagerTestBasic : public ::testing::TestWithParam<const char*> {
 public:
  PackagerTestBasic() {}

  void SetUp() override {
    // Create a test directory for testing, will be deleted after test.
    ASSERT_TRUE(base::CreateNewTempDirectory(
        base::FilePath::FromUTF8Unsafe("packager_").value(), &test_directory_));

    // Copy the input to test directory for easy reference.
    ASSERT_TRUE(base::CopyFile(GetTestDataFilePath(GetParam()),
                               test_directory_.AppendASCII(GetParam())));
  }

  void TearDown() override { base::DeleteFile(test_directory_, true); }

  std::string GetFullPath(const std::string& file_name);
  // Check if |file1| and |file2| are the same.
  bool ContentsEqual(const std::string& file1, const std::string file2);

  ChunkingOptions SetupChunkingOptions();
  MuxerOptions SetupMuxerOptions(const std::string& output,
                                 bool single_segment);
  void Remux(const std::string& input,
             const std::string& video_output,
             const std::string& audio_output,
             bool single_segment,
             bool enable_encryption);

  void Decrypt(const std::string& input,
               const std::string& video_output,
               const std::string& audio_output);

 protected:
  base::FilePath test_directory_;
  FakeClock fake_clock_;
};

std::string PackagerTestBasic::GetFullPath(const std::string& file_name) {
  return test_directory_.Append(
      base::FilePath::FromUTF8Unsafe(file_name)).AsUTF8Unsafe();
}

bool PackagerTestBasic::ContentsEqual(const std::string& file1,
                                      const std::string file2) {
  return base::ContentsEqual(test_directory_.AppendASCII(file1),
                             test_directory_.AppendASCII(file2));
}

MuxerOptions PackagerTestBasic::SetupMuxerOptions(const std::string& output,
                                                  bool single_segment) {
  MuxerOptions options;
  options.num_subsegments_per_sidx = kNumSubsegmentsPerSidx;
  options.output_file_name = GetFullPath(output);
  if (!single_segment)
    options.segment_template = GetFullPath(kSegmentTemplate);
  options.temp_dir = test_directory_.AsUTF8Unsafe();
  return options;
}

ChunkingOptions PackagerTestBasic::SetupChunkingOptions() {
  ChunkingOptions options;
  options.segment_duration_in_seconds = kSegmentDurationInSeconds;
  options.subsegment_duration_in_seconds = kFragmentDurationInSecodns;
  options.segment_sap_aligned = kSegmentSapAligned;
  options.subsegment_sap_aligned = kFragmentSapAligned;
  return options;
}

void PackagerTestBasic::Remux(const std::string& input,
                              const std::string& video_output,
                              const std::string& audio_output,
                              bool single_segment,
                              bool enable_encryption) {
  CHECK(!video_output.empty() || !audio_output.empty());

  Demuxer demuxer(GetFullPath(input));
  std::unique_ptr<KeySource> encryption_key_source(
      FixedKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, "", ""));
  DCHECK(encryption_key_source);

  std::shared_ptr<Muxer> muxer_video;
  if (!video_output.empty()) {
    muxer_video.reset(
        new mp4::MP4Muxer(SetupMuxerOptions(video_output, single_segment)));
    muxer_video->set_clock(&fake_clock_);

    if (enable_encryption) {
      muxer_video->SetKeySource(encryption_key_source.get(),
                                kMaxSDPixels, kMaxHDPixels,
                                kMaxUHD1Pixels, kClearLeadInSeconds,
                                kCryptoDurationInSeconds, FOURCC_cenc);
    }

    auto chunking_handler =
        std::make_shared<ChunkingHandler>(SetupChunkingOptions());
    ASSERT_OK(demuxer.SetHandler("video", chunking_handler));
    ASSERT_OK(chunking_handler->SetHandler(0, muxer_video));
  }

  std::shared_ptr<Muxer> muxer_audio;
  if (!audio_output.empty()) {
    muxer_audio.reset(
        new mp4::MP4Muxer(SetupMuxerOptions(audio_output, single_segment)));
    muxer_audio->set_clock(&fake_clock_);

    if (enable_encryption) {
      muxer_audio->SetKeySource(encryption_key_source.get(),
                                kMaxSDPixels, kMaxHDPixels,
                                kMaxUHD1Pixels, kClearLeadInSeconds,
                                kCryptoDurationInSeconds, FOURCC_cenc);
    }

    auto chunking_handler =
        std::make_shared<ChunkingHandler>(SetupChunkingOptions());
    ASSERT_OK(demuxer.SetHandler("audio", chunking_handler));
    ASSERT_OK(chunking_handler->SetHandler(0, muxer_audio));
  }

  ASSERT_OK(demuxer.Initialize());
  // Start remuxing process.
  ASSERT_OK(demuxer.Run());
}

void PackagerTestBasic::Decrypt(const std::string& input,
                                const std::string& video_output,
                                const std::string& audio_output) {
  CHECK(!video_output.empty() || !audio_output.empty());

  Demuxer demuxer(GetFullPath(input));
  std::unique_ptr<KeySource> decryption_key_source(
      FixedKeySource::CreateFromHexStrings(kKeyIdHex, kKeyHex, "", ""));
  ASSERT_TRUE(decryption_key_source);
  demuxer.SetKeySource(std::move(decryption_key_source));

  std::shared_ptr<Muxer> muxer;
  if (!video_output.empty()) {
    muxer.reset(new mp4::MP4Muxer(SetupMuxerOptions(video_output, true)));
  }
  if (!audio_output.empty()) {
    muxer.reset(new mp4::MP4Muxer(SetupMuxerOptions(audio_output, true)));
  }
  ASSERT_TRUE(muxer);
  muxer->set_clock(&fake_clock_);

  auto chunking_handler =
      std::make_shared<ChunkingHandler>(SetupChunkingOptions());
  ASSERT_OK(demuxer.SetHandler("0", chunking_handler));
  ASSERT_OK(chunking_handler->SetHandler(0, muxer));

  ASSERT_OK(demuxer.Initialize());
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

  ASSERT_NO_FATAL_FAILURE(Decrypt(kOutputVideo,
                                  kOutputVideo2,
                                  kOutputNone));
}

TEST_P(PackagerTestBasic, MP4MuxerSingleSegmentEncryptedAudio) {
  ASSERT_NO_FATAL_FAILURE(Remux(GetParam(),
                                kOutputNone,
                                kOutputAudio,
                                kSingleSegment,
                                kEnableEncryption));

  ASSERT_NO_FATAL_FAILURE(Decrypt(kOutputAudio,
                                  kOutputNone,
                                  kOutputAudio2));
}


class PackagerTest : public PackagerTestBasic {
 public:
  void SetUp() override {
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
    EXPECT_TRUE(base::AppendToFile(output_path, segment_content.data(),
                                   static_cast<int>(segment_content.size())));

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
}  // namespace shaka
