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
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/media/formats/mp4/box_reader.h>

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

bool ParseAndCheckType(media::mp4::Box& box, media::mp4::BoxReader* reader) {
  box.Parse(reader);
  return box.BoxType() == reader->type();
}

struct SegmentIndexBoxChecker {
  SegmentIndexBoxChecker(media::mp4::SegmentIndex box)
      : sidx_(std::move(box)) {}

  void Check(media::mp4::BoxReader* reader) {
    media::mp4::SegmentIndex box;
    CHECK(ParseAndCheckType(box, reader));
    EXPECT_EQ(sidx_.timescale, box.timescale);
  }

 private:
  media::mp4::SegmentIndex sidx_;
};

struct MovieFragmentBoxChecker {
  MovieFragmentBoxChecker(media::mp4::MovieFragment box)
      : moof_(std::move(box)) {}

  void Check(media::mp4::BoxReader* reader) {
    media::mp4::MovieFragment box;
    CHECK(ParseAndCheckType(box, reader));
    EXPECT_EQ(moof_.header.sequence_number, box.header.sequence_number);
  }

 private:
  media::mp4::MovieFragment moof_;
};

struct SegmentTypeBoxChecker {
  void Check(media::mp4::BoxReader* reader) {
    media::mp4::SegmentType box;
    CHECK(ParseAndCheckType(box, reader));
    EXPECT_EQ(media::FourCC::FOURCC_mp41, box.major_brand);
  }
};

struct FileTypeBoxChecker {
  void Check(media::mp4::BoxReader* reader) {
    media::mp4::FileType box;
    CHECK(ParseAndCheckType(box, reader));
    EXPECT_EQ(media::FourCC::FOURCC_mp41, box.major_brand);
  }
};

struct MovieBoxChecker {
  MovieBoxChecker(media::mp4::Movie movie) : moov_(std::move(movie)) {}

  void Check(media::mp4::BoxReader* reader) {
    media::mp4::Movie moov;
    CHECK(ParseAndCheckType(moov, reader));

    EXPECT_EQ(moov_.tracks.size(), moov.tracks.size());

    for (unsigned i(0); i < moov_.tracks.size(); ++i) {
      const auto& exp_track = moov_.tracks[i];
      const auto& act_track = moov.tracks[i];

      EXPECT_EQ(exp_track.media.handler.handler_type,
                act_track.media.handler.handler_type);

      const auto& exp_video_entries =
          exp_track.media.information.sample_table.description.video_entries;
      const auto& act_video_entries =
          act_track.media.information.sample_table.description.video_entries;

      EXPECT_EQ(exp_video_entries.size(), act_video_entries.size());

      for (unsigned j(0); j < exp_video_entries.size(); ++j) {
        const auto& exp_entry = exp_video_entries[j];
        const auto& act_entry = act_video_entries[j];

        EXPECT_EQ(exp_entry.BoxType(), act_entry.BoxType());
        EXPECT_EQ(exp_entry.width, act_entry.width);
        EXPECT_EQ(exp_entry.height, act_entry.height);
      }
    }
  }

 private:
  media::mp4::Movie moov_;
};

void CheckVideoInitSegment(const FullSegmentBuffer& buffer) {
  bool err(true);
  size_t bytes_to_read(buffer.InitSegmentSize());
  const uint8_t* data(buffer.InitSegmentData());

  {
    std::unique_ptr<media::mp4::BoxReader> reader(
        media::mp4::BoxReader::ReadBox(data, bytes_to_read, &err));
    EXPECT_FALSE(err);

    FileTypeBoxChecker checker;
    checker.Check(reader.get());

    data += reader->size();
    bytes_to_read -= reader->size();
  }

  {
    std::unique_ptr<media::mp4::BoxReader> reader(
        media::mp4::BoxReader::ReadBox(data, bytes_to_read, &err));
    EXPECT_FALSE(err);

    media::mp4::VideoSampleEntry entry;
    entry.format = media::FOURCC_avc1;
    entry.width = 1024;
    entry.height = 576;

    media::mp4::Track track;
    track.media.handler.handler_type = media::FourCC::FOURCC_vide;
    track.media.information.sample_table.description.video_entries.push_back(
        entry);

    media::mp4::Movie expected;
    expected.tracks.push_back(track);

    MovieBoxChecker checker(expected);
    checker.Check(reader.get());
  }
}

void CheckSegment(const LiveConfig& config, const FullSegmentBuffer& buffer) {
  bool err(true);
  size_t bytes_to_read(buffer.SegmentSize());
  const uint8_t* data(buffer.SegmentData());

  {
    std::unique_ptr<media::mp4::BoxReader> reader(
        media::mp4::BoxReader::ReadBox(data, bytes_to_read, &err));
    EXPECT_FALSE(err);

    SegmentTypeBoxChecker checker;
    checker.Check(reader.get());

    data += reader->size();
    bytes_to_read -= reader->size();
  }

  {
    std::unique_ptr<media::mp4::BoxReader> reader(
        media::mp4::BoxReader::ReadBox(data, bytes_to_read, &err));
    EXPECT_FALSE(err);

    media::mp4::SegmentIndex expected;
    expected.timescale = 10000000;
    SegmentIndexBoxChecker checker(expected);
    checker.Check(reader.get());

    data += reader->size();
    bytes_to_read -= reader->size();
  }

  {
    std::unique_ptr<media::mp4::BoxReader> reader(
        media::mp4::BoxReader::ReadBox(data, bytes_to_read, &err));
    EXPECT_FALSE(err);

    media::mp4::MovieFragment expected;
    expected.header.sequence_number = config.segment_number;

    MovieFragmentBoxChecker checker(expected);
    checker.Check(reader.get());
  }
}

}  // namespace

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

  CheckVideoInitSegment(out);
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

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
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

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                     "invalid key and IV supplied to encryptor"),
              live_packager_->Package(init_seg, media_seg, out));
  }
}

TEST_F(LivePackagerBaseTest, CustomMoofSequenceNumber) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());
  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  live_config.protection_scheme = LiveConfig::EncryptionScheme::NONE;
  live_config.segment_duration_sec = kSegmentDurationInSeconds;

  for (unsigned int i = 0; i < kNumSegments; i++) {
    live_config.segment_number = i + 1;
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;
    LivePackager packager(live_config);

    ASSERT_EQ(Status::OK, packager.Package(init_seg, media_seg, out));
    ASSERT_GT(out.SegmentSize(), 0);

    CheckSegment(live_config, out);
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

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    FullSegmentBuffer out;

    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
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
