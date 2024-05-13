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
#include <gmock/gmock-matchers.h>
#include <packager/crypto_params.h>
#include <packager/file.h>
#include <packager/live_packager.h>
#include <packager/media/base/aes_decryptor.h>
#include <packager/media/base/aes_pattern_cryptor.h>
#include <packager/media/base/byte_queue.h>
#include <packager/media/base/key_source.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/raw_key_source.h>
#include <packager/media/base/stream_info.h>
#include <packager/media/formats/mp2t/mp2t_media_parser.h>
#include <packager/media/formats/mp2t/program_map_table_writer.h>
#include <packager/media/formats/mp2t/ts_packet.h>
#include <packager/media/formats/mp2t/ts_section.h>
#include <packager/media/formats/mp4/box_definitions.h>
#include <packager/media/formats/mp4/box_definitions_comparison.h>
#include <packager/media/formats/mp4/box_reader.h>
#include <packager/media/formats/mp4/mp4_media_parser.h>

#include "absl/strings/escaping.h"
#include "packager/media/base/protection_system_ids.h"

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

const char kKeyIdHex[] = "00000000621f2afe7ab2c868d5fd2e2e";
const char kKeyHex[] = "1af987fa084ff3c0f4ad35a6bdab98e2";

std::vector<uint8_t> HexStringToVector(const std::string& hex_str) {
  std::string raw_str = absl::HexStringToBytes(hex_str);
  return std::vector<uint8_t>(raw_str.begin(), raw_str.end());
}

const int kNumSegments = 10;
constexpr unsigned kSegmentDurationMs = 5000;

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

std::vector<uint8_t> unhex(const std::string& in) {
  auto converted = absl::HexStringToBytes(in);
  return {converted.begin(), converted.end()};
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

bool ParseAndCheckType(media::mp4::Box& box, media::mp4::BoxReader* reader) {
  box.Parse(reader);
  return box.BoxType() == reader->type();
}

bool FormatWithIndex(const char* fmt, int i, std::string& out) {
  std::vector<absl::FormatArg> format_args;
  format_args.emplace_back(i);
  absl::UntypedFormatSpec format(fmt);

  return absl::FormatUntyped(&out, format, format_args);
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

    EXPECT_EQ(0, moov.pssh.size());

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

      const auto& exp_text_entries =
          exp_track.media.information.sample_table.description.text_entries;
      const auto& act_text_entries =
          act_track.media.information.sample_table.description.text_entries;

      EXPECT_EQ(exp_text_entries.size(), act_text_entries.size());
      for (unsigned j(0); j < exp_text_entries.size(); ++j) {
        const auto& exp_entry = exp_text_entries[j];
        const auto& act_entry = act_text_entries[j];

        EXPECT_EQ(exp_entry.BoxType(), act_entry.BoxType());
      }
    }
  }

 private:
  media::mp4::Movie moov_;
};

class MP4MediaParserTest {
 public:
  MP4MediaParserTest(media::KeySource* key_source) {
    InitializeParser(key_source);
  }

  const std::vector<std::shared_ptr<media::MediaSample>>& GetSamples() {
    return samples_;
  }

  const std::vector<std::shared_ptr<media::mp4::DASHEventMessageBox>>&
  GetEmsgSamples() {
    return emsg_samples_;
  }

  bool Parse(const uint8_t* buf, size_t len) {
    // Use a memoryfile so we can read inputs directly without going to disk
    const std::string input_fname = "memory://file1";
    shaka::File* writer(shaka::File::Open(input_fname.c_str(), "w"));
    writer->Write(buf, len);
    writer->Close();

    if (!parser_->LoadMoov(input_fname)) {
      return false;
    }

    return AppendDataInPieces(buf, len);
  }

 protected:
  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, static_cast<int>(length));
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size = 512) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size =
          std::min(piece_size, static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void InitF(const std::vector<std::shared_ptr<media::StreamInfo>>& streams) {}

  bool NewSampleF(uint32_t track_id,
                  std::shared_ptr<media::MediaSample> sample) {
    samples_.push_back(std::move(sample));
    return true;
  }

  bool NewTextSampleF(uint32_t track_id,
                      std::shared_ptr<media::TextSample> sample) {
    return false;
  }

  void InitializeParser(media::KeySource* decryption_key_source) {
    parser_->Init(
        std::bind(&MP4MediaParserTest::InitF, this, std::placeholders::_1),
        std::bind(&MP4MediaParserTest::NewSampleF, this, std::placeholders::_1,
                  std::placeholders::_2),
        std::bind(&MP4MediaParserTest::NewTextSampleF, this,
                  std::placeholders::_1, std::placeholders::_2),
        decryption_key_source);

    parser_->SetEventMessageBoxCB(
        [this](std::shared_ptr<media::mp4::DASHEventMessageBox> info) {
          emsg_samples_.push_back(std::move(info));
          return true;
        });
  }

  std::unique_ptr<media::mp4::MP4MediaParser> parser_ =
      std::make_unique<media::mp4::MP4MediaParser>();
  std::vector<std::shared_ptr<media::MediaSample>> samples_;
  std::vector<std::shared_ptr<media::mp4::DASHEventMessageBox>> emsg_samples_;
};

bool GetBox(const Segment& buffer, media::mp4::Box& out) {
  bool err(true);
  size_t bytes_to_read(buffer.Size());
  const uint8_t* data(buffer.Data());

  while (bytes_to_read > 0) {
    std::unique_ptr<media::mp4::BoxReader> reader(
        media::mp4::BoxReader::ReadBox(data, bytes_to_read, &err));

    if (err) {
      return false;
    }

    if (reader->type() == out.BoxType()) {
      out.Parse(reader.get());
      return true;
    }

    data += reader->size();
    bytes_to_read -= reader->size();
  }

  return false;
}

