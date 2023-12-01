// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <absl/log/log.h>
#include <absl/strings/str_format.h>
#include <packager/file.h>
#include <packager/live_packager.h>

namespace shaka {
namespace {

const double kSegmentDurationInSeconds = 5.0;
const int kNumSegments = 10;

}  // namespace

class LivePackagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    empty_live_config_.segment_duration_sec = kSegmentDurationInSeconds;
  }

  static std::filesystem::path GetTestDataFilePath(const std::string& name) {
    auto data_dir = std::filesystem::u8path(TEST_DATA_DIR);
    return data_dir / name;
  }

  // Reads a test file from media/test/data directory and returns its content.
  std::vector<uint8_t> ReadTestDataFile(const std::string& name) {
    auto path = GetTestDataFilePath(name);

    FILE* f = fopen(path.string().c_str(), "rb");
    if (!f) {
      LOG(ERROR) << "Failed to read test data from " << path;
      return std::vector<uint8_t>();
    }

    std::vector<uint8_t> data;
    data.resize(std::filesystem::file_size(path));
    size_t size = fread(data.data(), 1, data.size(), f);
    data.resize(size);
    fclose(f);

    return data;
  }

 protected:
  LiveConfig empty_live_config_{};
};

TEST_F(LivePackagerTest, Success) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string segment_num = absl::StrFormat("%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    FullSegmentBuffer in;
    in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());
    in.AppendData(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config = empty_live_config_;
    live_config.format = LiveConfig::OutputFormat::FMP4;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    LivePackager livePackager(live_config);
    ASSERT_EQ(kSegmentDurationInSeconds,
              empty_live_config_.segment_duration_sec);
    ASSERT_EQ(Status::OK, livePackager.Package(in, out));
    ASSERT_GT(out.InitSegmentSize(), 0);
    ASSERT_GT(out.SegmentSize(), 0);
  }
}

TEST_F(LivePackagerTest, InitSegmentOnly) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  FullSegmentBuffer in;
  in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());

  FullSegmentBuffer out;

  LiveConfig live_config = empty_live_config_;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  LivePackager livePackager(live_config);
  ASSERT_EQ(Status::OK, livePackager.PackageInit(in, out));
  ASSERT_GT(out.InitSegmentSize(), 0);
  ASSERT_EQ(out.SegmentSize(), 0);
}
}  // namespace shaka
