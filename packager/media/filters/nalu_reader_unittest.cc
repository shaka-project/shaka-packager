// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/filters/nalu_reader.h"

namespace edash_packager {
namespace media {

TEST(NaluReaderTest, StartCodeSearch) {
  const uint8_t kNaluData[] = {
      0x01, 0x00, 0x00, 0x04, 0x23, 0x56,
      // First NALU
      0x00, 0x00, 0x01, 0x14, 0x34, 0x56, 0x78,
      // Second NALU
      0x00, 0x00, 0x00, 0x01, 0x67, 0xbb, 0xcc, 0xdd,
  };

  NaluReader reader(NaluReader::kH264, kIsAnnexbByteStream, kNaluData,
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

  NaluReader reader(NaluReader::kH264, kIsAnnexbByteStream, kNaluData,
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

  NaluReader reader(NaluReader::kH264, 1, kNaluData, arraysize(kNaluData));

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

  NaluReader reader(NaluReader::kH264, 4, kNaluData, arraysize(kNaluData));

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

  NaluReader reader(NaluReader::kH264, 3, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  EXPECT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForNaluLengthExceedsRemainingData) {
  const uint8_t kNaluData[] = {
      // First NALU
      0xFF, 0x08, 0x00,
  };

  NaluReader reader(NaluReader::kH264, 1, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  EXPECT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));

  // Another test for off by one.
  const uint8_t kNaluData2[] = {
      // First NALU
      0x04, 0x08, 0x00, 0x00,
  };

  NaluReader reader2(NaluReader::kH264, 1, kNaluData2, arraysize(kNaluData2));
  EXPECT_EQ(NaluReader::kInvalidStream, reader2.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForForbiddenBitSet) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x03, 0x80, 0x00, 0x00,
  };

  NaluReader reader(NaluReader::kH264, 1, kNaluData, arraysize(kNaluData));

  Nalu nalu;
  EXPECT_EQ(NaluReader::kInvalidStream, reader.Advance(&nalu));
}

TEST(NaluReaderTest, ErrorForZeroSize) {
  const uint8_t kNaluData[] = {
      // First NALU
      0x03, 0x80, 0x00, 0x00,
  };

  Nalu nalu;
  EXPECT_FALSE(nalu.InitializeFromH264(kNaluData, 0));
  EXPECT_FALSE(nalu.InitializeFromH265(kNaluData, 0));
}

}  // namespace media
}  // namespace edash_packager