void CheckVideoInitSegment(const SegmentBuffer& buffer, media::FourCC format) {
  bool err(true);
  size_t bytes_to_read(buffer.Size());
  const uint8_t* data(buffer.Data());

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
    entry.format = format;
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

void CheckVideoPsshInfo(const ProtectionSystem& config,
                        const SegmentBuffer& buffer) {
  bool err(true);
  size_t bytes_to_read(buffer.Size());
  const uint8_t* data(buffer.Data());

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

    media::mp4::Movie moov;
    CHECK(ParseAndCheckType(moov, reader.get()));

    std::vector<std::vector<uint8_t>> expected_pssh_system_ids;
    if (has_flag(config, ProtectionSystem::kCommon)) {
      expected_pssh_system_ids.emplace_back(std::begin(media::kCommonSystemId),
                                            std::end(media::kCommonSystemId));
    }
    if (has_flag(config, ProtectionSystem::kPlayReady)) {
      expected_pssh_system_ids.emplace_back(
          std::begin(media::kPlayReadySystemId),
          std::end(media::kPlayReadySystemId));
    }
    if (has_flag(config, ProtectionSystem::kWidevine)) {
      expected_pssh_system_ids.emplace_back(
          std::begin(media::kWidevineSystemId),
          std::end(media::kWidevineSystemId));
    }

    std::vector<uint8_t> widevine_system_id(
        media::kWidevineSystemId,
        media::kWidevineSystemId + std::size(media::kWidevineSystemId));
    std::vector<uint8_t> playready_system_id(
        media::kPlayReadySystemId,
        media::kPlayReadySystemId + std::size(media::kPlayReadySystemId));

    std::vector<std::vector<uint8_t>> actual_pssh_system_ids;
    if (!moov.pssh.empty()) {
      for (const auto& pssh : moov.pssh) {
        auto pssh_builder = media::PsshBoxBuilder::ParseFromBox(
            pssh.raw_box.data(), pssh.raw_box.size());
        auto system_id = pssh_builder->system_id();
        actual_pssh_system_ids.emplace_back(system_id);
      }
    }

    ASSERT_THAT(actual_pssh_system_ids,
                ::testing::UnorderedElementsAreArray(expected_pssh_system_ids));
    ASSERT_EQ(actual_pssh_system_ids.size(), expected_pssh_system_ids.size());
  }
}

void CheckTextInitSegment(const FullSegmentBuffer& buffer,
                          media::FourCC handler,
                          media::FourCC format) {
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

    media::mp4::TextSampleEntry entry;
    entry.format = format;

    media::mp4::Track track;
    track.media.handler.handler_type = handler;
    track.media.information.sample_table.description.text_entries.push_back(
        entry);

    media::mp4::Movie expected;
    expected.tracks.push_back(track);

    MovieBoxChecker checker(expected);
    checker.Check(reader.get());
  }
}

void CheckSegment(const LiveConfig& config,
                  const SegmentBuffer& buffer,
                  const uint32_t expected_timescale,
                  const bool check_decode_time) {
  bool err(true);
  size_t bytes_to_read(buffer.Size());
  const uint8_t* data(buffer.Data());

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
    expected.timescale = expected_timescale;
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
    expected.tracks.resize(1);
    if (check_decode_time)
      expected.tracks[0].decode_time.decode_time =
          config.timed_text_decode_time;

    MovieFragmentBoxChecker checker(expected);
    checker.Check(reader.get());
  }
}

}  // namespace

