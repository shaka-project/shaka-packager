// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/formats/mp2t/pes_packet.h"
#include "packager/media/formats/mp2t/ts_writer.h"

namespace edash_packager {
namespace media {
namespace mp2t {

namespace {

const int kTsPacketSize = 188;

// Only {Audio,Video}Codec matter for this test. Other values are bogus.
const VideoCodec kH264VideoCodec = VideoCodec::kCodecH264;
const AudioCodec kAacAudioCodec = AudioCodec::kCodecAAC;
const int kTrackId = 0;
const uint32_t kTimeScale = 90000;
const uint64_t kDuration = 180000;
const char kCodecString[] = "avc1";
const char kLanguage[] = "eng";
const uint32_t kWidth = 1280;
const uint32_t kHeight = 720;
const uint32_t kPixelWidth = 1;
const uint32_t kPixelHeight = 1;
const uint16_t kTrickPlayRate = 1;
const uint8_t kNaluLengthSize = 1;
const bool kIsEncrypted = false;

const uint8_t kSampleBits = 16;
const uint8_t kNumChannels = 2;
const uint32_t kSamplingFrequency = 44100;
const uint32_t kMaxBitrate = 320000;
const uint32_t kAverageBitrate = 256000;

const uint8_t kExtraData[] = {
    0x01, 0x02,
};

}  // namespace

class TsWriterTest : public ::testing::Test {
 protected:
  // Using different file names for each test so that the tests can be run in
  // parallel.
  void SetUp() override {
    base::CreateTemporaryFile(&test_file_path_);
    // TODO(rkuroiwa): Use memory file prefix once its exposed.
    test_file_name_ = kLocalFilePrefix + test_file_path_.value();
  }

  void TearDown() override {
    const bool kRecursive = true;
    base::DeleteFile(test_file_path_, !kRecursive);
  }

  bool ReadFileToVector(const base::FilePath& path, std::vector<uint8_t>* out) {
    std::string content;
    if (!base::ReadFileToString(path, &content))
      return false;
    out->assign(content.begin(), content.end());
    return true;
  }

  // Checks whether |actual|'s prefix matches with |prefix| and the suffix
  // matches with |suffix|. If there is padding, then padding_length specifies
  // how long the padding is between prefix and suffix.
  // |actual| must be at least 188 bytes long.
  void ExpectTsPacketEqual(const uint8_t* prefix, size_t prefix_size,
                           int padding_length,
                           const uint8_t* suffix, size_t suffix_size,
                           const uint8_t* actual) {
    std::vector<uint8_t> actual_prefix(actual, actual + prefix_size);
    EXPECT_EQ(std::vector<uint8_t>(prefix, prefix + prefix_size),
              actual_prefix);

    // Padding until the payload.
    for (size_t i = prefix_size; i < kTsPacketSize - suffix_size; ++i) {
      EXPECT_EQ(0xFF, actual[i]) << "at index " << i;
    }

    std::vector<uint8_t> actual_suffix(actual + prefix_size + padding_length,
                                       actual + kTsPacketSize);
    EXPECT_EQ(std::vector<uint8_t>(suffix, suffix + suffix_size),
              actual_suffix);
  }

  std::string test_file_name_;
  TsWriter ts_writer_;

