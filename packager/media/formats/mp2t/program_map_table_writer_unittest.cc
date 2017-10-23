// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include <vector>

#include "packager/media/base/buffer_writer.h"
#include "packager/media/formats/mp2t/continuity_counter.h"
#include "packager/media/formats/mp2t/program_map_table_writer.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {
const size_t kTsPacketSize = 188;
const uint8_t kAacBasicProfileExtraData[] = {0x12, 0x10};
// Bogus data, the value does not matter.
const uint8_t kAc3SetupData[] = {0x00, 0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08, 0x09};
}  // namespace

class ProgramMapTableWriterTest : public ::testing::Test {
 protected:
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
    ASSERT_EQ(suffix_size, actual_suffix.size());

    for (size_t i = 0; i < suffix_size; ++i) {
      EXPECT_EQ(suffix[i], actual_suffix[i]) << "at index " << i;
    }
  }
};

TEST_F(ProgramMapTableWriterTest, ClearH264) {
  VideoProgramMapTableWriter writer(kCodecH264);
  BufferWriter buffer;
  writer.ClearSegmentPmt(&buffer);

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

  ASSERT_EQ(kTsPacketSize, buffer.Size());
  EXPECT_NO_FATAL_FAILURE(
      ExpectTsPacketEqual(kExpectedPmtPrefix, kExpectedPmtPrefixSize, 160,
                          kPmtH264, arraysize(kPmtH264), buffer.Buffer()));
}

// Verify that PSI for encrypted segments after clear lead is generated
// correctly.
TEST_F(ProgramMapTableWriterTest, EncryptedSegmentsAfterClearLeadH264) {
  VideoProgramMapTableWriter writer(kCodecH264);
  BufferWriter buffer;
  writer.ClearSegmentPmt(&buffer);
  buffer.Clear();
  writer.EncryptedSegmentPmt(&buffer);
  EXPECT_EQ(kTsPacketSize, buffer.Size());

  const uint8_t kPmtEncryptedH264Prefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x31,  // Adaptation field and payload are both present. counter = 1.
      0x9B,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };

  const uint8_t kPmtEncryptedH264[] = {
      0x00,              // pointer field
      0x02,              // Table id.
      0xB0,              // The first 4 bits must be '1011'.
      0x18,              // length of the rest of this array.
      0x00, 0x01,        // program number.
      0xC3,              // version 1, current next indicator 1.
      0x00,              // section number
      0x00,              // last section number.
      0xE0,              // first 3 bits reserved.
      0x50,              // PCR PID is the elementary streams PID.
      0xF0,              // first 4 bits reserved.
      0x00,              // No descriptor at this level.
      0xDB, 0xE0, 0x50,  // stream_type -> PID.
      0xF0, 0x06,        // Es_info_length is 6 for private_data_indicator
      0x0F,              // descriptor_tag.
      0x04,              // Length of the rest of this descriptor
      0x7A, 0x61, 0x76, 0x63,  // 'zavc'.
      // CRC32.
      0xAF, 0xCC, 0x24, 0x21,
  };
  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kPmtEncryptedH264Prefix, arraysize(kPmtEncryptedH264Prefix), 154,
      kPmtEncryptedH264, arraysize(kPmtEncryptedH264), buffer.Buffer()));
}