TEST(GeneratePSSHData, GeneratesPSSHBoxesAndMSPRObject) {
  PSSHGeneratorInput in{
      .protection_scheme = PSSHGeneratorInput::MP4ProtectionSchemeFourCC::CENC,
      .key = unhex("1af987fa084ff3c0f4ad35a6bdab98e2"),
      .key_id = unhex("00000000621f2afe7ab2c868d5fd2e2e"),
      .key_ids = {unhex("00000000621f2afe7ab2c868d5fd2e2e"),
                  unhex("00000000621f2afe7ab2c868d5fd2e2f")}};

  PSSHData expected{
      .cenc_box = unbase64("AAAARHBzc2gBAAAAEHfv7MCyTQKs4zweUuL7SwAAAAIAAAAAYh8"
                           "q/nqyyGjV/S4uAAAAAGIfKv56ssho1f0uLwAAAAA="),
      .mspr_box = unbase64(
          "AAACJnBzc2gAAAAAmgTweZhAQoarkuZb4IhflQAAAgYGAgAAAQABAPwBPABXAFIATQBI"
          "AEUAQQBEAEUAUgAgAHgAbQBsAG4AcwA9ACIAaAB0AHQAcAA6AC8ALwBzAGMAaABlAG0A"
          "YQBzAC4AbQBpAGMAcgBvAHMAbwBmAHQALgBjAG8AbQAvAEQAUgBNAC8AMgAwADAANwAv"
          "ADAAMwAvAFAAbABhAHkAUgBlAGEAZAB5AEgAZQBhAGQAZQByACIAIAB2AGUAcgBzAGkA"
          "bwBuAD0AIgA0AC4AMAAuADAALgAwACIAPgA8AEQAQQBUAEEAPgA8AFAAUgBPAFQARQBD"
          "AFQASQBOAEYATwA+"
          "ADwASwBFAFkATABFAE4APgAxADYAPAAvAEsARQBZAEwARQBOAD4APABBAEwARwBJAEQA"
          "PgBBAEUAUwBDAFQAUgA8AC8AQQBMAEcASQBEAD4APAAvAFAAUgBPAFQARQBDAFQASQBO"
          "AEYATwA+"
          "ADwASwBJAEQAPgBBAEEAQQBBAEEAQgA5AGkALwBpAHAANgBzAHMAaABvADEAZgAwAHUA"
          "TABnAD0APQA8AC8ASwBJAEQAPgA8AEMASABFAEMASwBTAFUATQA+"
          "ADQAZgB1AEIAdABEAFUAKwBLAGsARQA9ADwALwBDAEgARQBDAEsAUwBVAE0APgA8AC8A"
          "RABBAFQAQQA+ADwALwBXAFIATQBIAEUAQQBEAEUAUgA+AA=="),
      .mspr_pro = unbase64(
          "BgIAAAEAAQD8ATwAVwBSAE0ASABFAEEARABFAFIAIAB4AG0AbABuAHMAPQAiAGgAdAB0"
          "AHAAOgAvAC8AcwBjAGgAZQBtAGEAcwAuAG0AaQBjAHIAbwBzAG8AZgB0AC4AYwBvAG0A"
          "LwBEAFIATQAvADIAMAAwADcALwAwADMALwBQAGwAYQB5AFIAZQBhAGQAeQBIAGUAYQBk"
          "AGUAcgAiACAAdgBlAHIAcwBpAG8AbgA9ACIANAAuADAALgAwAC4AMAAiAD4APABEAEEA"
          "VABBAD4APABQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsARQBZAEwARQBOAD4AMQA2"
          "ADwALwBLAEUAWQBMAEUATgA+"
          "ADwAQQBMAEcASQBEAD4AQQBFAFMAQwBUAFIAPAAvAEEATABHAEkARAA+"
          "ADwALwBQAFIATwBUAEUAQwBUAEkATgBGAE8APgA8AEsASQBEAD4AQQBBAEEAQQBBAEIA"
          "OQBpAC8AaQBwADYAcwBzAGgAbwAxAGYAMAB1AEwAZwA9AD0APAAvAEsASQBEAD4APABD"
          "AEgARQBDAEsAUwBVAE0APgA0AGYAdQBCAHQARABVACsASwBrAEUAPQA8AC8AQwBIAEUA"
          "QwBLAFMAVQBNAD4APAAvAEQAQQBUAEEAPgA8AC8AVwBSAE0ASABFAEEARABFAFIAPgA"
          "="),
      .wv_box =
          unbase64("AAAASnBzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAACoSEAAAAABiHyr+"
                   "erLIaNX9Li4SEAAAAABiHyr+erLIaNX9Li9I49yVmwY=")};
  PSSHData actual{};

  ASSERT_EQ(Status::OK, GeneratePSSHData(in, &actual));
  ASSERT_EQ(expected.cenc_box, actual.cenc_box);
  ASSERT_EQ(expected.mspr_box, actual.mspr_box);
  ASSERT_EQ(expected.mspr_pro, actual.mspr_pro);
  ASSERT_EQ(expected.wv_box, actual.wv_box);
}

TEST(GeneratePSSHData, FailsOnInvalidInput) {
  const PSSHGeneratorInput valid_input{
      .protection_scheme = PSSHGeneratorInput::MP4ProtectionSchemeFourCC::CENC,
      .key = unhex("1af987fa084ff3c0f4ad35a6bdab98e2"),
      .key_id = unhex("00000000621f2afe7ab2c868d5fd2e2e"),
      .key_ids = {unhex("00000000621f2afe7ab2c868d5fd2e2e"),
                  unhex("00000000621f2afe7ab2c868d5fd2e2f")}};

  PSSHGeneratorInput in;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                   "invalid encryption scheme in PSSH generator input"),
            GeneratePSSHData(in, nullptr));

  in.protection_scheme = valid_input.protection_scheme;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                   "invalid key length in PSSH generator input"),
            GeneratePSSHData(in, nullptr));

  in.key = valid_input.key;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                   "invalid key id length in PSSH generator input"),
            GeneratePSSHData(in, nullptr));

  in.key_id = valid_input.key_id;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                   "key ids cannot be empty in PSSH generator input"),
            GeneratePSSHData(in, nullptr));

  in.key_ids = valid_input.key_ids;
  in.key_ids[1] = {};
  ASSERT_EQ(Status(error::INVALID_ARGUMENT,
                   "invalid key id length in key ids array in PSSH generator "
                   "input, index 1"),
            GeneratePSSHData(in, nullptr));

  in.key_ids = valid_input.key_ids;
  ASSERT_EQ(Status(error::INVALID_ARGUMENT, "output data cannot be null"),
            GeneratePSSHData(in, nullptr));
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
    switch (new_live_config.protection_scheme) {
      case LiveConfig::EncryptionScheme::NONE:
        break;
      case LiveConfig::EncryptionScheme::SAMPLE_AES:
      case LiveConfig::EncryptionScheme::AES_128:
      case LiveConfig::EncryptionScheme::CBCS:
      case LiveConfig::EncryptionScheme::CENC:
        new_live_config.key = key_;
        new_live_config.iv = iv_;
        new_live_config.key_id = key_id_;
        break;
    }
    new_live_config.m2ts_offset_ms = 9000;
    live_packager_ = std::make_unique<LivePackager>(new_live_config);
  }

 protected:
  std::unique_ptr<LivePackager> live_packager_;

  std::vector<uint8_t> key_;
  std::vector<uint8_t> iv_;
  std::vector<uint8_t> key_id_;
};

class LivePackagerMp2tTest : public LivePackagerBaseTest {
 public:
  LivePackagerMp2tTest() : LivePackagerBaseTest() {}

  void SetUp() override {
    LivePackagerBaseTest::SetUp();
    parser_.reset(new media::mp2t::Mp2tMediaParser());
    InitializeParser();
  }

 protected:
  typedef std::map<int, std::shared_ptr<media::StreamInfo>> StreamMap;

  std::unique_ptr<media::mp2t::Mp2tMediaParser> parser_;
  StreamMap stream_map_;

  bool AppendData(const uint8_t* data, size_t length) {
    return parser_->Parse(data, static_cast<int>(length));
  }

