// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <regex>

#include <absl/log/log.h>
#include <packager/file.h>
#include <packager/packager.h>

using testing::_;
using testing::HasSubstr;
using testing::Invoke;
using testing::MockFunction;
using testing::Return;
using testing::ReturnArg;
using testing::StrEq;
using testing::UnitTest;
using testing::WithArgs;

namespace shaka {
namespace {

const char kTestFile[] = "packager/media/test/data/bear-640x360.mp4";
const char kOutputVideo[] = "output_video.mp4";
const char kOutputVideoTemplate[] = "output_video_$Number$.m4s";
const char kOutputAudio[] = "output_audio.mp4";
const char kOutputAudioTemplate[] = "output_audio_$Number$.m4s";
const char kOutputMpd[] = "output.mpd";

const double kSegmentDurationInSeconds = 1.0;
const uint8_t kKeyId[] = {
    0xe5, 0x00, 0x7e, 0x6e, 0x9d, 0xcd, 0x5a, 0xc0,
    0x95, 0x20, 0x2e, 0xd3, 0x75, 0x83, 0x82, 0xcd,
};
const uint8_t kKey[]{
    0x6f, 0xc9, 0x6f, 0xe6, 0x28, 0xa2, 0x65, 0xb1,
    0x3a, 0xed, 0xde, 0xc0, 0xbc, 0x42, 0x1f, 0x4d,
};
const double kClearLeadInSeconds = 1.0;
const double kFragmentDurationInSeconds = 5.0;

}  // namespace

class PackagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    FILE* f = fopen(kTestFile, "rb");
    if (!f) {
      FAIL() << "The test is expected to run from packager repository root.";
      return;
    }
    fclose(f);

    // Use memory file for testing and generate different test directories for
    // different tests.
    test_directory_ = std::string("memory://test/") +
                      UnitTest::GetInstance()->current_test_info()->name() +
                      "/";
  }

  std::string GetFullPath(const std::string& file_name) {
    return test_directory_ + file_name;
  }

  PackagingParams SetupPackagingParams() {
    PackagingParams packaging_params;
    packaging_params.temp_dir = test_directory_;
    packaging_params.chunking_params.segment_duration_in_seconds =
        kSegmentDurationInSeconds;
    packaging_params.mpd_params.mpd_output = GetFullPath(kOutputMpd);

    packaging_params.encryption_params.clear_lead_in_seconds =
        kClearLeadInSeconds;
    packaging_params.encryption_params.key_provider = KeyProvider::kRawKey;
    packaging_params.encryption_params.raw_key.key_map[""].key_id.assign(
        std::begin(kKeyId), std::end(kKeyId));
    packaging_params.encryption_params.raw_key.key_map[""].key.assign(
        std::begin(kKey), std::end(kKey));
    return packaging_params;
  }

  std::vector<StreamDescriptor> SetupStreamDescriptors() {
    std::vector<StreamDescriptor> stream_descriptors;
    StreamDescriptor stream_descriptor;

    stream_descriptor.input = kTestFile;
    stream_descriptor.stream_selector = "video";
    stream_descriptor.output = GetFullPath(kOutputVideo);
    stream_descriptors.push_back(stream_descriptor);

    stream_descriptor.input = kTestFile;
    stream_descriptor.stream_selector = "audio";
    stream_descriptor.output = GetFullPath(kOutputAudio);
    stream_descriptors.push_back(stream_descriptor);

    return stream_descriptors;
  }

 protected:
  std::string test_directory_;
};

TEST_F(PackagerTest, Version) {
  EXPECT_FALSE(Packager::GetLibraryVersion().empty());
}

TEST_F(PackagerTest, Success) {
  Packager packager;
  ASSERT_EQ(Status::OK, packager.Initialize(SetupPackagingParams(),
                                            SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());
}

TEST_F(PackagerTest, MissingStreamDescriptors) {
  std::vector<StreamDescriptor> stream_descriptors;
  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST_F(PackagerTest, MixingSegmentTemplateAndSingleSegment) {
  std::vector<StreamDescriptor> stream_descriptors;
  StreamDescriptor stream_descriptor;

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputAudio);
  stream_descriptor.segment_template.clear();
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST_F(PackagerTest, DuplicatedOutputs) {
  std::vector<StreamDescriptor> stream_descriptors;
  StreamDescriptor stream_descriptor;

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputAudioTemplate);
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("duplicated outputs"));
}

