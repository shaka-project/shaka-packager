// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/media/codecs/h26x_bit_reader.h"

namespace shaka {
namespace media {

TEST(H26xBitReaderTest, ReadStreamWithoutEscapeAndTrailingZeroBytes) {
  H26xBitReader reader;
  const unsigned char rbsp[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xa0};
  int dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 0x00);
  EXPECT_EQ(reader.NumBitsLeft(), 47);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(8, &dummy));
  EXPECT_EQ(dummy, 0x02);
  EXPECT_EQ(reader.NumBitsLeft(), 39);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(31, &dummy));
  EXPECT_EQ(dummy, 0x23456789);
  EXPECT_EQ(reader.NumBitsLeft(), 8);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 1);
  EXPECT_EQ(reader.NumBitsLeft(), 7);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(1, &dummy));
  EXPECT_EQ(dummy, 0);
  EXPECT_EQ(reader.NumBitsLeft(), 6);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H26xBitReaderTest, ReadPpsWithTrailingZeroByte) {
  H26xBitReader reader;

  // Data copied from https://github.com/google/shaka-packager/issues/418.
  const unsigned char pps_rbsp[] = {0xee, 0x3c, 0x80, 0x00};
  EXPECT_TRUE(reader.Initialize(pps_rbsp, sizeof(pps_rbsp)));

  // Skips all the fields in PPS (kind of simulates ParsePps).
  EXPECT_TRUE(reader.SkipBits(16));

  EXPECT_EQ(reader.NumBitsLeft(), 16);
  // The remaining data is '80 00'. The trailing null byte is ignored. There
  // are no bits before the stop bit, so there is no more RBSP data.
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H26xBitReaderTest, SingleByteStream) {
  H26xBitReader reader;
  const unsigned char rbsp[] = {0x18};
  int dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));
  EXPECT_EQ(reader.NumBitsLeft(), 8);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(4, &dummy));
  EXPECT_EQ(dummy, 0x01);
  EXPECT_EQ(reader.NumBitsLeft(), 4);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

TEST(H26xBitReaderTest, ReadBool) {
  H26xBitReader reader;
  const unsigned char rbsp[] = {0xc5};
  bool dummy = false;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));
  EXPECT_EQ(reader.NumBitsLeft(), 8);

  EXPECT_TRUE(reader.ReadBool(&dummy));
  EXPECT_TRUE(dummy);
  EXPECT_TRUE(reader.ReadBool(&dummy));
  EXPECT_TRUE(dummy);
  EXPECT_TRUE(reader.ReadBool(&dummy));
  EXPECT_FALSE(dummy);
  EXPECT_TRUE(reader.ReadBool(&dummy));
  EXPECT_FALSE(dummy);

  EXPECT_EQ(reader.NumBitsLeft(), 4);
}

TEST(H26xBitReaderTest, SkipBits) {
  H26xBitReader reader;
  const unsigned char rbsp[] = {0xc5, 0x41, 0x51};
  int dummy;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));
  EXPECT_EQ(reader.NumBitsLeft(), 24);

  EXPECT_TRUE(reader.SkipBits(3));
  EXPECT_EQ(21, reader.NumBitsLeft());
  EXPECT_TRUE(reader.ReadBits(4, &dummy));
  EXPECT_EQ(0x2, dummy);
  EXPECT_TRUE(reader.SkipBits(8));
  EXPECT_EQ(9, reader.NumBitsLeft());
  EXPECT_TRUE(reader.ReadBits(5, &dummy));
  EXPECT_EQ(0x15, dummy);
  EXPECT_EQ(4, reader.NumBitsLeft());
  EXPECT_FALSE(reader.SkipBits(5));
  EXPECT_TRUE(reader.SkipBits(0));
  EXPECT_EQ(4, reader.NumBitsLeft());
}

TEST(H26xBitReaderTest, StopBitOccupyFullByte) {
  H26xBitReader reader;
  const unsigned char rbsp[] = {0xab, 0x80};
  int dummy = 0;

  EXPECT_TRUE(reader.Initialize(rbsp, sizeof(rbsp)));
  EXPECT_EQ(reader.NumBitsLeft(), 16);
  EXPECT_TRUE(reader.HasMoreRBSPData());

  EXPECT_TRUE(reader.ReadBits(8, &dummy));
  EXPECT_EQ(dummy, 0xab);
  EXPECT_EQ(reader.NumBitsLeft(), 8);
  EXPECT_FALSE(reader.HasMoreRBSPData());
}

}  // namespace media
}  // namespace shaka
