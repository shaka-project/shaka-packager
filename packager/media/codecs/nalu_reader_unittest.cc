// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/codecs/nalu_reader.h"

namespace shaka {
namespace media {

TEST(NaluReaderTest, StartCodeSearch) {
  const uint8_t kNaluData[] = {
      0x01, 0x00, 0x00, 0x04, 0x23, 0x56,
      // First NALU
      0x00, 0x00, 0x01, 0x14, 0x34, 0x56, 0x78,
      // Second NALU
      0x00, 0x00, 0x00, 0x01, 0x67, 0xbb, 0xcc, 0xdd,
  };

  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, kNaluData,
                    arraysize(kNaluData));

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 9, nalu.data());
  EXPECT_EQ(3u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());

  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 17, nalu.data());
  EXPECT_EQ(3u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(3, nalu.ref_idc());
  EXPECT_EQ(7, nalu.type());

  EXPECT_EQ(NaluReader::kEOStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, StartCodeSearchWithStartCodeInsideNalUnit) {
  const uint8_t kNaluData[] = {
      0x01, 0x00, 0x00, 0x04, 0x23, 0x56,
      // First NALU
      0x00, 0x00, 0x01, 0x14, 0x34, 0x56, 0x78,
      // This is part of the first NALU as it is not a valid NALU.
      0x00, 0x00, 0x00, 0x01, 0x07, 0xbb, 0xcc, 0xdd,
      // Second NALU
      0x00, 0x00, 0x01, 0x67, 0x03, 0x04,
      // This is part of the second NALU.
      0x00, 0x00, 0x01,
  };

  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, kNaluData,
                    arraysize(kNaluData));

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 9, nalu.data());
  EXPECT_EQ(11u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());

  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 24, nalu.data());
  EXPECT_EQ(5u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(3, nalu.ref_idc());
  EXPECT_EQ(7, nalu.type());

  EXPECT_EQ(NaluReader::kEOStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, OneByteNaluLength) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x05, 0x06, 0x01, 0x02, 0x03, 0x04,
      // Second NALU
      0x06, 0x67, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
  };

  NaluReader reader(Nalu::kH264, 1, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 1, nalu.data());
  EXPECT_EQ(4u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(6, nalu.type());

  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 7, nalu.data());
  EXPECT_EQ(5u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(3, nalu.ref_idc());
  EXPECT_EQ(7, nalu.type());

  EXPECT_EQ(NaluReader::kEOStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, FourByteNaluLength) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x00, 0x00, 0x00, 0x07, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
      // Second NALU
      0x00, 0x00, 0x00, 0x03, 0x67, 0x0a, 0x0b,
  };

  NaluReader reader(Nalu::kH264, 4, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 4, nalu.data());
  EXPECT_EQ(6u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(6, nalu.type());

  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 15, nalu.data());
  EXPECT_EQ(2u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(3, nalu.ref_idc());
  EXPECT_EQ(7, nalu.type());

  EXPECT_EQ(NaluReader::kEOStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForNotEnoughForNaluLength) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x00,
  };

  NaluReader reader(Nalu::kH264, 3, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  EXPECT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForNaluLengthExceedsRemainingData) {
  const uint8_t kNaluData[] = {
      // First NALU
      0xFF, 0x08, 0x00,
  };

  NaluReader reader(Nalu::kH264, 1, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  EXPECT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));

  // Another test for off by one.
  const uint8_t kNaluData2[] = {
      // First NALU
      0x04, 0x08, 0x00, 0x00,
  };

  NaluReader reader2(Nalu::kH264, 1, kNaluData2, arraysize(kNaluData2));
  EXPECT_EQ(NaluReader::kInvalidStream, reader2.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForForbiddenBitSet) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x03, 0x80, 0x00, 0x00,
  };

  NaluReader reader(Nalu::kH264, 1, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  EXPECT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForZeroSize) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x03, 0x80, 0x00, 0x00,
  };

  Nalu nalu;
  EXPECT_FALSE(nalu.Initialize(Nalu::kH264, kNaluData, 0));
  EXPECT_FALSE(nalu.Initialize(Nalu::kH265, kNaluData, 0));
}

TEST(NaluReaderTest, SubsamplesAnnexB) {
  const uint8_t kNaluData[] = {
      // This array contains 1 nalu starting with a NALU start code.
      // what looks like NALU start codes below are "encrypted" portion.
      0x00, 0x00, 0x01, 0x14,
      // This is in the encrypted portion and none of the following sequence
      // should be recognized as a NALU start code.
      0x00, 0x00, 0x01, 0x65, 0x00, 0x00, 0x00, 0x01, 0x67,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(4, 9));
  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, kNaluData,
                    arraysize(kNaluData), subsamples);

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 3, nalu.data());
  EXPECT_EQ(9u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());
}

TEST(NaluReaderTest, MultiSubsamplesAnnexB) {
  const uint8_t kNaluData[] = {
      // Clear
      0x00,
      // Encrypted. Should not recognize this as a NALU start code.
      0x00, 0x01, 0x14,
      // Clear. Valid NALU start code + NALU header.
      0x00, 0x00, 0x01, 0x65,
      // Encrypted.
      0x00, 0x00, 0x00, 0x01, 0x67,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(1, 3));
  subsamples.emplace_back(SubsampleEntry(4, 5));
  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, kNaluData,
                    arraysize(kNaluData), subsamples);

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 7, nalu.data());
  EXPECT_EQ(5u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(3, nalu.ref_idc());
  EXPECT_EQ(5, nalu.type());
}