  base::FilePath test_file_path_;
};

TEST_F(TsWriterTest, InitializeVideoH264) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
}

TEST_F(TsWriterTest, InitializeVideoNonH264) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, VideoCodec::kCodecVP9, kCodecString,
      kLanguage, kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_FALSE(ts_writer_.Initialize(*stream_info));
}

TEST_F(TsWriterTest, InitializeAudioAac) {
  scoped_refptr<AudioStreamInfo> stream_info(new AudioStreamInfo(
      kTrackId, kTimeScale, kDuration, kAacAudioCodec, kCodecString, kLanguage,
      kSampleBits, kNumChannels, kSamplingFrequency, kMaxBitrate,
      kAverageBitrate, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
}

TEST_F(TsWriterTest, InitializeAudioNonAac) {
  scoped_refptr<AudioStreamInfo> stream_info(new AudioStreamInfo(
      kTrackId, kTimeScale, kDuration, AudioCodec::kCodecOpus, kCodecString,
      kLanguage, kSampleBits, kNumChannels, kSamplingFrequency, kMaxBitrate,
      kAverageBitrate, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_FALSE(ts_writer_.Initialize(*stream_info));
}

TEST_F(TsWriterTest, NewSegment) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
  EXPECT_TRUE(ts_writer_.NewSegment(test_file_name_));
  ASSERT_TRUE(ts_writer_.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // 2 TS Packets. PAT, PMT.
  ASSERT_EQ(376u, content.size());

  const uint8_t kExpectedPatPrefix[] = {
    0x47,  // Sync byte.
    0x40,  // payload_unit_start_indicator set.
    0x00,  // pid.
    0x30,  // Adaptation field and payload are both present. counter = 0.
    0xA6,  // Adaptation Field length.
    0x00,  // All adaptation field flags 0.
  };
  const int kExpectedPatPrefixSize = arraysize(kExpectedPatPrefix);
  const uint8_t kExpectedPatPayload[] = {
      0x00,  // pointer field
      0x00,
      0xB0,        // The last 2 '00' assumes that this PAT is not very long.
      0x0D,        // Length of the rest of this array.
      0x00, 0x00,  // Transport stream ID is 0.
      0xC1,        // version number 0, current next indicator 1.
      0x00,        // section number
      0x00,        // last section number
      // program number -> PMT PID mapping.
      0x00, 0x01,  // program number is 1.
      0xE0,        // first 3 bits is reserved.
      0x20,        // PMT PID.
      // CRC32.
      0xf9, 0x62, 0xf5, 0x8b,
  };

  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kExpectedPatPrefix, kExpectedPatPrefixSize, 165, kExpectedPatPayload,
      arraysize(kExpectedPatPayload), content.data()));

  const uint8_t kExpectedPmtPrefix[] = {
    0x47,  // Sync byte.
    0x40,  // payload_unit_start_indicator set.
    0x20,  // pid.
    0x30,  // Adaptation field and payload are both present. counter = 0.
    0xA1,  // Adaptation Field length.
    0x00,  // All adaptation field flags 0.
  };
  const int kExpectedPmtPrefixSize = arraysize(kExpectedPmtPrefix);
  const uint8_t kPmtH264[] = {
      0x00,  // pointer field
      0x02,
      0xB0,  // assumes length is <= 256 bytes.
      0x12,  // length of the rest of this array.
      0x00, 0x01,
      0xC1,              // version 0, current next indicator 1.
      0x00,              // section number
      0x00,              // last section number.
      0xE0,              // first 3 bits reserved.
      0x50,              // PCR PID is the elementary streams PID.
      0xF0,              // first 4 bits reserved.
      0x00,              // No descriptor at this level.
      0x1B, 0xE0, 0x50,  // stream_type -> PID.
      0xF0, 0x00,        // Es_info_length is 0.
      // CRC32.
      0x43, 0x49, 0x97, 0xbe,
  };

  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kExpectedPmtPrefix, kExpectedPmtPrefixSize, 160, kPmtH264,
      arraysize(kPmtH264), content.data() + kTsPacketSize));
}

TEST_F(TsWriterTest, AddPesPacket) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
  EXPECT_TRUE(ts_writer_.NewSegment(test_file_name_));

  scoped_ptr<PesPacket> pes(new PesPacket());
  pes->set_stream_id(0xE0);
  pes->set_pts(0x900);
  pes->set_dts(0x900);
  const uint8_t kAnyData[] = {
      0x12, 0x88, 0x4f, 0x4a,
  };
  pes->mutable_data()->assign(kAnyData, kAnyData + arraysize(kAnyData));

  EXPECT_TRUE(ts_writer_.AddPesPacket(pes.Pass()));
  ASSERT_TRUE(ts_writer_.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // 3 TS Packets. PAT, PMT, and PES.
  ASSERT_EQ(564u, content.size());

  const int kPesStartPosition = 376;

  // Prefix of the expected output. Rest of the packet should be filled with
  // padding.
  const uint8_t kExpectedOutputPrefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x50,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0xA0,  // Adaptation Field length.
      0x10,  // pcr flag.
      0x00, 0x00, 0x04, 0x80, 0x00, 0x00,  // PCR.
  };

  const uint8_t kExpectedPayload[] = {
      0x00, 0x00, 0x01,  // Start code.
      0xE0,              // stream id.
      0x00, 0x11,        // PES_packet_length.
      0x80,              // Flags.
      0xC0,              // PTS and DTS both present.
      0x0A,              // PES_header_data_length.
      0x31,  // Since PTS is 0 this is '0011' (fixed) and marker bit at LSB.
      0x00,  // PTS leading bits 0.
      0x01,  // PTS 0 followed by marker bit.
      0x12,  // PTS 0x900 shifted.
      0x01,  // PTS 0 followed by marker bit.
      0x11,  // Fixed '0001' followed by marker bit at LSB.
      0x00,  // DTS leading bits 0.
      0x01,  // DTS 0 followed by marker bit.
      0x12,  // DTS 0x900 shifted.
      0x01,  // DTS 0 followed by marker bit.
      0x12, 0x88, 0x4f, 0x4a,  // Payload.
  };
  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kExpectedOutputPrefix, arraysize(kExpectedOutputPrefix), 153,
      kExpectedPayload, arraysize(kExpectedPayload),
      content.data() + kPesStartPosition));
}

