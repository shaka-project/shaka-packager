// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

static const size_t kNpos = string16::npos;

}  // namespace

TEST(UTFOffsetStringConversionsTest, AdjustOffset) {
  struct UTF8ToUTF16Case {
    const char* utf8;
    size_t input_offset;
    size_t output_offset;
  } utf8_to_utf16_cases[] = {
    {"", 0, kNpos},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", 1, kNpos},
    {"\xe4\xbd\xa0\xe5\xa5\xbd", 3, 1},
    {"\xed\xb0\x80z", 3, 1},
    {"A\xF0\x90\x8C\x80z", 1, 1},
    {"A\xF0\x90\x8C\x80z", 2, kNpos},
    {"A\xF0\x90\x8C\x80z", 5, 3},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(utf8_to_utf16_cases); ++i) {
    size_t offset = utf8_to_utf16_cases[i].input_offset;
    UTF8ToUTF16AndAdjustOffset(utf8_to_utf16_cases[i].utf8, &offset);
    EXPECT_EQ(utf8_to_utf16_cases[i].output_offset, offset);
  }

  struct UTF16ToUTF8Case {
    char16 utf16[10];
    size_t input_offset;
    size_t output_offset;
  } utf16_to_utf8_cases[] = {
      {{}, 0, kNpos},
      // Converted to 3-byte utf-8 sequences
      {{0x5909, 0x63DB}, 2, kNpos},
      {{0x5909, 0x63DB}, 1, 3},
      // Converted to 2-byte utf-8 sequences
      {{'A', 0x00bc, 0x00be, 'z'}, 1, 1},
      {{'A', 0x00bc, 0x00be, 'z'}, 2, 3},
      {{'A', 0x00bc, 0x00be, 'z'}, 3, 5},
      // Surrogate pair
      {{'A', 0xd800, 0xdf00, 'z'}, 1, 1},
      {{'A', 0xd800, 0xdf00, 'z'}, 2, kNpos},
      {{'A', 0xd800, 0xdf00, 'z'}, 3, 5},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(utf16_to_utf8_cases); ++i) {
    size_t offset = utf16_to_utf8_cases[i].input_offset;
    UTF16ToUTF8AndAdjustOffset(utf16_to_utf8_cases[i].utf16, &offset);
    EXPECT_EQ(utf16_to_utf8_cases[i].output_offset, offset);
  }
}

TEST(UTFOffsetStringConversionsTest, LimitOffsets) {
  const size_t kLimit = 10;
  const size_t kItems = 20;
  std::vector<size_t> size_ts;
  for (size_t t = 0; t < kItems; ++t)
    size_ts.push_back(t);
  std::for_each(size_ts.begin(), size_ts.end(),
                LimitOffset<string16>(kLimit));
  size_t unlimited_count = 0;
  for (std::vector<size_t>::iterator ti = size_ts.begin(); ti != size_ts.end();
       ++ti) {
    if (*ti < kLimit && *ti != kNpos)
      ++unlimited_count;
  }
  EXPECT_EQ(10U, unlimited_count);

  // Reverse the values in the vector and try again.
  size_ts.clear();
  for (size_t t = kItems; t > 0; --t)
    size_ts.push_back(t - 1);
  std::for_each(size_ts.begin(), size_ts.end(),
                LimitOffset<string16>(kLimit));
  unlimited_count = 0;
  for (std::vector<size_t>::iterator ti = size_ts.begin(); ti != size_ts.end();
       ++ti) {
    if (*ti < kLimit && *ti != kNpos)
      ++unlimited_count;
  }
  EXPECT_EQ(10U, unlimited_count);
}

TEST(UTFOffsetStringConversionsTest, AdjustOffsets) {
  // Imagine we have strings as shown in the following cases where the
  // X's represent encoded characters.
  // 1: abcXXXdef ==> abcXdef
  {
    std::vector<size_t> offsets;
    for (size_t t = 0; t < 9; ++t)
      offsets.push_back(t);
    {
      OffsetAdjuster offset_adjuster(&offsets);
      offset_adjuster.Add(OffsetAdjuster::Adjustment(3, 3, 1));
    }
    size_t expected_1[] = {0, 1, 2, 3, kNpos, kNpos, 4, 5, 6};
    EXPECT_EQ(offsets.size(), arraysize(expected_1));
    for (size_t i = 0; i < arraysize(expected_1); ++i)
      EXPECT_EQ(expected_1[i], offsets[i]);
  }

  // 2: XXXaXXXXbcXXXXXXXdefXXX ==> XaXXbcXXXXdefX
  {
    std::vector<size_t> offsets;
    for (size_t t = 0; t < 23; ++t)
      offsets.push_back(t);
    {
      OffsetAdjuster offset_adjuster(&offsets);
      offset_adjuster.Add(OffsetAdjuster::Adjustment(0, 3, 1));
      offset_adjuster.Add(OffsetAdjuster::Adjustment(4, 4, 2));
      offset_adjuster.Add(OffsetAdjuster::Adjustment(10, 7, 4));
      offset_adjuster.Add(OffsetAdjuster::Adjustment(20, 3, 1));
    }
    size_t expected_2[] = {0, kNpos, kNpos, 1, 2, kNpos, kNpos, kNpos, 4, 5, 6,
                           kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 10, 11, 12,
                           13, kNpos, kNpos};
    EXPECT_EQ(offsets.size(), arraysize(expected_2));
    for (size_t i = 0; i < arraysize(expected_2); ++i)
      EXPECT_EQ(expected_2[i], offsets[i]);
  }

  // 3: XXXaXXXXbcdXXXeXX ==> aXXXXbcdXXXe
  {
    std::vector<size_t> offsets;
    for (size_t t = 0; t < 17; ++t)
      offsets.push_back(t);
    {
      OffsetAdjuster offset_adjuster(&offsets);
      offset_adjuster.Add(OffsetAdjuster::Adjustment(0, 3, 0));
      offset_adjuster.Add(OffsetAdjuster::Adjustment(4, 4, 4));
      offset_adjuster.Add(OffsetAdjuster::Adjustment(11, 3, 3));
      offset_adjuster.Add(OffsetAdjuster::Adjustment(15, 2, 0));
    }
    size_t expected_3[] = {kNpos, kNpos, kNpos, 0, 1, kNpos, kNpos, kNpos, 5, 6,
                           7, 8, kNpos, kNpos, 11, kNpos, kNpos};
    EXPECT_EQ(offsets.size(), arraysize(expected_3));
    for (size_t i = 0; i < arraysize(expected_3); ++i)
      EXPECT_EQ(expected_3[i], offsets[i]);
  }
}

}  // namaspace base