TEST_F(PackagerTest, DuplicatedSegmentTemplates) {
  std::vector<StreamDescriptor> stream_descriptors;
  StreamDescriptor stream_descriptor;

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "video";
  stream_descriptor.output = GetFullPath(kOutputVideo);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  stream_descriptor.input = kTestFile;
  stream_descriptor.stream_selector = "audio";
  stream_descriptor.output = GetFullPath(kOutputAudio);
  stream_descriptor.segment_template = GetFullPath(kOutputVideoTemplate);
  stream_descriptors.push_back(stream_descriptor);

  Packager packager;
  auto status = packager.Initialize(SetupPackagingParams(), stream_descriptors);
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("duplicated segment templates"));
}

TEST_F(PackagerTest, SegmentAlignedAndSubsegmentNotAligned) {
  auto packaging_params = SetupPackagingParams();
  packaging_params.chunking_params.segment_sap_aligned = true;
  packaging_params.chunking_params.subsegment_sap_aligned = false;
  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());
}

TEST_F(PackagerTest, SegmentNotAlignedButSubsegmentAligned) {
  auto packaging_params = SetupPackagingParams();
  packaging_params.chunking_params.segment_sap_aligned = false;
  packaging_params.chunking_params.subsegment_sap_aligned = true;
  Packager packager;
  auto status = packager.Initialize(packaging_params, SetupStreamDescriptors());
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
}

TEST_F(PackagerTest, WriteOutputToBuffer) {
  auto packaging_params = SetupPackagingParams();

  MockFunction<int64_t(const std::string& name, const void* buffer,
                       uint64_t length)>
      mock_write_func;
  packaging_params.buffer_callback_params.write_func =
      mock_write_func.AsStdFunction();
  EXPECT_CALL(mock_write_func, Call(StrEq(GetFullPath(kOutputVideo)), _, _))
      .WillRepeatedly(ReturnArg<2>());
  EXPECT_CALL(mock_write_func, Call(StrEq(GetFullPath(kOutputAudio)), _, _))
      .WillRepeatedly(ReturnArg<2>());
  EXPECT_CALL(mock_write_func, Call(StrEq(GetFullPath(kOutputMpd)), _, _))
      .WillRepeatedly(ReturnArg<2>());

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());
}

TEST_F(PackagerTest, ReadFromBuffer) {
  auto packaging_params = SetupPackagingParams();

  MockFunction<int64_t(const std::string& name, void* buffer, uint64_t length)>
      mock_read_func;
  packaging_params.buffer_callback_params.read_func =
      mock_read_func.AsStdFunction();

  const std::string file_name = kTestFile;
  FILE* file_ptr = fopen(file_name.c_str(), "rb");
  ASSERT_TRUE(file_ptr);
  EXPECT_CALL(mock_read_func, Call(StrEq(file_name), _, _))
      .WillRepeatedly(
          WithArgs<1, 2>(Invoke([file_ptr](void* buffer, uint64_t size) {
            return fread(buffer, sizeof(char), size, file_ptr);
          })));

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(Status::OK, packager.Run());

  fclose(file_ptr);
}

TEST_F(PackagerTest, ReadFromBufferFailed) {
  auto packaging_params = SetupPackagingParams();

  MockFunction<int64_t(const std::string& name, void* buffer, uint64_t length)>
      mock_read_func;
  packaging_params.buffer_callback_params.read_func =
      mock_read_func.AsStdFunction();

  EXPECT_CALL(mock_read_func, Call(_, _, _)).WillOnce(Return(-1));

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, SetupStreamDescriptors()));
  ASSERT_EQ(error::FILE_FAILURE, packager.Run().error_code());
}

TEST_F(PackagerTest, LowLatencyDashEnabledAndFragmentDurationSet) {
  auto packaging_params = SetupPackagingParams();
  packaging_params.chunking_params.low_latency_dash_mode = true;
  packaging_params.chunking_params.subsegment_duration_in_seconds =
      kFragmentDurationInSeconds;
  Packager packager;
  auto status = packager.Initialize(packaging_params, SetupStreamDescriptors());
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("--fragment_duration cannot be set"));
}

