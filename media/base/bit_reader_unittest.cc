// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bit_reader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(BitReaderTest, NormalOperationTest) {
  uint8 value8;
  uint64 value64;
  // 0101 0101 1001 1001 repeats 4 times
  uint8 buffer[] = {0x55, 0x99, 0x55, 0x99, 0x55, 0x99, 0x55, 0x99};
  BitReader reader1(buffer, 6);  // Initialize with 6 bytes only

  EXPECT_TRUE(reader1.ReadBits(1, &value8));
  EXPECT_EQ(value8, 0);
  EXPECT_TRUE(reader1.ReadBits(8, &value8));
  EXPECT_EQ(value8, 0xab);  // 1010 1011
  EXPECT_TRUE(reader1.ReadBits(7, &value64));
  EXPECT_TRUE(reader1.ReadBits(32, &value64));
  EXPECT_EQ(value64, 0x55995599u);
  EXPECT_FALSE(reader1.ReadBits(1, &value8));
  value8 = 0xff;
  EXPECT_TRUE(reader1.ReadBits(0, &value8));
  EXPECT_EQ(value8, 0);

  BitReader reader2(buffer, 8);
  EXPECT_TRUE(reader2.ReadBits(64, &value64));
  EXPECT_EQ(value64, 0x5599559955995599ull);
  EXPECT_FALSE(reader2.ReadBits(1, &value8));
  EXPECT_TRUE(reader2.ReadBits(0, &value8));
}

TEST(BitReaderTest, ReadBeyondEndTest) {
  uint8 value8;
  uint8 buffer[] = {0x12};
  BitReader reader1(buffer, sizeof(buffer));

  EXPECT_TRUE(reader1.ReadBits(4, &value8));
  EXPECT_FALSE(reader1.ReadBits(5, &value8));
  EXPECT_FALSE(reader1.ReadBits(1, &value8));
  EXPECT_TRUE(reader1.ReadBits(0, &value8));
}

TEST(BitReaderTest, SkipBitsTest) {
  uint8 value8;
  uint8 buffer[] = { 0x0a, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
  BitReader reader1(buffer, sizeof(buffer));

  EXPECT_TRUE(reader1.SkipBits(2));
  EXPECT_TRUE(reader1.ReadBits(3, &value8));
  EXPECT_EQ(value8, 1);
  EXPECT_TRUE(reader1.SkipBits(11));
  EXPECT_TRUE(reader1.ReadBits(8, &value8));
  EXPECT_EQ(value8, 3);
  EXPECT_TRUE(reader1.SkipBits(76));
  EXPECT_TRUE(reader1.ReadBits(4, &value8));
  EXPECT_EQ(value8, 13);
  EXPECT_FALSE(reader1.SkipBits(100));
  EXPECT_TRUE(reader1.SkipBits(0));
  EXPECT_FALSE(reader1.SkipBits(1));
}

}  // namespace media