// Verify that PMT for encrypted segments can be generated (without clear lead).
TEST_F(ProgramMapTableWriterTest, EncryptedSegmentsH264Pmt) {
  VideoProgramMapTableWriter writer(kCodecH264);
  BufferWriter buffer;
  writer.EncryptedSegmentPmt(&buffer);

  EXPECT_EQ(kTsPacketSize, buffer.Size());

  const uint8_t kPmtEncryptedH264Prefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0x9B,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };

  const uint8_t kPmtEncryptedH264[] = {
      0x00,              // pointer field
      0x02,              // Table id.
      0xB0,              // The first 4 bits must be '1011'.
      0x18,              // length of the rest of this array.
      0x00, 0x01,        // program number.
      0xC1,              // version 0, current next indicator 1.
      0x00,              // section number
      0x00,              // last section number.
      0xE0,              // first 3 bits reserved.
      0x50,              // PCR PID is the elementary streams PID.
      0xF0,              // first 4 bits reserved.
      0x00,              // No descriptor at this level.
      0xDB, 0xE0, 0x50,  // stream_type -> PID.
      0xF0, 0x06,        // Es_info_length is 6 for private_data_indicator
      0x0F,              // descriptor_tag.
      0x04,              // Length of the rest of this descriptor
      0x7A, 0x61, 0x76, 0x63,  // 'zavc'.
      // CRC32.
      0xA9, 0xC2, 0x95, 0x7C,
  };
  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kPmtEncryptedH264Prefix, arraysize(kPmtEncryptedH264Prefix), 154,
      kPmtEncryptedH264, arraysize(kPmtEncryptedH264), buffer.Buffer()));
}

TEST_F(ProgramMapTableWriterTest, ClearAac) {
  const std::vector<uint8_t> aac_audio_specific_config(
      std::begin(kAacBasicProfileExtraData),
      std::end(kAacBasicProfileExtraData));
  AudioProgramMapTableWriter writer(kCodecAAC, aac_audio_specific_config);
  BufferWriter buffer;
  writer.ClearSegmentPmt(&buffer);

  const uint8_t kExpectedPmtPrefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0xA1,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };
  const uint8_t kPmtAac[] = {
      0x00,                    // pointer field
      0x02,                    // table id must be 0x02.
      0xB0,                    // assumes length is <= 256 bytes.
      0x12,                    // length of the rest of this array.
      0x00, 0x01,              // program number.
      0xC1,                    // version 0, current next indicator 1.
      0x00,                    // section number
      0x00,                    // last section number.
      0xE0,                    // first 3 bits reserved.
      0x50,                    // PCR PID is the elementary streams PID.
      0xF0,                    // first 4 bits reserved.
      0x00,                    // No descriptor at this level.
      0x0F, 0xE0, 0x50,        // stream_type -> PID.
      0xF0, 0x00,              // Es_info_length is 0.
      0xE0, 0x6F, 0x1A, 0x31,  // CRC32.
  };
  EXPECT_NO_FATAL_FAILURE(
      ExpectTsPacketEqual(kExpectedPmtPrefix, arraysize(kExpectedPmtPrefix),
                          160, kPmtAac, arraysize(kPmtAac), buffer.Buffer()));
}

TEST_F(ProgramMapTableWriterTest, ClearAc3) {
  const std::vector<uint8_t> audio_specific_config(std::begin(kAc3SetupData),
                                                   std::end(kAc3SetupData));
  AudioProgramMapTableWriter writer(kCodecAC3, audio_specific_config);
  BufferWriter buffer;
  writer.ClearSegmentPmt(&buffer);

  const uint8_t kExpectedPmtPrefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0xA1,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };
  const uint8_t kPmtAc3[] = {
      0x00,                    // pointer field
      0x02,                    // table id must be 0x02.
      0xB0,                    // assumes length is <= 256 bytes.
      0x12,                    // length of the rest of this array.
      0x00, 0x01,              // program number.
      0xC1,                    // version 0, current next indicator 1.
      0x00,                    // section number
      0x00,                    // last section number.
      0xE0,                    // first 3 bits reserved.
      0x50,                    // PCR PID is the elementary streams PID.
      0xF0,                    // first 4 bits reserved.
      0x00,                    // No descriptor at this level.
      0x81, 0xE0, 0x50,        // stream_type -> PID.
      0xF0, 0x00,              // Es_info_length is 0.
      0x1E, 0xFC, 0x57, 0x12,  // CRC32.
  };

  EXPECT_NO_FATAL_FAILURE(
      ExpectTsPacketEqual(kExpectedPmtPrefix, arraysize(kExpectedPmtPrefix),
                          160, kPmtAc3, arraysize(kPmtAc3), buffer.Buffer()));
}