  bool AppendDataInPieces(const uint8_t* data,
                          size_t length,
                          size_t piece_size) {
    const uint8_t* start = data;
    const uint8_t* end = data + length;
    while (start < end) {
      size_t append_size =
          std::min(piece_size, static_cast<size_t>(end - start));
      if (!AppendData(start, append_size))
        return false;
      start += append_size;
    }
    return true;
  }

  void OnInit(
      const std::vector<std::shared_ptr<media::StreamInfo>>& stream_infos) {
    for (const auto& stream_info : stream_infos) {
      stream_map_[stream_info->track_id()] = stream_info;
    }
  }

  bool OnNewSample(uint32_t track_id,
                   std::shared_ptr<media::MediaSample> sample) {
    StreamMap::const_iterator stream = stream_map_.find(track_id);
    EXPECT_NE(stream_map_.end(), stream);
    if (stream != stream_map_.end()) {
      if (stream->second->stream_type() == media::kStreamVideo) {
        EXPECT_GE(sample->pts(), sample->dts());
      }
    }
    return true;
  }

  bool OnNewTextSample(uint32_t track_id,
                       std::shared_ptr<media::TextSample> sample) {
    return false;
  }

  void InitializeParser() {
    parser_->Init(
        std::bind(&LivePackagerMp2tTest::OnInit, this, std::placeholders::_1),
        std::bind(&LivePackagerMp2tTest::OnNewSample, this,
                  std::placeholders::_1, std::placeholders::_2),
        std::bind(&LivePackagerMp2tTest::OnNewTextSample, this,
                  std::placeholders::_1, std::placeholders::_2),
        NULL);
  }
};

TEST_F(LivePackagerBaseTest, InitSegmentOnly) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  FullSegmentBuffer in;
  in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());

  SegmentBuffer out;

  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  SetupLivePackagerConfig(live_config);

  ASSERT_EQ(Status::OK, live_packager_->PackageInit(in, out));
  ASSERT_GT(out.Size(), 0);

  CheckVideoInitSegment(out, media::FourCC::FOURCC_avc1);
}

TEST_F(LivePackagerBaseTest, InitSegmentOnlyWithCBCS) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  FullSegmentBuffer in;
  in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());

  SegmentBuffer out;

  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  live_config.protection_scheme = LiveConfig::EncryptionScheme::CBCS;
  SetupLivePackagerConfig(live_config);

  ASSERT_EQ(Status::OK, live_packager_->PackageInit(in, out));
  ASSERT_GT(out.Size(), 0);

  CheckVideoInitSegment(out, media::FourCC::FOURCC_encv);
}

TEST_F(LivePackagerBaseTest, InitSegmentOnlyWithCENC) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  FullSegmentBuffer in;
  in.SetInitSegment(init_segment_buffer.data(), init_segment_buffer.size());

  SegmentBuffer out;

  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  live_config.protection_scheme = LiveConfig::EncryptionScheme::CENC;
  SetupLivePackagerConfig(live_config);

  ASSERT_EQ(Status::OK, live_packager_->PackageInit(in, out));
  ASSERT_GT(out.Size(), 0);

  CheckVideoInitSegment(out, media::FourCC::FOURCC_encv);
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

    SegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;
    live_config.segment_number = i;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);

    std::string exp_segment_num =
        absl::StrFormat("expected/stuffing_ts/%04d.ts", i + 1);
    std::vector<uint8_t> exp_segment_buffer = ReadTestDataFile(exp_segment_num);
    ASSERT_FALSE(exp_segment_buffer.empty());

    std::vector<uint8_t> decrypted;
    std::vector<uint8_t> buffer(out.Data(), out.Data() + out.Size());

    ASSERT_TRUE(decryptor.Crypt(buffer, &decrypted));
    ASSERT_EQ(decrypted, exp_segment_buffer);
  }
}

// Test demonstrates decrypting fmp4 source with one set of keys and
// re-encrypting using a difference encryption schema (AES-128) and different
// set of keys. As validation the re-encryption segments are decrypted and
// compared against decrypted segments.
TEST_F(LivePackagerBaseTest, VerifyPrdDecryptReEncrypt) {
  std::vector<uint8_t> init_segment_buffer =
      ReadTestDataFile("encrypted/prd_data/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  media::AesCbcDecryptor decryptor(media::kPkcs5Padding,
                                   media::AesCryptor::kUseConstantIv);
  ASSERT_TRUE(decryptor.InitializeWithIv(key_, iv_));

  for (unsigned int i = 1; i <= 7; i++) {
    std::string segment_num = absl::StrFormat("encrypted/prd_data/%05d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::AES_128;
    live_config.segment_number = i;
    live_config.decryption_key = HexStringToVector(kKeyHex);
    live_config.decryption_key_id = HexStringToVector(kKeyIdHex);

    SetupLivePackagerConfig(live_config);

    SegmentBuffer out;
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);

    std::string exp_decrypted_segment =
        absl::StrFormat("encrypted/prd_data/decrypt/ts/%04d.ts", i);
    std::vector<uint8_t> exp_decrypted_segment_buffer =
        ReadTestDataFile(exp_decrypted_segment);
    ASSERT_FALSE(exp_decrypted_segment_buffer.empty());

    std::vector<uint8_t> decrypted;
    std::vector<uint8_t> buffer(out.Data(), out.Data() + out.Size());

    ASSERT_TRUE(decryptor.Crypt(buffer, &decrypted));
    ASSERT_EQ(decrypted, exp_decrypted_segment_buffer);
  }
}

