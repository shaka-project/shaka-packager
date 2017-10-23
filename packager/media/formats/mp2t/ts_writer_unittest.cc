// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/formats/mp2t/pes_packet.h"
#include "packager/media/formats/mp2t/program_map_table_writer.h"
#include "packager/media/formats/mp2t/ts_writer.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace shaka {
namespace media {
namespace mp2t {

namespace {

const int kTsPacketSize = 188;
const Codec kCodecForTesting = kCodecH264;

class MockProgramMapTableWriter : public ProgramMapTableWriter {
 public:
  MockProgramMapTableWriter() : ProgramMapTableWriter(kCodecForTesting) {}
  ~MockProgramMapTableWriter() override = default;

  MOCK_METHOD1(EncryptedSegmentPmt, bool(BufferWriter* writer));
  MOCK_METHOD1(ClearSegmentPmt, bool(BufferWriter* writer));

 private:
  MockProgramMapTableWriter(const MockProgramMapTableWriter&) = delete;
  MockProgramMapTableWriter& operator=(const MockProgramMapTableWriter&) =
      delete;

  bool WriteDescriptors(BufferWriter* writer) const override { return true; }
};

// This is not a real TS Packet. But is used to check that the result from the
// MockProgramMapTableWriter is used at the right place.
const uint8_t kMockPmtWriterData[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B,
    0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B,
    0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB,
};

ACTION(WriteOnePmt) {
  BufferWriter* writer = arg0;
  writer->AppendArray(kMockPmtWriterData, arraysize(kMockPmtWriterData));
  return true;
}

ACTION(WriteTwoPmts) {
  BufferWriter* writer = arg0;
  writer->AppendArray(kMockPmtWriterData, arraysize(kMockPmtWriterData));
  writer->AppendArray(kMockPmtWriterData, arraysize(kMockPmtWriterData));
  return true;
}

}  // namespace

class TsWriterTest : public ::testing::Test {
 protected:
  // Using different file names for each test so that the tests can be run in
  // parallel.
  void SetUp() override {
    base::CreateTemporaryFile(&test_file_path_);
    // TODO(rkuroiwa): Use memory file prefix once its exposed.
    test_file_name_ =
        std::string(kLocalFilePrefix) + test_file_path_.AsUTF8Unsafe();
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
  void ExpectTsPacketEqual(const uint8_t* prefix,
                           size_t prefix_size,
                           int padding_length,
                           const uint8_t* suffix,
                           size_t suffix_size,
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

  base::FilePath test_file_path_;
};

// Verify that PAT and PMT are correct for clear segment.
// This test covers verifies the PAT, and since it doesn't change, other tests
// shouldn't have to check this.
TEST_F(TsWriterTest, ClearH264Psi) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_)).WillOnce(WriteOnePmt());

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // 2 TS Packets one for PAT and the fake PMT data.
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

  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + kTsPacketSize,
                      kTsPacketSize));
}

TEST_F(TsWriterTest, ClearAacPmt) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_)).WillOnce(WriteOnePmt());

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));
  // 2 TS Packets. PAT, PMT.
  ASSERT_EQ(376u, content.size());

  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + kTsPacketSize,
                      kTsPacketSize));
}

// The stream is flaged with will_be_encrypted. Verify that 2 PMTs are created.
// One for clear lead and another for encrypted segments that follow.
TEST_F(TsWriterTest, ClearLeadH264Pmt) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_))
      .WillOnce(WriteTwoPmts());

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  EXPECT_TRUE(ts_writer.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));

  ASSERT_EQ(564u, content.size());

  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + kTsPacketSize,
                      kTsPacketSize));
  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + 2 * kTsPacketSize,
                      kTsPacketSize));
}

TEST_F(TsWriterTest, ClearSegmentPmtFailure) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_)).WillOnce(Return(false));

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_FALSE(ts_writer.NewSegment(test_file_name_));
}

// Check the encrypted segments' PMT (after clear lead).
TEST_F(TsWriterTest, EncryptedSegmentsH264Pmt) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  InSequence s;
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pmt_writer, EncryptedSegmentPmt(_)).WillOnce(WriteOnePmt());

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  EXPECT_TRUE(ts_writer.FinalizeSegment());

  // Overwrite the file but as encrypted segment.
  ts_writer.SignalEncrypted();
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  EXPECT_TRUE(ts_writer.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));

  ASSERT_EQ(376u, content.size());

  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + kTsPacketSize,
                      kTsPacketSize));
}

TEST_F(TsWriterTest, EncryptedSegmentPmtFailure) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  InSequence s;
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pmt_writer, EncryptedSegmentPmt(_)).WillOnce(Return(false));

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  EXPECT_TRUE(ts_writer.FinalizeSegment());

  ts_writer.SignalEncrypted();
  EXPECT_FALSE(ts_writer.NewSegment(test_file_name_));
}