// Verify that PSI for encrypted segments after clear lead is generated
// correctly.
TEST_F(ProgramMapTableWriterTest, EncryptedSegmentsAfterClearLeadAac) {
  const std::vector<uint8_t> aac_audio_specific_config(
      std::begin(kAacBasicProfileExtraData),
      std::end(kAacBasicProfileExtraData));
  AudioProgramMapTableWriter writer(kCodecAAC, aac_audio_specific_config);
  BufferWriter buffer;
  writer.ClearSegmentPmt(&buffer);

  buffer.Clear();
  writer.EncryptedSegmentPmt(&buffer);
  EXPECT_EQ(kTsPacketSize, buffer.Size());

  const uint8_t kPmtEncryptedAacPrefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x31,  // Adaptation field and payload are both present. counter = 1.
      0x8B,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };
  const uint8_t kPmtEncryptedAac[] = {
      0x00,                    // pointer field
      0x02,                    // table id.
      0xB0,                    // The first 4 bits must be '1011'.
      0x28,                    // length of the rest of this array.
      0x00, 0x01,              // Program number.
      0xC3,                    // version 1, current next indicator 1.
      0x00,                    // section number
      0x00,                    // last section number.
      0xE0,                    // first 3 bits reserved.
      0x50,                    // PCR PID is the elementary streams PID.
      0xF0,                    // first 4 bits reserved.
      0x00,                    // No descriptor at this level.
      0xCF, 0xE0, 0x50,        // stream_type -> PID.
      0xF0, 0x16,              // Es_info_length is 22 for descriptors.
      0x0F,                    // private_data_indicator descriptor_tag.
      0x04,                    // Length of the rest of this descriptor
      0x61, 0x61, 0x63, 0x64,  // 'aacd'.
      0x05,                    // registration_descriptor tag.
      0x0E,  // space for 'zaac' + priming (0x0000) + version (0x01) +
             // setup_data_length size + size of kAacBasicProfileExtraData +
             // space for 'apad'. Which is 14.
      0x61, 0x70, 0x61, 0x64,  // 'apad'.
      0x7A, 0x61, 0x61, 0x63,  // 'zaac'.
      0x00, 0x00,              // priming.
      0x01,                    // version.
      0x02,                    // setup_data_length == extra data length
      0x12, 0x10,              // setup_data == extra data.
      0xC6, 0xB3, 0x31, 0x3A,  // CRC32.
  };

  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kPmtEncryptedAacPrefix, arraysize(kPmtEncryptedAacPrefix), 138,
      kPmtEncryptedAac, arraysize(kPmtEncryptedAac),
      buffer.Buffer()));
}

// Verify that PMT for encrypted segments can be generated (without clear lead).
TEST_F(ProgramMapTableWriterTest, EncryptedSegmentsAacPmt) {
  const std::vector<uint8_t> aac_audio_specific_config(
      std::begin(kAacBasicProfileExtraData),
      std::end(kAacBasicProfileExtraData));
  AudioProgramMapTableWriter writer(kCodecAAC, aac_audio_specific_config);
  BufferWriter buffer;
  writer.EncryptedSegmentPmt(&buffer);

  EXPECT_EQ(kTsPacketSize, buffer.Size());

  // Second PMT is for the encrypted segments after clear lead.
  const uint8_t kPmtEncryptedAacPrefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0x8B,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };
  const uint8_t kPmtEncryptedAac[] = {
      0x00,              // pointer field
      0x02,              // table id.
      0xB0,              // The first 4 bits must be '1011'.
      0x28,              // length of the rest of this array.
      0x00, 0x01,        // Program number.
      0xC1,              // version 0, current next indicator 1.
      0x00,              // section number
      0x00,              // last section number.
      0xE0,              // first 3 bits reserved.
      0x50,              // PCR PID is the elementary streams PID.
      0xF0,              // first 4 bits reserved.
      0x00,              // No descriptor at this level.
      0xCF, 0xE0, 0x50,  // stream_type -> PID.
      0xF0, 0x16,        // Es_info_length is 22 for private_data_indicator
      0x0F,              // private_data_indicator descriptor_tag.
      0x04,              // Length of the rest of this descriptor
      0x61, 0x61, 0x63, 0x64,  // 'aacd'.
      0x05,                    // registration_descriptor tag.
      0x0E,  // space for 'zaac' + priming (0x0000) + version (0x01) +
             // setup_data_length size + size of kAacBasicProfileExtraData +
             // space for 'apad'. Which is 14.
      0x61, 0x70, 0x61, 0x64,  // 'apad'.
      0x7A, 0x61, 0x61, 0x63,  // 'zaac'.
      0x00, 0x00,              // priming.
      0x01,                    // version.
      0x02,                    // setup_data_length == extra data length
      0x12, 0x10,              // setup_data == extra data.
      0xF7, 0xD5, 0x2A, 0x53,  // CRC32.
  };

  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kPmtEncryptedAacPrefix, arraysize(kPmtEncryptedAacPrefix), 138,
      kPmtEncryptedAac, arraysize(kPmtEncryptedAac),
      buffer.Buffer()));
}