TEST_F(LivePackagerBaseTest, EditListAfterRepackage) {
  std::vector<uint8_t> init_segment_buffer =
      ReadTestDataFile("encrypted/prd_data/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  live_config.protection_scheme = LiveConfig::EncryptionScheme::CENC;
  live_config.decryption_key = HexStringToVector(kKeyHex);
  live_config.decryption_key_id = HexStringToVector(kKeyIdHex);
  SetupLivePackagerConfig(live_config);

  SegmentData init_seg(init_segment_buffer.data(), init_segment_buffer.size());
  SegmentBuffer actual_buf;
  const auto status = live_packager_->PackageInit(init_seg, actual_buf);
  ASSERT_EQ(Status::OK, status);
  ASSERT_GT(actual_buf.Size(), 0);

  media::mp4::Movie exp_moov;
  ASSERT_TRUE(GetBox(init_seg, exp_moov));
  media::mp4::Movie act_moov;
  ASSERT_TRUE(GetBox(actual_buf, act_moov));

  ASSERT_EQ(exp_moov.tracks.size(), act_moov.tracks.size());
  for (size_t i(0); i < exp_moov.tracks.size(); ++i) {
    const auto& exp_track = exp_moov.tracks[i];
    const auto& act_track = act_moov.tracks[i];
    EXPECT_EQ(exp_track.edit.list.edits, act_track.edit.list.edits);
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

    SegmentBuffer out;

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

TEST_F(LivePackagerBaseTest, CheckContinutityCounter) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  media::ByteQueue ts_byte_queue;
  int continuity_counter_tracker = 0;

  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    SegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::NONE;
    live_config.segment_number = i;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);

    ts_byte_queue.Push(out.Data(), static_cast<int>(out.Size()));
    while (true) {
      const uint8_t* ts_buffer;
      int ts_buffer_size;
      ts_byte_queue.Peek(&ts_buffer, &ts_buffer_size);

      if (ts_buffer_size < media::mp2t::TsPacket::kPacketSize)
        break;

      // Synchronization.
      int skipped_bytes =
          media::mp2t::TsPacket::Sync(ts_buffer, ts_buffer_size);
      ASSERT_EQ(skipped_bytes, 0);

      // Parse the TS header, skipping 1 byte if the header is invalid.
      std::unique_ptr<media::mp2t::TsPacket> ts_packet(
          media::mp2t::TsPacket::Parse(ts_buffer, ts_buffer_size));
      ASSERT_NE(nullptr, ts_packet);

      if (ts_packet->payload_unit_start_indicator() &&
          (ts_packet->pid() == media::mp2t::TsSection::kPidPat ||
           ts_packet->pid() == media::mp2t::ProgramMapTableWriter::kPmtPid)) {
        LOG(INFO) << "Processing PID=" << ts_packet->pid()
                  << " start_unit=" << ts_packet->payload_unit_start_indicator()
                  << " continuity_counter=" << ts_packet->continuity_counter();
        // check the PAT (PID = 0x0) or PMT (PID = 0x20) continuity counter is
        // in sync with the segment number.
        EXPECT_EQ(ts_packet->continuity_counter(), live_config.segment_number);
      } else if (ts_packet->pid() == 0x80) {
        // check PES TS Packets CC correctly increments.
        int expected_continuity_counter = (continuity_counter_tracker++) % 16;
        EXPECT_EQ(ts_packet->continuity_counter(), expected_continuity_counter);
      }
      // Go to the next packet.
      ts_byte_queue.Pop(media::mp2t::TsPacket::kPacketSize);
    }
    continuity_counter_tracker = 0;
    ts_byte_queue.Reset();
  }
}

TEST_F(LivePackagerMp2tTest, Mp2TSNegativeCTS) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());

  FullSegmentBuffer actual_buf;

  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    SegmentBuffer out;

    LiveConfig live_config;
    live_config.format = LiveConfig::OutputFormat::TS;
    live_config.track_type = LiveConfig::TrackType::VIDEO;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::NONE;
    live_config.segment_number = i;

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);
    actual_buf.AppendData(out.Data(), out.Size());
  }

  ASSERT_TRUE(
      AppendDataInPieces(actual_buf.Data(), actual_buf.SegmentSize(), 512));
  EXPECT_TRUE(parser_->Flush());
}

TEST_F(LivePackagerBaseTest, CustomMoofSequenceNumber) {
  std::vector<uint8_t> init_segment_buffer = ReadTestDataFile("input/init.mp4");
  ASSERT_FALSE(init_segment_buffer.empty());
  LiveConfig live_config;
  live_config.format = LiveConfig::OutputFormat::FMP4;
  live_config.track_type = LiveConfig::TrackType::VIDEO;
  live_config.protection_scheme = LiveConfig::EncryptionScheme::NONE;

  for (unsigned int i = 0; i < kNumSegments; i++) {
    live_config.segment_number = i + 1;
    std::string segment_num = absl::StrFormat("input/%04d.m4s", i);
    std::vector<uint8_t> segment_buffer = ReadTestDataFile(segment_num);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentData init_seg(init_segment_buffer.data(),
                         init_segment_buffer.size());
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());

    SegmentBuffer out;
    LivePackager packager(live_config);

    ASSERT_EQ(Status::OK, packager.Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);

    CheckSegment(live_config, out, 10000000, false);
  }
}

struct LivePackagerTestCase {
  unsigned int num_segments;
  std::string init_segment_name;
  LiveConfig::EncryptionScheme encryption_scheme;
  LiveConfig::OutputFormat output_format;
  LiveConfig::TrackType track_type;
  const char* media_segment_format;
  bool compare_samples;
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

 protected:
  static std::vector<uint8_t> ReadExpectedData() {
    // TODO: make this more generic to handle mp2t as well
    std::vector<uint8_t> buf = ReadTestDataFile("expected/fmp4/init.mp4");
    for (unsigned int i = 0; i < GetParam().num_segments; i++) {
      auto seg_buf =
          ReadTestDataFile(absl::StrFormat("expected/fmp4/%04d.m4s", i + 1));
      buf.insert(buf.end(), seg_buf.begin(), seg_buf.end());
    }

    return buf;
  }

