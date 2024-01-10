// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <absl/log/log.h>
#include <absl/strings/str_format.h>
#include <packager/file.h>
#include <packager/live_packager.h>
#include <packager/media/base/aes_decryptor.h>

#include "absl/strings/escaping.h"

namespace shaka {
namespace {

const uint8_t kKeyId[]{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};
const uint8_t kKey[]{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};
const uint8_t kIv[]{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
};

const double kSegmentDurationInSeconds = 5.0;
const int kNumSegments = 10;

std::filesystem::path GetTestDataFilePath(const std::string& name) {
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

uint8_t hex_char_to_int(const char& c) {
  unsigned result = 0;
  if (c >= '0' && c <= '9') {
    result = c - '0';
  } else if (c >= 'A' && c <= 'F') {
    result = c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    result = c - 'a' + 10;
  } else {
    throw std::out_of_range("input character is out of hex range");
  }

  return result;
}

std::vector<uint8_t> unhex(const std::string& in) {
  std::vector<uint8_t> out;
  for (std::size_t i = 1; i < in.size(); i+=2) {
      out.push_back(16 * hex_char_to_int(in[i-1]) +hex_char_to_int(in[i]));
  }

  return out;
}

std::vector<uint8_t> unbase64(const std::string& base64_string) {
  std::string str;
  std::vector<uint8_t> bytes;
  if (!absl::Base64Unescape(base64_string, &str)) {
    return {};
  }

  bytes.assign(str.begin(), str.end());
  return bytes;
}

}  // namespace

TEST(GeneratePSSHData, GeneratesPSSHBoxesAndMSPRObject) {
  PSSHGeneratorInput in{
      .encryption_scheme = EncryptionSchemeFourCC::CENC,
      .key_id = unhex("00000000621f2afe7ab2c868d5fd2e2e"),
      .key = unhex("1af987fa084ff3c0f4ad35a6bdab98e2"),
      .key_ids = {
        unhex("00000000621f2afe7ab2c868d5fd2e2e"),
        unhex("00000000621f2afe7ab2c868d5fd2e2f")
      }
  };

  PSSHData expected{
    .cenc_box = unbase64("AAAARHBzc2gBAAAAEHfv7MCyTQKs4zweUuL7SwAAAAIAAAAAYh8q/nqyyGjV/S4uAAAAAGIfKv56ssho1f0uLwAAAAA="),
    .mspr_box = unbase64("AAACJnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAAgYGAgAAAQABAPwBPABXAFIATQBIAEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0AYQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAvADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkAbwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQAPgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBOAEYATwA+ADwASwBJAEQAPgBBAEEAQQBBAEEAQgA5AGkALwBpAHAANgBzAHMAaABvADEAZgAwAHUATABnAD0APQA8AC8ASwBJAEQAPgA8AEMASABFAEMASwBTAFUATQA+ADQAZgB1AEIAdABEAFUAKwBLAGsARQA9ADwALwBDAEgARQBDAEsAUwBVAE0APgA8AC8ARABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA=="),
    .mspr_pro = unbase64("BgIAAAEAAQD8ATwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0ALwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBkAGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEAVABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2ADwALwBLAEUAWQBMAEUATgA+ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsASQBEAD4AQQBBAEEAQQBBAEIAOQBpAC8AaQBwADYAcwBzAGgAbwAxAGYAMAB1AEwAZwA9AD0APAAvAEsASQBEAD4APABDAEgARQBDAEsAUwBVAE0APgA0AGYAdQBCAHQARABVACsASwBrAEUAPQA8AC8AQwBIAEUAQwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA="),
    .wv_box = unbase64("AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoSEAAAAABiHyr+erLIaNX9Li4SEAAAAABiHyr+erLIaNX9Li9I49yVmwY=")
  };
  PSSHData actual {};

  ASSERT_EQ(Status::OK, GeneratePSSHData(in, &actual));
  ASSERT_EQ(expected.cenc_box, actual.cenc_box);
  ASSERT_EQ(expected.mspr_box, actual.mspr_box);
  ASSERT_EQ(expected.mspr_pro, actual.mspr_pro);
  ASSERT_EQ(expected.wv_box, actual.wv_box);
}

TEST(GeneratePSSHData, FailsOnInvalidInput) {
  const PSSHGeneratorInput valid_input{
    .encryption_scheme = EncryptionSchemeFourCC::CENC,
    .key_id = unhex("00000000621f2afe7ab2c868d5fd2e2e"),
    .key = unhex("1af987fa084ff3c0f4ad35a6bdab98e2"),
    .key_ids = {
      unhex("00000000621f2afe7ab2c868d5fd2e2e"),
      unhex("00000000621f2afe7ab2c868d5fd2e2f")
    }
  };

  PSSHGeneratorInput in;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "invalid encryption scheme in PSSH generator input"), GeneratePSSHData(in, nullptr));