TEST_F(PackagerTest, LowLatencyDashEnabledAndUtcTimingNotSet) {
  auto packaging_params = SetupPackagingParams();
  packaging_params.mpd_params.low_latency_dash_mode = true;
  Packager packager;
  auto status = packager.Initialize(packaging_params, SetupStreamDescriptors());
  ASSERT_EQ(error::INVALID_ARGUMENT, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("--utc_timings must be be set"));
}

namespace {

// Helper to extract SegmentTimeline entries from an AdaptationSet in MPD XML.
// Returns a vector of pairs (start_time, duration) for each segment.
std::vector<std::pair<int64_t, int64_t>> ExtractSegmentTimeline(
    const std::string& mpd_content,
    const std::string& content_type) {
  std::vector<std::pair<int64_t, int64_t>> segments;

  // Find the AdaptationSet with the specified contentType
  std::regex adaptation_regex("AdaptationSet[^>]*contentType=\"" +
                              content_type +
                              "\"[^>]*>([\\s\\S]*?)</AdaptationSet>");
  std::smatch adaptation_match;
  if (!std::regex_search(mpd_content, adaptation_match, adaptation_regex)) {
    return segments;
  }
  std::string adaptation_content = adaptation_match[1].str();

  // Find the SegmentTimeline within this AdaptationSet
  std::regex timeline_regex("<SegmentTimeline>([\\s\\S]*?)</SegmentTimeline>");
  std::smatch timeline_match;
  if (!std::regex_search(adaptation_content, timeline_match, timeline_regex)) {
    return segments;
  }
  std::string timeline_content = timeline_match[1].str();

  // Parse each <S> element: <S t="..." d="..." r="..."/>
  std::regex s_regex(
      "<S[^>]*t=\"([0-9]+)\"[^>]*d=\"([0-9]+)\"(?:[^>]*r=\"([0-9]+)\")?");
  auto s_begin = std::sregex_iterator(timeline_content.begin(),
                                      timeline_content.end(), s_regex);
  auto s_end = std::sregex_iterator();

  int64_t current_time = 0;
  for (std::sregex_iterator i = s_begin; i != s_end; ++i) {
    std::smatch match = *i;
    int64_t t = std::stoll(match[1].str());
    int64_t d = std::stoll(match[2].str());
    int repeat =
        (match.size() > 3 && match[3].matched) ? std::stoi(match[3].str()) : 0;

    current_time = t;
    // Add segment and any repeats
    for (int r = 0; r <= repeat; ++r) {
      segments.push_back({current_time, d});
      current_time += d;
    }
  }

  return segments;
}

// Extract cue start times from VTT content (in seconds)
// VTT format: "HH:MM:SS.mmm --> HH:MM:SS.mmm"
std::vector<double> ExtractVttCueStartTimes(const std::string& vtt_content) {
  std::vector<double> start_times;
  std::regex cue_regex(R"((\d+):(\d+):(\d+)\.(\d+)\s*-->)");
  auto cue_begin =
      std::sregex_iterator(vtt_content.begin(), vtt_content.end(), cue_regex);
  auto cue_end = std::sregex_iterator();

  for (std::sregex_iterator i = cue_begin; i != cue_end; ++i) {
    std::smatch match = *i;
    int hours = std::stoi(match[1].str());
    int minutes = std::stoi(match[2].str());
    int seconds = std::stoi(match[3].str());
    int millis = std::stoi(match[4].str());
    double start_time =
        hours * 3600.0 + minutes * 60.0 + seconds + millis / 1000.0;
    start_times.push_back(start_time);
  }
  return start_times;
}

}  // namespace

class TeletextSegmentAlignmentTest : public ::testing::Test {
 public:
  void SetUp() override {
    test_directory_ =
        std::string("memory://teletext_test/") +
        testing::UnitTest::GetInstance()->current_test_info()->name() + "/";
  }

  std::string GetFullPath(const std::string& file_name) {
    return test_directory_ + file_name;
  }

 protected:
  std::string test_directory_;
};