  static std::unique_ptr<media::KeySource> MakeKeySource() {
    RawKeyParams raw_key;
    RawKeyParams::KeyInfo& key_info = raw_key.key_map[""];
    key_info.key = {std::begin(kKey), std::end(kKey)};
    key_info.key_id = {std::begin(kKeyId), std::end(kKeyId)};
    key_info.iv = {std::begin(kIv), std::end(kIv)};

    return media::RawKeySource::Create(raw_key);
  }

  // TODO: once we have a similar parser created for mp2t, we can create a more
  // generic way to handle reading and comparison of media samples.
  std::unique_ptr<media::KeySource> key_source_ = MakeKeySource();
  std::unique_ptr<MP4MediaParserTest> parser_noenc_ =
      std::make_unique<MP4MediaParserTest>(nullptr);
  std::unique_ptr<MP4MediaParserTest> parser_enc_ =
      std::make_unique<MP4MediaParserTest>(key_source_.get());
};

TEST_P(LivePackagerEncryptionTest, VerifyWithEncryption) {
  std::vector<uint8_t> init_segment_buffer =
      ReadTestDataFile(GetParam().init_segment_name);
  ASSERT_FALSE(init_segment_buffer.empty());

  SegmentData init_seg(init_segment_buffer.data(), init_segment_buffer.size());

  SegmentBuffer actual_buf;
  live_packager_->PackageInit(init_seg, actual_buf);

  for (unsigned int i = 0; i < GetParam().num_segments; i++) {
    std::string input_fname;
    ASSERT_TRUE(
        FormatWithIndex(GetParam().media_segment_format, i, input_fname));

    std::vector<uint8_t> segment_buffer = ReadTestDataFile(input_fname);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentBuffer out;
    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);

    actual_buf.AppendData(out.Data(), out.Size());
  }

  if (GetParam().compare_samples) {
    auto expected_buf = ReadExpectedData();
    CHECK(parser_noenc_->Parse(expected_buf.data(), expected_buf.size()));
    auto& expected_samples = parser_noenc_->GetSamples();

    CHECK(parser_enc_->Parse(actual_buf.Data(), actual_buf.Size()));
    auto& actual_samples = parser_enc_->GetSamples();

    CHECK_EQ(expected_samples.size(), actual_samples.size());
    CHECK(std::equal(expected_samples.begin(), expected_samples.end(),
                     actual_samples.begin(), actual_samples.end(),
                     [](const auto& s1, const auto& s2) {
                       return s1->data_size() == s2->data_size() &&
                              0 == memcmp(s1->data(), s2->data(),
                                          s1->data_size());
                     }));
  }
}

INSTANTIATE_TEST_CASE_P(
    LivePackagerEncryptionTypes,
    LivePackagerEncryptionTest,
    ::testing::Values(
        // Verify FMP4 to TS with Sample Aes encryption.
        LivePackagerTestCase{
            10, "input/init.mp4", LiveConfig::EncryptionScheme::SAMPLE_AES,
            LiveConfig::OutputFormat::TS, LiveConfig::TrackType::VIDEO,
            "input/%04d.m4s", false},
        // Verify FMP4 to TS with AES-128 encryption.
        LivePackagerTestCase{
            10, "input/init.mp4", LiveConfig::EncryptionScheme::AES_128,
            LiveConfig::OutputFormat::TS, LiveConfig::TrackType::VIDEO,
            "input/%04d.m4s", false},
        // Verify FMP4 to FMP4 with Sample AES encryption.
        LivePackagerTestCase{
            10, "input/init.mp4", LiveConfig::EncryptionScheme::SAMPLE_AES,
            LiveConfig::OutputFormat::FMP4, LiveConfig::TrackType::VIDEO,
            "input/%04d.m4s", true},
        // Verify FMP4 to FMP4 with CENC encryption.
        LivePackagerTestCase{
            10, "input/init.mp4", LiveConfig::EncryptionScheme::CENC,
            LiveConfig::OutputFormat::FMP4, LiveConfig::TrackType::VIDEO,
            "input/%04d.m4s", true},
        // Verify FMP4 to FMP4 with CBCS encryption.
        LivePackagerTestCase{
            10, "input/init.mp4", LiveConfig::EncryptionScheme::CBCS,
            LiveConfig::OutputFormat::FMP4, LiveConfig::TrackType::VIDEO,
            "input/%04d.m4s", true},
        // Verify AUDIO segments only to TS with Sample AES encryption.
        LivePackagerTestCase{
            5, "audio/en/init.mp4", LiveConfig::EncryptionScheme::SAMPLE_AES,
            LiveConfig::OutputFormat::TS, LiveConfig::TrackType::AUDIO,
            "audio/en/%05d.m4s", false}));

struct LivePackagerReEncryptCase {
  unsigned int num_segments;
  std::string init_segment_name;
  LiveConfig::EncryptionScheme encryption_scheme;
  LiveConfig::OutputFormat output_format;
  LiveConfig::TrackType track_type;
  ProtectionSystem protection_system;
  const char* media_segment_format;
  bool emsg_processing;
};