  in.encryption_scheme = valid_input.encryption_scheme;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "invalid key lenght in PSSH generator input"), GeneratePSSHData(in, nullptr));

  in.key = valid_input.key;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "invalid key id lenght in PSSH generator input"), GeneratePSSHData(in, nullptr));

  in.key_id = valid_input.key_id;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "key ids cannot be empty in PSSH generator input"), GeneratePSSHData(in, nullptr));

  in.key_ids = valid_input.key_ids;
  in.key_ids[1] = {};
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "invalid key id lenght in key ids array in PSSH generator input, index 1"), GeneratePSSHData(in, nullptr));

  in.key_ids = valid_input.key_ids;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "output data cannot be null"), GeneratePSSHData(in, nullptr));
}

class LivePackagerBaseTest : public ::testing::Test {
 public:
  void SetUp() override {
    key_.assign(kKey, kKey + std::size(kKey));
    iv_.assign(kIv, kIv + std::size(kIv));
    key_id_.assign(kKeyId, kKeyId + std::size(kKeyId));
    SetupLivePackagerConfig(LiveConfig());
  }

  void SetupLivePackagerConfig(const LiveConfig& config) {
    LiveConfig new_live_config = config;
    new_live_config.segment_duration_sec = kSegmentDurationInSeconds;
    switch (new_live_config.protection_scheme) {
      case LiveConfig::EncryptionScheme::NONE:
        break;
      case LiveConfig::EncryptionScheme::SAMPLE_AES:
      case LiveConfig::EncryptionScheme::AES_128:
        new_live_config.key = key_;
        new_live_config.iv = iv_;
        new_live_config.key_id = key_id_;
        break;
    }
    live_packager_ = std::make_unique<LivePackager>(new_live_config);
  }

 protected:
  std::unique_ptr<LivePackager> live_packager_;

  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> key_id_;
};

TEST_F(LivePackagerBaseTest, InitSegmentOnly) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  FullSegmentBuffer in;
  in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());

  FullSegmentBuffer out;

  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  SetupLivePackagerConfig(live_config);

  ASSERT_EQ(Status::OK, live_packager_->PackageInit(in, out));
  ASSERT_GT(out.InitSegmentSize(), 0);
  ASSERT_EQ(out.SegmentSize(), 0);
}

TEST_F(LivePackagerBaseTest, VerifyAes128WithDecryption) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  media::AesCbcDecryptor decryptor(media::kPkcs5Padding,
                                   media::AesCryptor::kUseConstantIv);

  ASSERT_TRUE(decryptor.InitializeWithIv(key_, iv_));

  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    FullSegmentBuffer in;
    in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());
    in.AppendData(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status::OK, live_packager_->Package(in, out));
    ASSERT_GT(out.SegmentSize(), 0);

    std::string exp_segment_num = absl::StrFormat("expected/ts/%04d.ts", i + 1);
    std::vector<uint8_t> exp_segment_buffer = ReadTestDataFile(exp_segment_num);
    ASSERT_FALSE(exp_segment_buffer.empty());

    std::vector<uint8_t> decrypted;
    std::vector<uint8_t> buffer(out.SegmentData(),
                                out.SegmentData() + out.SegmentSize());

    ASSERT_TRUE(decryptor.Crypt(buffer, &decrypted));
    ASSERT_EQ(decrypted, exp_segment_buffer);
  }
}