// Verify that PES packet > 64KiB can be handled.
TEST_F(TsWriterTest, BigPesPacket) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
  EXPECT_TRUE(ts_writer_.NewSegment(test_file_name_));

  scoped_ptr<PesPacket> pes(new PesPacket());
  pes->set_pts(0);
  pes->set_dts(0);
  // A little over 2 TS Packets (3 TS Packets).
  const std::vector<uint8_t> big_data(400, 0x23);
  *pes->mutable_data() = big_data;

  EXPECT_TRUE(ts_writer_.AddPesPacket(pes.Pass()));
  ASSERT_TRUE(ts_writer_.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // The first TsPacket can only carry
  // 177 (TS packet size - header - adaptation_field) - 19 (PES header data) =
  // 158 bytes of the PES packet payload.
  // So this should create
  // 2 + 1 + ceil((400 - 158) / 184) = 5 TsPackets.
  // Where 184 is the maxium payload of a TS packet.
  EXPECT_EQ(5u * 188, content.size());

  // Check continuity counter.
  EXPECT_EQ(0, (content[2 * 188 + 3] & 0xF));
  EXPECT_EQ(1, (content[3 * 188  + 3] & 0xF));
  EXPECT_EQ(2, (content[4 * 188  + 3] & 0xF));
}

// Bug found in code review. It should check whether PTS is present not whether
// PTS (implicilty) cast to bool is true.
TEST_F(TsWriterTest, PesPtsZeroNoDts) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
  EXPECT_TRUE(ts_writer_.NewSegment(test_file_name_));

  scoped_ptr<PesPacket> pes(new PesPacket());
  pes->set_stream_id(0xE0);
  pes->set_pts(0x0);
  const uint8_t kAnyData[] = {
      0x12, 0x88, 0x4F, 0x4A,
  };
  pes->mutable_data()->assign(kAnyData, kAnyData + arraysize(kAnyData));

  EXPECT_TRUE(ts_writer_.AddPesPacket(pes.Pass()));
  ASSERT_TRUE(ts_writer_.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // 3 TS Packets. PAT, PMT, and PES.
  ASSERT_EQ(564u, content.size());

  const int kPesStartPosition = 376;

  // Prefix of the expected output. Rest of the packet should be filled with
  // padding.
  const uint8_t kExpectedOutputPrefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x50,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0xA5,  // Adaptation Field length.
      0x10,  // pcr flag.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // PCR.
  };

  const uint8_t kExpectedPayload[] = {
      0x00, 0x00, 0x01,  // Start code.
      0xE0,              // stream id.
      0x00, 0x0C,        // PES_packet_length.
      0x80,              // Flags.
      0x80,              // Only PTS present.
      0x05,              // PES_header_data_length.
      0x21,  // Since PTS is 0 this is '0010' (fixed) and marker bit at LSB.
      0x00,  // PTS 0.
      0x01,  // PTS 0 followed by marker bit.
      0x00,  // PTS 0.
      0x01,  // PTS 0 followed by marker bit.
      0x12, 0x88, 0x4F, 0x4A,  // Payload.
  };
  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kExpectedOutputPrefix, arraysize(kExpectedOutputPrefix), 158,
      kExpectedPayload, arraysize(kExpectedPayload),
      content.data() + kPesStartPosition));
}

// Verify that TS packet with payload 183 is handled correctly, e.g.
// adaptation_field_length should be 0.
TEST_F(TsWriterTest, TsPacketPayload183Bytes) {
  scoped_refptr<VideoStreamInfo> stream_info(new VideoStreamInfo(
      kTrackId, kTimeScale, kDuration, kH264VideoCodec, kCodecString, kLanguage,
      kWidth, kHeight, kPixelWidth, kPixelHeight, kTrickPlayRate,
      kNaluLengthSize, kExtraData, arraysize(kExtraData), kIsEncrypted));
  EXPECT_TRUE(ts_writer_.Initialize(*stream_info));
  EXPECT_TRUE(ts_writer_.NewSegment(test_file_name_));

  scoped_ptr<PesPacket> pes(new PesPacket());
  pes->set_stream_id(0xE0);
  pes->set_pts(0x00);
  pes->set_dts(0x00);

  // Note that first TS packet will have adaptation fields with PCR, so make
  // payload big enough so that second PES packet's payload is 183.
  // First TS packet can carry 157 bytes of PES payload. The next one should
  // carry 183 bytes.
  std::vector<uint8_t> pes_payload(157 + 183, 0xAF);
  *pes->mutable_data() = pes_payload;

  EXPECT_TRUE(ts_writer_.AddPesPacket(pes.Pass()));
  ASSERT_TRUE(ts_writer_.FinalizeSegment());

  const uint8_t kExpectedOutputPrefix[] = {
      0x47,  // Sync byte.
      0x00,  // payload_unit_start_indicator set.
      0x50,  // pid.
      0x31,  // Adaptation field and payload are both present. counter = 0.
      0x00,  // Adaptation Field length, 1 byte padding.
  };

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // 4 TsPackets. PAT, PMT, TsPacket with PES header, TsPacket rest of PES
  // payload.
  ASSERT_EQ(752u, content.size());

  const int kPesStartPosition = 564;
  std::vector<uint8_t> actual_prefix(content.data() + kPesStartPosition,
                                     content.data() + kPesStartPosition + 5);
  EXPECT_EQ(
      std::vector<uint8_t>(kExpectedOutputPrefix, kExpectedOutputPrefix + 5),
      actual_prefix);
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