class LivePackagerTestReEncrypt
    : public LivePackagerBaseTest,
      public ::testing::WithParamInterface<LivePackagerReEncryptCase> {
 public:
  void SetUp() override {
    LivePackagerBaseTest::SetUp();
    LiveConfig live_config;
    live_config.format = GetParam().output_format;
    live_config.track_type = GetParam().track_type;
    live_config.protection_scheme = GetParam().encryption_scheme;
    live_config.protection_system = GetParam().protection_system;
    live_config.decryption_key = HexStringToVector(kKeyHex);
    live_config.decryption_key_id = HexStringToVector(kKeyIdHex);
    SetupLivePackagerConfig(live_config);
  }

 protected:
  static std::vector<uint8_t> ReadExpectedData() {
    std::vector<uint8_t> buf =
        ReadTestDataFile("encrypted/prd_data/decrypt/fmp4/init.mp4");
    for (unsigned int i = 0; i < GetParam().num_segments; i++) {
      auto seg_buf = ReadTestDataFile(
          absl::StrFormat("encrypted/prd_data/decrypt/fmp4/%05d.m4s", i + 1));
      buf.insert(buf.end(), seg_buf.begin(), seg_buf.end());
    }

    return buf;
  }

  static std::unique_ptr<media::KeySource> MakeKeySource() {
    RawKeyParams raw_key;
    RawKeyParams::KeyInfo& key_info = raw_key.key_map[""];
    key_info.key = {std::begin(kKey), std::end(kKey)};
    key_info.key_id = {std::begin(kKeyId), std::end(kKeyId)};
    key_info.iv = {std::begin(kIv), std::end(kIv)};

    return media::RawKeySource::Create(raw_key);
  }

  std::unique_ptr<media::KeySource> key_source_ = MakeKeySource();
  std::unique_ptr<MP4MediaParserTest> parser_noenc_ =
      std::make_unique<MP4MediaParserTest>(nullptr);
  std::unique_ptr<MP4MediaParserTest> parser_enc_ =
      std::make_unique<MP4MediaParserTest>(key_source_.get());
};

inline bool operator==(const media::mp4::DASHEventMessageBox& lhs,
                       const media::mp4::DASHEventMessageBox& rhs) {
  return std::tie(lhs.scheme_id_uri, lhs.value, lhs.timescale,
                  lhs.presentation_time_delta, lhs.event_duration, lhs.id,
                  lhs.message_data) ==
         std::tie(rhs.scheme_id_uri, rhs.value, rhs.timescale,
                  rhs.presentation_time_delta, rhs.event_duration, rhs.id,
                  rhs.message_data);
}

TEST_P(LivePackagerTestReEncrypt, VerifyReEncryption) {
  std::vector<uint8_t> init_segment_buffer =
      ReadTestDataFile(GetParam().init_segment_name);
  ASSERT_FALSE(init_segment_buffer.empty());

  SegmentData init_seg(init_segment_buffer.data(), init_segment_buffer.size());

  SegmentBuffer actual_buf;
  ASSERT_EQ(Status::OK, live_packager_->PackageInit(init_seg, actual_buf));
  if (GetParam().protection_system != ProtectionSystem::kNone) {
    CheckVideoPsshInfo(GetParam().protection_system, actual_buf);
  }

  for (unsigned int i = 0; i < GetParam().num_segments; i++) {
    std::string input_fname;
    ASSERT_TRUE(
        FormatWithIndex(GetParam().media_segment_format, i + 1, input_fname));

    std::vector<uint8_t> segment_buffer = ReadTestDataFile(input_fname);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentBuffer out;
    LiveConfig live_config;
    live_config.segment_number = i + 1;
    live_config.format = GetParam().output_format;
    live_config.track_type = GetParam().track_type;
    live_config.protection_scheme = GetParam().encryption_scheme;
    live_config.decryption_key = HexStringToVector(kKeyHex);
    live_config.decryption_key_id = HexStringToVector(kKeyIdHex);
    live_config.emsg_processing = GetParam().emsg_processing;

    SetupLivePackagerConfig(live_config);

    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());
    ASSERT_EQ(Status::OK, live_packager_->Package(init_seg, media_seg, out));
    ASSERT_GT(out.Size(), 0);
    actual_buf.AppendData(out.Data(), out.Size());
  }

  auto expected_buf = ReadExpectedData();
  CHECK(parser_noenc_->Parse(expected_buf.data(), expected_buf.size()));
  auto& expected_samples = parser_noenc_->GetSamples();
  auto& expected_emsg_samples = parser_noenc_->GetEmsgSamples();

  CHECK(parser_enc_->Parse(actual_buf.Data(), actual_buf.Size()));
  auto& actual_samples = parser_enc_->GetSamples();
  auto& actual_emsg_samples = parser_enc_->GetEmsgSamples();

  CHECK_EQ(expected_samples.size(), actual_samples.size());
  ASSERT_GT(expected_samples.size(), 0);
  CHECK(std::equal(
      expected_samples.begin(), expected_samples.end(), actual_samples.begin(),
      actual_samples.end(), [](const auto& s1, const auto& s2) {
        return s1->data_size() == s2->data_size() &&
               0 == memcmp(s1->data(), s2->data(), s1->data_size());
      }));

  if (GetParam().emsg_processing) {
    ASSERT_GT(expected_emsg_samples.size(), 0);
    CHECK_EQ(expected_emsg_samples.size(), actual_emsg_samples.size());
    CHECK(std::equal(expected_emsg_samples.begin(), expected_emsg_samples.end(),
                     actual_emsg_samples.begin(), actual_emsg_samples.end(),
                     [](const auto& s1, const auto& s2) {
                       return (*s1.get()) == (*s2.get());
                     }));
  } else {
    ASSERT_EQ(actual_emsg_samples.size(), 0);
  }
}