// Same as ClearLeadH264Pmt but for AAC.
TEST_F(TsWriterTest, ClearLeadAacPmt) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_))
      .WillOnce(WriteTwoPmts());

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));

  ASSERT_EQ(564u, content.size());

  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + kTsPacketSize,
                      kTsPacketSize));
  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + 2 * kTsPacketSize,
                      kTsPacketSize));
}

// Same as EncryptedSegmentsH264Pmt but for AAC.
TEST_F(TsWriterTest, EncryptedSegmentsAacPmt) {
  std::unique_ptr<MockProgramMapTableWriter> mock_pmt_writer(
      new MockProgramMapTableWriter());
  InSequence s;
  EXPECT_CALL(*mock_pmt_writer, ClearSegmentPmt(_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_pmt_writer, EncryptedSegmentPmt(_)).WillOnce(WriteOnePmt());

  TsWriter ts_writer(std::move(mock_pmt_writer));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  EXPECT_TRUE(ts_writer.FinalizeSegment());

  // Overwrite the file but as encrypted segment.
  ts_writer.SignalEncrypted();
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));
  EXPECT_TRUE(ts_writer.FinalizeSegment());

  std::vector<uint8_t> content;
  ASSERT_TRUE(ReadFileToVector(test_file_path_, &content));

  ASSERT_EQ(376u, content.size());

  EXPECT_EQ(0, memcmp(kMockPmtWriterData, content.data() + kTsPacketSize,
                      kTsPacketSize));
}


TEST_F(TsWriterTest, AddPesPacket) {
  TsWriter ts_writer(std::unique_ptr<ProgramMapTableWriter>(
      new VideoProgramMapTableWriter(kCodecForTesting)));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));

  std::unique_ptr<PesPacket> pes(new PesPacket());
  pes->set_stream_id(0xE0);
  pes->set_pts(0x900);
  pes->set_dts(0x900);
  const uint8_t kAnyData[] = {
      0x12, 0x88, 0x4f, 0x4a,
  };
  pes->mutable_data()->assign(kAnyData, kAnyData + arraysize(kAnyData));

  EXPECT_TRUE(ts_writer.AddPesPacket(std::move(pes)));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

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
  TsWriter ts_writer(std::unique_ptr<ProgramMapTableWriter>(
      new VideoProgramMapTableWriter(kCodecForTesting)));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));

  std::unique_ptr<PesPacket> pes(new PesPacket());
  pes->set_pts(0);
  pes->set_dts(0);
  // A little over 2 TS Packets (3 TS Packets).
  const std::vector<uint8_t> big_data(400, 0x23);
  *pes->mutable_data() = big_data;

  EXPECT_TRUE(ts_writer.AddPesPacket(std::move(pes)));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

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
  EXPECT_EQ(1, (content[3 * 188 + 3] & 0xF));
  EXPECT_EQ(2, (content[4 * 188 + 3] & 0xF));
}

// Bug found in code review. It should check whether PTS is present not whether
// PTS (implicilty) cast to bool is true.
TEST_F(TsWriterTest, PesPtsZeroNoDts) {
  TsWriter ts_writer(std::unique_ptr<ProgramMapTableWriter>(
      new VideoProgramMapTableWriter(kCodecForTesting)));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));

  std::unique_ptr<PesPacket> pes(new PesPacket());
  pes->set_stream_id(0xE0);
  pes->set_pts(0x0);
  const uint8_t kAnyData[] = {
      0x12, 0x88, 0x4F, 0x4A,
  };
  pes->mutable_data()->assign(kAnyData, kAnyData + arraysize(kAnyData));

  EXPECT_TRUE(ts_writer.AddPesPacket(std::move(pes)));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

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
  TsWriter ts_writer(std::unique_ptr<ProgramMapTableWriter>(
      new VideoProgramMapTableWriter(kCodecForTesting)));
  EXPECT_TRUE(ts_writer.NewSegment(test_file_name_));

  std::unique_ptr<PesPacket> pes(new PesPacket());
  pes->set_stream_id(0xE0);
  pes->set_pts(0x00);
  pes->set_dts(0x00);

  // Note that first TS packet will have adaptation fields with PCR, so make
  // payload big enough so that second PES packet's payload is 183.
  // First TS packet can carry 157 bytes of PES payload. The next one should
  // carry 183 bytes.
  std::vector<uint8_t> pes_payload(157 + 183, 0xAF);
  *pes->mutable_data() = pes_payload;

  EXPECT_TRUE(ts_writer.AddPesPacket(std::move(pes)));
  ASSERT_TRUE(ts_writer.FinalizeSegment());

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
}  // namespace shaka