TEST_F(ProgramMapTableWriterTest, EncryptedSegmentsAc3Pmt) {
  const std::vector<uint8_t> audio_specific_config(std::begin(kAc3SetupData),
                                                   std::end(kAc3SetupData));
  AudioProgramMapTableWriter writer(kCodecAC3, audio_specific_config);
  BufferWriter buffer;
  writer.EncryptedSegmentPmt(&buffer);

  EXPECT_EQ(kTsPacketSize, buffer.Size());

  // Second PMT is for the encrypted segments after clear lead.
  const uint8_t kPmtEncryptedAc3Prefix[] = {
      0x47,  // Sync byte.
      0x40,  // payload_unit_start_indicator set.
      0x20,  // pid.
      0x30,  // Adaptation field and payload are both present. counter = 0.
      0x83,  // Adaptation Field length.
      0x00,  // All adaptation field flags 0.
  };
  const uint8_t kPmtEncryptedAc3[] = {
      0x00,              // pointer field
      0x02,              // table id.
      0xB0,              // The first 4 bits must be '1011'.
      0x30,              // length of the rest of this array.
      0x00, 0x01,        // Program number.
      0xC1,              // version 0, current next indicator 1.
      0x00,              // section number
      0x00,              // last section number.
      0xE0,              // first 3 bits reserved.
      0x50,              // PCR PID is the elementary streams PID.
      0xF0,              // first 4 bits reserved.
      0x00,              // No descriptor at this level.
      0xC1, 0xE0, 0x50,  // stream_type -> PID.
      0xF0, 0x1E,        // Es_info_length is 30 for private_data_indicator
      0x0F,              // private_data_indicator descriptor_tag.
      0x04,              // Length of the rest of this descriptor
      0x61, 0x63, 0x33, 0x64,  // 'ac3d'.
      0x05,                    // registration_descriptor tag.
      0x16,  // space for 'zac3' + priming (0x0000) + version (0x01) +
             // setup_data_length size + size of kAc3SetupData + space for
             // 'apad'. Which is 22.
      0x61, 0x70, 0x61, 0x64,  // 'apad'.
      0x7A, 0x61, 0x63, 0x33,  // 'zac3'.
      0x00, 0x00,              // priming.
      0x01,                    // version.
      0x0A,                    // setup_data_length
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,  // setup_data
      0xCE, 0xB6, 0x52, 0x5C,                                      // CRC32.
  };

  EXPECT_NO_FATAL_FAILURE(ExpectTsPacketEqual(
      kPmtEncryptedAc3Prefix, arraysize(kPmtEncryptedAc3Prefix), 130,
      kPmtEncryptedAc3, arraysize(kPmtEncryptedAc3), buffer.Buffer()));
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