INSTANTIATE_TEST_CASE_P(
    LivePackagerReEncryptTypes,
    LivePackagerTestReEncrypt,
    ::testing::Values(
        // Verify decrypt FMP4 and re-encrypt to FMP4 with CENC encryption,
        // ENABLE processing EMSG.
        LivePackagerReEncryptCase{
            7, "encrypted/prd_data/init.mp4",
            LiveConfig::EncryptionScheme::CENC, LiveConfig::OutputFormat::FMP4,
            LiveConfig::TrackType::VIDEO,
            ProtectionSystem::kPlayReady | ProtectionSystem::kWidevine,
            "encrypted/prd_data/%05d.m4s", true},
        // Verify decrypt FMP4 and re-encrypt to FMP4 with CBCS encryption,
        // ENABLE processing EMSG.
        LivePackagerReEncryptCase{
            7, "encrypted/prd_data/init.mp4",
            LiveConfig::EncryptionScheme::CBCS, LiveConfig::OutputFormat::FMP4,
            LiveConfig::TrackType::VIDEO, ProtectionSystem::kWidevine,
            "encrypted/prd_data/%05d.m4s", true},
        // Verify decrypt FMP4 and re-encrypt to FMP4 with CBCS encryption,
        // DISABLE processing EMSG
        LivePackagerReEncryptCase{
            7,
            "encrypted/prd_data/init.mp4",
            LiveConfig::EncryptionScheme::CBCS,
            LiveConfig::OutputFormat::FMP4,
            LiveConfig::TrackType::VIDEO,
            ProtectionSystem::kPlayReady,
            "encrypted/prd_data/%05d.m4s",
            false,
        }));

struct TimedTextTestCase {
  const char* media_segment_format;
  const char* expected_segment_format;
  LiveConfig::TrackType track_type;
  LiveConfig::OutputFormat output_format;
  media::FourCC handler_type;
  media::FourCC format;
  Status expected_status;
  int64_t start_decode_time;
};

class TimedTextParameterizedTest
    : public LivePackagerBaseTest,
      public ::testing::WithParamInterface<TimedTextTestCase> {};

TEST_P(TimedTextParameterizedTest, VerifyTimedText) {
  for (unsigned int i = 0; i < kNumSegments; i++) {
    std::string input_fname;
    ASSERT_TRUE(
        FormatWithIndex(GetParam().media_segment_format, i, input_fname));

    std::vector<uint8_t> segment_buffer = ReadTestDataFile(input_fname);
    ASSERT_FALSE(segment_buffer.empty());

    SegmentData media_seg(segment_buffer.data(), segment_buffer.size());
    FullSegmentBuffer out;

    LiveConfig live_config;
    live_config.protection_scheme = LiveConfig::EncryptionScheme::NONE;
    live_config.format = GetParam().output_format;
    live_config.track_type = GetParam().track_type;
    if (live_config.format == LiveConfig::OutputFormat::VTTMP4 ||
        live_config.format == LiveConfig::OutputFormat::TTMLMP4) {
      live_config.segment_number = i + 1;
      live_config.timed_text_decode_time =
          GetParam().start_decode_time + (i * kSegmentDurationMs);
    }

    SetupLivePackagerConfig(live_config);
    ASSERT_EQ(GetParam().expected_status,
              live_packager_->PackageTimedText(media_seg, out));
    if (GetParam().expected_status == Status::OK) {
      ASSERT_GT(out.SegmentSize(), 0);
      if (live_config.format == LiveConfig::OutputFormat::VTTMP4 ||
          live_config.format == LiveConfig::OutputFormat::TTMLMP4) {
        SegmentBuffer seg;
        seg.AppendData(out.SegmentData(), out.SegmentSize());
        CheckSegment(live_config, seg, 1000, true);

        if (i == 0) {
          CheckTextInitSegment(out, GetParam().handler_type, GetParam().format);
        }

        std::string expected_fname;
        ASSERT_TRUE(FormatWithIndex(GetParam().expected_segment_format, i + 1,
                                    expected_fname));

        std::vector<uint8_t> expected_buf(ReadTestDataFile(expected_fname));
        ASSERT_FALSE(expected_buf.empty());

        std::vector<uint8_t> actual_buf(out.SegmentData(),
                                        out.SegmentData() + out.SegmentSize());
        ASSERT_EQ(expected_buf, actual_buf);
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    LivePackagerTimedText,
    TimedTextParameterizedTest,
    ::testing::Values(
        // VTT in text --> VTT in MP4
        TimedTextTestCase{
            "timed_text/input/en.m3u8_%010d.vtt",
            "timed_text/expected/vtt/%05d.m4s",
            LiveConfig::TrackType::TEXT,
            LiveConfig::OutputFormat::VTTMP4,
            media::FourCC::FOURCC_text,
            media::FourCC::FOURCC_wvtt,
            Status::OK,
            0,
        },
        // VTT in text --> TTML in Text
        TimedTextTestCase{
            "timed_text/input/en.m3u8_%010d.vtt",
            "",
            LiveConfig::TrackType::TEXT,
            LiveConfig::OutputFormat::TTML,
            media::FourCC::FOURCC_NULL,
            media::FourCC::FOURCC_NULL,
            Status::OK,
            0,
        },
        // VTT in text --> TTML in MP4
        TimedTextTestCase{
            "timed_text/input/en.m3u8_%010d.vtt",
            "timed_text/expected/ttml/%05d.m4s",
            LiveConfig::TrackType::TEXT,
            LiveConfig::OutputFormat::TTMLMP4,
            media::FourCC::FOURCC_subt,
            media::FourCC::FOURCC_stpp,
            Status::OK,
            16000,
        },
        // Invalid track type of audio
        TimedTextTestCase{
            "timed_text/input/en.m3u8_%010d.vtt",
            "",
            LiveConfig::TrackType::AUDIO,
            LiveConfig::OutputFormat::TTMLMP4,
            media::FourCC::FOURCC_NULL,
            media::FourCC::FOURCC_NULL,
            Status(error::INVALID_ARGUMENT, "Stream not available"),
            0,
        },
        // Invalid track type of video
        TimedTextTestCase{
            "timed_text/input/en.m3u8_%010d.vtt",
            "",
            LiveConfig::TrackType::VIDEO,
            LiveConfig::OutputFormat::TTMLMP4,
            media::FourCC::FOURCC_NULL,
            media::FourCC::FOURCC_NULL,
            Status(error::INVALID_ARGUMENT, "Stream not available"),
            0,
        }));
}  // namespace shaka
