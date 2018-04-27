// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "packager/media/base/bit_reader.h"

namespace shaka {
namespace media {

TEST(BitReaderTest, NormalOperationTest) {
  uint8_t value8;
  uint64_t value64;
  // 0101 0101 1001 1001 repeats 4 times
  uint8_t buffer[] = {0x55, 0x99, 0x55, 0x99, 0x55, 0x99, 0x55, 0x99};
  BitReader reader1(buffer, 6);  // Initialize with 6 bytes only

  EXPECT_TRUE(reader1.ReadBits(1, &value8));
  EXPECT_EQ(0, value8);
  EXPECT_TRUE(reader1.ReadBits(8, &value8));
  EXPECT_EQ(0xab, value8);  // 1010 1011
  EXPECT_EQ(39u, reader1.bits_available());
  EXPECT_EQ(9u, reader1.bit_position());
  EXPECT_TRUE(reader1.ReadBits(7, &value64));
  EXPECT_TRUE(reader1.ReadBits(32, &value64));
  EXPECT_EQ(0x55995599u, value64);
  EXPECT_FALSE(reader1.ReadBits(1, &value8));
  value8 = 0xff;
  EXPECT_TRUE(reader1.ReadBits(0, &value8));
  EXPECT_EQ(0, value8);

  BitReader reader2(buffer, 8);
  EXPECT_TRUE(reader2.ReadBits(64, &value64));
  EXPECT_EQ(0x5599559955995599ull, value64);
  EXPECT_FALSE(reader2.ReadBits(1, &value8));
  EXPECT_TRUE(reader2.ReadBits(0, &value8));
}

TEST(BitReaderTest, ReadBeyondEndTest) {
  uint8_t value8;
  uint8_t buffer[] = {0x12};
  BitReader reader1(buffer, sizeof(buffer));

  EXPECT_TRUE(reader1.ReadBits(4, &value8));
  EXPECT_FALSE(reader1.ReadBits(5, &value8));
  EXPECT_FALSE(reader1.ReadBits(1, &value8));
  EXPECT_TRUE(reader1.ReadBits(0, &value8));
}

TEST(BitReaderTest, SkipBitsTest) {
  uint8_t value8;
  uint8_t buffer[] = {0x0a, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  BitReader reader1(buffer, sizeof(buffer));

  EXPECT_TRUE(reader1.SkipBits(2));
  EXPECT_TRUE(reader1.ReadBits(3, &value8));
  EXPECT_EQ(1, value8);
  EXPECT_FALSE(reader1.SkipBytes(1));  // not aligned.
  EXPECT_TRUE(reader1.SkipBits(11));
  EXPECT_TRUE(reader1.ReadBits(8, &value8));
  EXPECT_EQ(3, value8);
  EXPECT_TRUE(reader1.SkipBytes(2));
  EXPECT_TRUE(reader1.SkipBytes(0));
  EXPECT_TRUE(reader1.SkipBytes(1));
  EXPECT_TRUE(reader1.SkipBits(52));
  EXPECT_EQ(20u, reader1.bits_available());
  EXPECT_EQ(100u, reader1.bit_position());
  EXPECT_TRUE(reader1.ReadBits(4, &value8));
  EXPECT_EQ(13, value8);
  EXPECT_FALSE(reader1.SkipBits(100));
  EXPECT_TRUE(reader1.SkipBits(0));
  EXPECT_FALSE(reader1.SkipBits(1));
}

TEST(BitReaderTest, SkipBitsConditionalTest) {
  uint8_t buffer[] = {0x8a, 0x12};
  BitReader reader(buffer, sizeof(buffer));
  EXPECT_TRUE(reader.SkipBitsConditional(false, 2));
  EXPECT_EQ(1u, reader.bit_position());  // Not skipped.
  EXPECT_TRUE(reader.SkipBitsConditional(false, 3));
  EXPECT_EQ(5u, reader.bit_position());  // Skipped.
  EXPECT_TRUE(reader.SkipBitsConditional(true, 2));
  EXPECT_EQ(6u, reader.bit_position());  // Not skipped.
  EXPECT_TRUE(reader.SkipBitsConditional(true, 5));
  EXPECT_EQ(12u, reader.bit_position());  // Skipped.
  EXPECT_TRUE(reader.SkipBits(4));
  EXPECT_FALSE(reader.SkipBits(1));
}

TEST(BitReaderTest, SkipToNextByteAligned) {
  uint8_t buffer[] = {0x8a, 0x12};
  BitReader reader(buffer, sizeof(buffer));

  reader.SkipToNextByte();
  EXPECT_EQ(0u, reader.bit_position());

  EXPECT_TRUE(reader.SkipBits(8));
  EXPECT_EQ(8u, reader.bit_position());

  reader.SkipToNextByte();
  EXPECT_EQ(8u, reader.bit_position());
}

TEST(BitReaderTest, SkipToNextByteTestUnaligned) {
  uint8_t buffer[] = {0x8a, 0x12};
  BitReader reader(buffer, sizeof(buffer));

  EXPECT_TRUE(reader.SkipBits(4));
  EXPECT_EQ(4u, reader.bit_position());

  reader.SkipToNextByte();
  EXPECT_EQ(8u, reader.bit_position());
}

}  // namespace media
}  // namespace shaka