TEST_F(TeletextSegmentAlignmentTest, VideoAndTextSegmentsAligned) {
  // Test that video and teletext segments have the same start times and
  // durations when using the SegmentCoordinator feature.
  //
  // test_teletext_live.ts timing:
  //   First video PTS: 324216000 (3602.4s at 90kHz)
  //   First text cue:  324306000 (1 second after video start)
  //   Text cues: 1.0-3.0s, 3.5-4.5s, 13.0-21.0s relative to video start
  const char* kTeletextTestFile =
      "packager/media/test/data/test_teletext_live.ts";
  constexpr int64_t kExpectedFirstVideoPts = 324216000;
  // Expected cue start times relative to first video PTS (in seconds)
  const std::vector<double> kExpectedCueStartTimes = {1.0, 3.5, 13.0};

  PackagingParams packaging_params;
  packaging_params.temp_dir = test_directory_;
  packaging_params.chunking_params.segment_duration_in_seconds = 6.0;
  packaging_params.mpd_params.mpd_output = GetFullPath("manifest.mpd");

  std::vector<StreamDescriptor> stream_descriptors;

  StreamDescriptor video_desc;
  video_desc.input = kTeletextTestFile;
  video_desc.stream_selector = "video";
  video_desc.output = GetFullPath("video/init.mp4");
  video_desc.segment_template = GetFullPath("video/$Number$.m4s");
  stream_descriptors.push_back(video_desc);

  StreamDescriptor text_desc;
  text_desc.input = kTeletextTestFile;
  text_desc.stream_selector = "text";
  text_desc.cc_index = 888;  // Teletext page
  text_desc.language = "eng";
  text_desc.output = GetFullPath("text/init.mp4");
  text_desc.segment_template = GetFullPath("text/$Number$.m4s");
  text_desc.dash_only = true;
  stream_descriptors.push_back(text_desc);

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, stream_descriptors));
  ASSERT_EQ(Status::OK, packager.Run());

  // Run second packager instance for plain VTT output (for cue timing
  // verification)
  {
    PackagingParams vtt_params;
    vtt_params.temp_dir = test_directory_;

    std::vector<StreamDescriptor> vtt_descriptors;
    StreamDescriptor vtt_desc;
    vtt_desc.input = kTeletextTestFile;
    vtt_desc.stream_selector = "text";
    vtt_desc.cc_index = 888;
    vtt_desc.language = "eng";
    vtt_desc.output = GetFullPath("subtitles.vtt");
    vtt_descriptors.push_back(vtt_desc);

    Packager vtt_packager;
    ASSERT_EQ(Status::OK, vtt_packager.Initialize(vtt_params, vtt_descriptors));
    ASSERT_EQ(Status::OK, vtt_packager.Run());
  }

  // Read the MPD output
  std::string mpd_content;
  ASSERT_TRUE(File::ReadFileToString(GetFullPath("manifest.mpd").c_str(),
                                     &mpd_content));

  // Extract segment timelines for video and text
  auto video_segments = ExtractSegmentTimeline(mpd_content, "video");
  auto text_segments = ExtractSegmentTimeline(mpd_content, "text");

  ASSERT_FALSE(video_segments.empty()) << "No video segments found in MPD";
  ASSERT_FALSE(text_segments.empty()) << "No text segments found in MPD";

  // Verify first segment starts at expected video PTS
  EXPECT_EQ(video_segments[0].first, kExpectedFirstVideoPts)
      << "First video segment should start at expected PTS";
  EXPECT_EQ(text_segments[0].first, kExpectedFirstVideoPts)
      << "First text segment should start at same PTS as video";

  // Verify video and text have the same number of segments
  ASSERT_EQ(video_segments.size(), text_segments.size())
      << "Video and text segment count mismatch: " << "video="
      << video_segments.size() << " text=" << text_segments.size();

  // Verify video and text segments have the same start times and durations
  for (size_t i = 0; i < video_segments.size(); ++i) {
    EXPECT_EQ(video_segments[i].first, text_segments[i].first)
        << "Segment " << i
        << " start time mismatch: " << "video=" << video_segments[i].first
        << " text=" << text_segments[i].first;
    EXPECT_EQ(video_segments[i].second, text_segments[i].second)
        << "Segment " << i
        << " duration mismatch: " << "video=" << video_segments[i].second
        << " text=" << text_segments[i].second;
  }

  // Read and parse VTT file to verify cue timing relative to video
  std::string vtt_content;
  ASSERT_TRUE(File::ReadFileToString(GetFullPath("subtitles.vtt").c_str(),
                                     &vtt_content));
  auto cue_start_times = ExtractVttCueStartTimes(vtt_content);

  ASSERT_EQ(cue_start_times.size(), kExpectedCueStartTimes.size())
      << "Expected " << kExpectedCueStartTimes.size() << " cues in VTT";

  // VTT times use PTS/1000 format (90kHz / 1000 = 90x actual seconds)
  // This is intentional for MPEG-TS sync with X-TIMESTAMP-MAP header.
  // We verify relative timing between cues is correct (scaled by 90)
  constexpr double kVttTimescale = 90.0;  // VTT uses PTS/1000, so 90x actual

  // Convert first video PTS to VTT timescale for offset calculation
  double first_video_vtt_time =
      static_cast<double>(kExpectedFirstVideoPts) / 1000.0;

  // Verify each cue starts at the expected offset from video start
  for (size_t i = 0; i < cue_start_times.size(); ++i) {
    // Convert VTT time to actual seconds: (vtt_time - video_vtt_time) / 90
    double relative_cue_seconds =
        (cue_start_times[i] - first_video_vtt_time) / kVttTimescale;
    EXPECT_NEAR(relative_cue_seconds, kExpectedCueStartTimes[i], 0.1)
        << "Cue " << i << " start time mismatch: " << "got "
        << relative_cue_seconds << "s relative to video start, expected "
        << kExpectedCueStartTimes[i] << "s";
  }

  LOG(INFO) << "Segment alignment test: " << video_segments.size()
            << " segments verified aligned, " << cue_start_times.size()
            << " cue times verified";
}

