// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/base/bit_writer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::ElementsAreArray;

namespace shaka {
namespace media {

TEST(BitWriterTest, Simple) {
  std::vector<uint8_t> storage;
  BitWriter writer(&storage);
  writer.WriteBits(1, 1);
  EXPECT_EQ(1u, writer.BitPos());
  EXPECT_EQ(0u, writer.BytePos());
  writer.Flush();
  // Bits are byte-aligned after flush.
  EXPECT_EQ(8u, writer.BitPos());
  EXPECT_EQ(1u, writer.BytePos());

  EXPECT_THAT(storage, ElementsAreArray({0x80}));
}

TEST(BitWriterTest, Test) {
  std::vector<uint8_t> storage;
  BitWriter writer(&storage);
  writer.WriteBits(0, 1);
  EXPECT_EQ(1u, writer.BitPos());
  EXPECT_EQ(0u, writer.BytePos());
  writer.WriteBits(0xab, 8);
  EXPECT_EQ(9u, writer.BitPos());
  EXPECT_EQ(1u, writer.BytePos());
  writer.WriteBits(0x34, 6);
  EXPECT_EQ(15u, writer.BitPos());
  EXPECT_EQ(1u, writer.BytePos());
  writer.WriteBits(0x55995599, 32);
  EXPECT_EQ(47u, writer.BitPos());
  EXPECT_EQ(5u, writer.BytePos());
  writer.WriteBits(1, 1);
  EXPECT_EQ(48u, writer.BitPos());
  EXPECT_EQ(6u, writer.BytePos());
  writer.WriteBits(0x13, 21);
  EXPECT_EQ(69u, writer.BitPos());
  EXPECT_EQ(8u, writer.BytePos());
  writer.Flush();
  // Bits are byte-aligned after flush.
  EXPECT_EQ(72u, writer.BitPos());
  EXPECT_EQ(9u, writer.BytePos());

  EXPECT_THAT(storage, ElementsAreArray({0x55, 0xe8, 0xab, 0x32, 0xab, 0x33,
                                         0x00, 0x00, 0x98}));
}

}  // namespace media
}  // namespace shaka