// Verify that data outside subsamples is treated as clear data.
TEST(NaluReaderTest, BufferBiggerThanSubsamplesAnnexB) {
  const uint8_t kNaluData[] = {
      // This array contains 1 nalu starting with a NALU start code.
      // what looks like NALU start codes below are "encrypted" portion.
      0x00, 0x00, 0x01, 0x14,
      // This is in the encrypted portion and none of the following sequence
      // should be recognized as a NALU start code.
      0x00, 0x00, 0x01, 0x65, 0x00, 0x00, 0x00, 0x01, 0x67,
      // Start of second NALU not specified by subsamples.
      0x00, 0x00, 0x00, 0x01, 0x67, 0xbb, 0xcc, 0xdd,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(4, 9));
  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, kNaluData,
                    arraysize(kNaluData), subsamples);

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 3, nalu.data());
  EXPECT_EQ(9u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());

  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 17, nalu.data());
  EXPECT_EQ(3u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(3, nalu.ref_idc());
  EXPECT_EQ(7, nalu.type());
}

// Finds a NALU start code + header in the clear section but is an invalid NALU.
TEST(NaluReaderTest, SubsamplesWithInvalidNalu) {
  const uint8_t kNaluData[] = {
      // Start with a valid NALU.
      // Clear.
      0x00, 0x00, 0x01, 0x14,
      // Encrypted.
      0x00, 0x00,
      // Clear. Has NALU start code but invalid NALU.
      0x00, 0x00, 0x01, 0x80,
      // Encrypted.
      0x00, 0x04, 0x03,
      // Clear.
      0x00, 0xFE,
      // Encrypted.
      0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0x00, 0x01,
      // Clear. Valid NALU. The first NALU should end here.
      // If subsamples is not updated correctly the parser won't recognize that
      // this is a NALU start code.
      0x00, 0x00, 0x01, 0x65,
      // Encrypted.
      0xEE, 0xCE, 0x12, 0x44,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(4, 2));
  subsamples.emplace_back(SubsampleEntry(4, 3));
  subsamples.emplace_back(SubsampleEntry(2, 8));
  subsamples.emplace_back(SubsampleEntry(4, 4));

  NaluReader reader(Nalu::kH264, kIsAnnexbByteStream, kNaluData,
                    arraysize(kNaluData), subsamples);

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(19u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());
}

// No NALU start code in the subsample range. A NALU start code in the buffer
// not specified by subsamples.
TEST(NaluReaderTest, FindStartCodeInClearRangeNoNalu) {
  const uint8_t kNaluData[] = {
      // Any sequence not NALU start code in the subsample region.
      0xFF, 0xFE, 0xFD, 0xFC,
      // End of subsample specified region. No NALU start code.
      0x00, 0x04, 0x03, 0x14, 0x34, 0x56, 0x78,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(2, 2));

  uint64_t offset = 0;
  uint8_t start_code_size = 0;
  EXPECT_FALSE(NaluReader::FindStartCodeInClearRange(
      kNaluData, arraysize(kNaluData), &offset, &start_code_size, subsamples));
  EXPECT_GT(offset, 4u)
      << "Expect at least the subsample region should be consumed.";
}

// If subsamples goes beyond the data size and cannot find a NALU start code,
// |offset| should not be set to the end of the subsamples. Instead it should be
// less than or equal to the size of the data as documented in the header.
TEST(NaluReaderTest, FindStartCodeInClearRangeSubsamplesBiggerThanBuffer) {
  const uint8_t kNaluData[] = {
    // The data in here doesn't really matter.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(1, 14));

  uint64_t offset;
  uint8_t start_code_size;
  EXPECT_FALSE(NaluReader::FindStartCodeInClearRange(
      kNaluData, arraysize(kNaluData), &offset, &start_code_size, subsamples));
  EXPECT_LE(offset, arraysize(kNaluData));
}

// Verify that it doesn't affect the Nalu stream mode too much.
TEST(NaluReaderTest, SubsamplesNaluStream) {
  const uint8_t kNaluData[] = {
      // This array contains 1 nalu starting with a 1 byte NALU length size.
      0x0A, 0x14,
      // This is in the encrypted portion and none of the following sequence
      // should be recognized as a NALU start code.
      0x00, 0x00, 0x01, 0x65, 0x00, 0x00, 0x00, 0x01, 0x67,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(2, 9));
  NaluReader reader(Nalu::kH264, 1, kNaluData,
                    arraysize(kNaluData), subsamples);

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 1, nalu.data());
  EXPECT_EQ(9u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());
}

// Verify that if NALU length is encrypted, NALUs cannot be parsed.
TEST(NaluReaderTest, EncryptedNaluLengthNaluStream) {
  const uint8_t kNaluData[] = {
      // This array contains 1 nalu starting with a 1 byte NALU length size.
      0x00, 0x0A, 0x14,
      // This is in the encrypted portion and none of the following sequence
      // should be recognized as a NALU start code.
      0x00, 0x00, 0x01, 0x65, 0x00, 0x00, 0x00, 0x01, 0x67,
      // Second NALU is supposed to start here but the second byte of the length
      // is encrypted.
      0x00, 0xFF, 0xFF,
  };

  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(SubsampleEntry(3, 9));
  subsamples.emplace_back(SubsampleEntry(1, 2));
  NaluReader reader(Nalu::kH264, 2, kNaluData,
                    arraysize(kNaluData), subsamples);

  Nalu nalu;
  ASSERT_EQ(NaluReader::kOk, reader.Advance(&nalu));
  EXPECT_EQ(kNaluData + 2, nalu.data());
  EXPECT_EQ(9u, nalu.payload_size());
  EXPECT_EQ(1u, nalu.header_size());
  EXPECT_EQ(0, nalu.ref_idc());
  EXPECT_EQ(0x14, nalu.type());

  ASSERT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));
}

}  // namespace media
}  // namespace shaka