TEST_F(TeletextSegmentAlignmentTest,
       VideoAndTextSegmentsAlignedWithWrapAround) {
  // Test that video and teletext segments remain aligned even when PTS
  // timestamps cross the 33-bit wrap-around boundary.
  //
  // test_teletext_live_wrap.ts timing (same relative timing as live.ts):
  //   First video PTS: 8588584592 (near 2^33 wrap point)
  //   First text cue:  8588674592 (1 second after video start)
  //   Text cues: 1.0-3.0s, 3.5-4.5s, 13.0-21.0s relative to video start
  const char* kTeletextWrapTestFile =
      "packager/media/test/data/test_teletext_live_wrap.ts";
  constexpr int64_t kExpectedFirstVideoPts = 8588584592;
  // Expected cue start times relative to first video PTS (in seconds)
  // Same as non-wrap test since timing is preserved
  const std::vector<double> kExpectedCueStartTimes = {1.0, 3.5, 13.0};

  PackagingParams packaging_params;
  packaging_params.temp_dir = test_directory_;
  packaging_params.chunking_params.segment_duration_in_seconds = 6.0;
  packaging_params.mpd_params.mpd_output = GetFullPath("manifest.mpd");

  std::vector<StreamDescriptor> stream_descriptors;

  StreamDescriptor video_desc;
  video_desc.input = kTeletextWrapTestFile;
  video_desc.stream_selector = "video";
  video_desc.output = GetFullPath("video/init.mp4");
  video_desc.segment_template = GetFullPath("video/$Number$.m4s");
  stream_descriptors.push_back(video_desc);

  StreamDescriptor text_desc;
  text_desc.input = kTeletextWrapTestFile;
  text_desc.stream_selector = "text";
  text_desc.cc_index = 888;  // Teletext page
  text_desc.language = "eng";
  text_desc.output = GetFullPath("text/init.mp4");
  text_desc.segment_template = GetFullPath("text/$Number$.m4s");
  text_desc.dash_only = true;
  stream_descriptors.push_back(text_desc);

  Packager packager;
  ASSERT_EQ(Status::OK,
            packager.Initialize(packaging_params, stream_descriptors));
  ASSERT_EQ(Status::OK, packager.Run());

  // Run second packager instance for plain VTT output (for cue timing
  // verification)
  {
    PackagingParams vtt_params;
    vtt_params.temp_dir = test_directory_;

    std::vector<StreamDescriptor> vtt_descriptors;
    StreamDescriptor vtt_desc;
    vtt_desc.input = kTeletextWrapTestFile;
    vtt_desc.stream_selector = "text";
    vtt_desc.cc_index = 888;
    vtt_desc.language = "eng";
    vtt_desc.output = GetFullPath("subtitles.vtt");
    vtt_descriptors.push_back(vtt_desc);

    Packager vtt_packager;
    ASSERT_EQ(Status::OK, vtt_packager.Initialize(vtt_params, vtt_descriptors));
    ASSERT_EQ(Status::OK, vtt_packager.Run());
  }

  // Read the MPD output
  std::string mpd_content;
  ASSERT_TRUE(File::ReadFileToString(GetFullPath("manifest.mpd").c_str(),
                                     &mpd_content));

  // Extract segment timelines for video and text
  auto video_segments = ExtractSegmentTimeline(mpd_content, "video");
  auto text_segments = ExtractSegmentTimeline(mpd_content, "text");

  ASSERT_FALSE(video_segments.empty()) << "No video segments found in MPD";
  ASSERT_FALSE(text_segments.empty()) << "No text segments found in MPD";

  // Verify first segment starts at expected video PTS (near wrap point)
  EXPECT_EQ(video_segments[0].first, kExpectedFirstVideoPts)
      << "First video segment should start at expected PTS near wrap point";
  EXPECT_EQ(text_segments[0].first, kExpectedFirstVideoPts)
      << "First text segment should start at same PTS as video";

  // Verify segments span the wrap-around point (2^33 = 8589934592)
  constexpr int64_t kPtsWrapAround = 1LL << 33;
  bool has_pre_wrap = false;
  bool has_post_wrap = false;

  for (const auto& seg : video_segments) {
    if (seg.first < kPtsWrapAround)
      has_pre_wrap = true;
    if (seg.first + seg.second > kPtsWrapAround)
      has_post_wrap = true;
  }

  EXPECT_TRUE(has_pre_wrap) << "Expected segments before wrap-around point";
  EXPECT_TRUE(has_post_wrap) << "Expected segments after wrap-around point";

  // Verify video and text have the same number of segments
  ASSERT_EQ(video_segments.size(), text_segments.size())
      << "Video and text segment count mismatch (wrap-around test): "
      << "video=" << video_segments.size() << " text=" << text_segments.size();

  // Verify video and text segments have the same start times and durations
  for (size_t i = 0; i < video_segments.size(); ++i) {
    EXPECT_EQ(video_segments[i].first, text_segments[i].first)
        << "Segment " << i << " start time mismatch (wrap-around test): "
        << "video=" << video_segments[i].first
        << " text=" << text_segments[i].first;
    EXPECT_EQ(video_segments[i].second, text_segments[i].second)
        << "Segment " << i << " duration mismatch (wrap-around test): "
        << "video=" << video_segments[i].second
        << " text=" << text_segments[i].second;
  }

  // Read and parse VTT file to verify cue timing relative to video
  std::string vtt_content;
  ASSERT_TRUE(File::ReadFileToString(GetFullPath("subtitles.vtt").c_str(),
                                     &vtt_content));
  auto cue_start_times = ExtractVttCueStartTimes(vtt_content);

  ASSERT_EQ(cue_start_times.size(), kExpectedCueStartTimes.size())
      << "Expected " << kExpectedCueStartTimes.size() << " cues in VTT";

  // VTT times use PTS/1000 format (90kHz / 1000 = 90x actual seconds)
  // This is intentional for MPEG-TS sync with X-TIMESTAMP-MAP header.
  constexpr double kVttTimescale = 90.0;  // VTT uses PTS/1000, so 90x actual

  // Convert first video PTS to VTT timescale for offset calculation
  double first_video_vtt_time =
      static_cast<double>(kExpectedFirstVideoPts) / 1000.0;

  // Verify each cue starts at the expected offset from video start
  // despite the PTS wrap-around
  for (size_t i = 0; i < cue_start_times.size(); ++i) {
    // Convert VTT time to actual seconds: (vtt_time - video_vtt_time) / 90
    double relative_cue_seconds =
        (cue_start_times[i] - first_video_vtt_time) / kVttTimescale;
    EXPECT_NEAR(relative_cue_seconds, kExpectedCueStartTimes[i], 0.1)
        << "Cue " << i << " start time mismatch (wrap-around): " << "got "
        << relative_cue_seconds << "s relative to video start, expected "
        << kExpectedCueStartTimes[i] << "s";
  }

  LOG(INFO) << "Wrap-around segment alignment test: " << video_segments.size()
            << " segments verified aligned, " << cue_start_times.size()
            << " cue times verified across PTS wrap boundary";
}

// TODO(kqyang): Add more tests.

}  // namespace shaka