TEST_F(LivePackagerBaseTest, EncryptionFailure) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  // Invalid key and iv sizes to trigger an encryption error
  key_ = std::vector<uint8_t>(15, 0);
  iv_ = std::vector<uint8_t>(14, 0);

  for (unsigned int i = 0; i < 1; i++) {
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    FullSegmentBuffer in;
    in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());
    in.AppendData(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                     "invalid key and IV supplied to encryptor"),
              live_packager_->Package(in, out));
  }
}

struct LivePackagerTestCase {
  unsigned int num_segments;
  std::string init_segment_name;
  LiveConfig::EncryptionScheme encryption_scheme;
  LiveConfig::OutputFormat output_format;
  LiveConfig::TrackType track_type;
  const char* media_segment_format;
};

class LivePackagerEncryptionTest
    : public LivePackagerBaseTest,
      public ::testing::WithParamInterface<LivePackagerTestCase> {
 public:
  void SetUp() override {
    LivePackagerBaseTest::SetUp();

    LiveConfig live_config;
    live_config.format = GetParam().output_format;
    live_config.track_type = GetParam().track_type;
    live_config.protection_scheme = GetParam().encryption_scheme;
    SetupLivePackagerConfig(live_config);
  }
};

TEST_P(LivePackagerEncryptionTest, VerifyWithEncryption) {
  std::vector<uint8_t> init_segment_buffer =
      ReadTestDataFile(GetParam().init_segment_name);
  ASSERT_FALSE(init_segment_buffer.empty());

  for (unsigned int i = 0; i < GetParam().num_segments; i++) {
    std::string format_output;

    std::vector<absl::FormatArg> format_args;
    format_args.emplace_back(i);
    absl::UntypedFormatSpec format(GetParam().media_segment_format);

    ASSERT_TRUE(absl::FormatUntyped(&format_output, format, format_args));
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(format_output);
    ASSERT_FALSE(segment_buffer.empty());

    FullSegmentBuffer in;
    in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());
    in.AppendData(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    ASSERT_EQ(Status::OK, live_packager_->Package(in, out));
    ASSERT_GT(out.SegmentSize(), 0);
  }
}

INSTANTIATE_TEST_CASE_P(
    LivePackagerEncryptionTypes,
    LivePackagerEncryptionTest,
    ::testing::Values(
        // Verify FMP4 to TS with Sample Aes encryption.
        LivePackagerTestCase{10, "input/init.mp4",
                             LiveConfig::EncryptionScheme::SAMPLE_AES,
                             LiveConfig::OutputFormat::TS,
                             LiveConfig::TrackType::VIDEO, "input/%04d.m4s"},
        // Verify FMP4 to FMP4 with Sample AES encryption.
        LivePackagerTestCase{10, "input/init.mp4",
                             LiveConfig::EncryptionScheme::SAMPLE_AES,
                             LiveConfig::OutputFormat::FMP4,
                             LiveConfig::TrackType::VIDEO, "input/%04d.m4s"},
        // Verify FMP4 to TS with AES-128 encryption.
        LivePackagerTestCase{10, "input/init.mp4",
                             LiveConfig::EncryptionScheme::AES_128,
                             LiveConfig::OutputFormat::TS,
                             LiveConfig::TrackType::VIDEO, "input/%04d.m4s"},
        // Verify AUDIO segments only to TS with Sample AES encryption.
        LivePackagerTestCase{
            5, "audio/en/init.mp4", LiveConfig::EncryptionScheme::SAMPLE_AES,
            LiveConfig::OutputFormat::TS, LiveConfig::TrackType::AUDIO,
            "audio/en/%05d.m4s"}));
}  // namespace shaka
