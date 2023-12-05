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
#include <absl/strings/escaping.h>
#include <absl/strings/str_format.h>
#include <packager/file.h>
#include <packager/live_packager.h>
#include <packager/media/base/aes_decryptor.h>

namespace shaka {
namespace {

const char kKeyHex[] = "06313e0d02666fc455f15a56c363e392";
const char kIvHex[] = "c80be14086853cd52d9acd002392dc09";
const char kKeyId[] = "f3c5e0361e6654b28f8049c778b23946";

const double kSegmentDurationInSeconds = 5.0;
const int kNumSegments = 10;
const int kNumAudioSegments = 5;

std::vector<uint8_t> HexStringToVector(const std::string& hex_str) {
  std::string raw_str = absl::HexStringToBytes(hex_str);
  return std::vector<uint8_t>(raw_str.begin(), raw_str.end());
}

}  // namespace

class LivePackagerTest : public ::testing::Test {
 public:
  LivePackagerTest()
      : decryptor_(
            new media::AesCbcDecryptor(media::kPkcs5Padding,
                                       media::AesCryptor::kUseConstantIv)) {}
  void SetUp() override {
    empty_live_config_.segment_duration_sec = kSegmentDurationInSeconds;
  }

  static std::filesystem::path GetTestDataFilePath(const std::string& name) {
    auto data_dir = std::filesystem::u8path(TEST_DATA_DIR);
    return data_dir / name;
  }

  // Reads a test file from media/test/data directory and returns its content.
  static std::vector<uint8_t> ReadTestDataFile(const std::string& name) {
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
  std::unique_ptr<media::AesCbcDecryptor> decryptor_;
};

TEST_F(LivePackagerTest, SuccessVideoFMP4NoEncryption) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
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

TEST_F(LivePackagerTest, SuccessFmp4MpegTsAes128) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  ASSERT_TRUE(decryptor_->InitializeWithIv(HexStringToVector(kKeyHex),
                                           HexStringToVector(kIvHex)));

  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    FullSegmentBuffer in;
    in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());
    in.AppendData(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config = empty_live_config_;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.key = HexStringToVector(kKeyHex);
    live_config.iv = HexStringToVector(kIvHex);
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;

    LivePackager livePackager(live_config);
    ASSERT_EQ(kSegmentDurationInSeconds,
              empty_live_config_.segment_duration_sec);
    ASSERT_EQ(Status::OK, livePackager.Package(in, out));
    ASSERT_GT(out.SegmentSize(), 0);

    std::string exp_segment_num = absl::StrFormat("expected/ts/%04d.ts", i + 1);
    std::vector<uint8_t> exp_segment_buffer = ReadTestDataFile(exp_segment_num);
    ASSERT_FALSE(exp_segment_buffer.empty());

    std::vector<uint8_t> decrypted;
    std::vector<uint8_t> buffer(out.SegmentData(),
                                out.SegmentData() + out.SegmentSize());

    ASSERT_TRUE(decryptor_->Crypt(buffer, &decrypted));
    ASSERT_EQ(decrypted, exp_segment_buffer);
  }
}

TEST_F(LivePackagerTest, SuccessFmp4MpegTsSampleAes) {
  std::vector<uint8_t> init_segment_buffer =
      ReadTestDataFile("audio/en/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  decryptor_.reset(new media::AesCbcDecryptor(
      media::kNoPadding, media::AesCryptor::kUseConstantIv));

  ASSERT_TRUE(decryptor_->InitializeWithIv(HexStringToVector(kKeyHex),
                                           HexStringToVector(kIvHex)));

  for (unsigned int i = 0; i < kNumAudioSegments; i++) {
    std::string segment_num = absl::StrFormat("audio/en/%05d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    FullSegmentBuffer in;
    in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());
    in.AppendData(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config = empty_live_config_;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::AUDIO;
    live_config.key = HexStringToVector(kKeyHex);
    live_config.iv = HexStringToVector(kIvHex);
    live_config.key_id = HexStringToVector(kKeyId);
    live_config.protection_scheme = LiveConfig::EncryptionScheme::SAMPLE_AES;

    LivePackager livePackager(live_config);
    ASSERT_EQ(kSegmentDurationInSeconds,
              empty_live_config_.segment_duration_sec);
    ASSERT_EQ(Status::OK, livePackager.Package(in, out));
    ASSERT_GT(out.SegmentSize(), 0);

    std::vector<uint8_t> decrypted;
    std::vector<uint8_t> buffer(out.SegmentData(),
                                out.SegmentData() + out.SegmentSize());

    ASSERT_TRUE(decryptor_->Crypt(buffer, &decrypted));
  }
}

TEST_F(LivePackagerTest, InitSegmentOnly) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
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
