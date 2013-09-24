// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <sstream>
#include <vector>

#include "base/safe_numerics.h"

namespace base {
namespace internal {

// This is far (far, far) too slow to run normally, but if you're refactoring
// it might be useful.
// #define RUN_EXHAUSTIVE_TEST

#ifdef RUN_EXHAUSTIVE_TEST

template <class From, class To> void ExhaustiveCheckFromTo() {
  fprintf(stderr, ".");
  From i = std::numeric_limits<From>::min();
  for (;;) {
    std::ostringstream str_from, str_to;
    str_from << i;
    To to = static_cast<To>(i);
    str_to << to;
    bool strings_equal = str_from.str() == str_to.str();
    EXPECT_EQ(IsValidNumericCast<To>(i), strings_equal);
    fprintf(stderr, "\r%s vs %s\x1B[K",
        str_from.str().c_str(), str_to.str().c_str());
    ++i;
    // If we wrap, then we've tested everything.
    if (i == std::numeric_limits<From>::min())
      break;
  }
}

template <class From> void ExhaustiveCheckFrom() {
  ExhaustiveCheckFromTo<From, short>();
  ExhaustiveCheckFromTo<From, unsigned short>();
  ExhaustiveCheckFromTo<From, int>();
  ExhaustiveCheckFromTo<From, unsigned int>();
  ExhaustiveCheckFromTo<From, long long>();
  ExhaustiveCheckFromTo<From, unsigned long long>();
  ExhaustiveCheckFromTo<From, size_t>();
  fprintf(stderr, "\n");
}

#endif


TEST(SafeNumerics, NumericCast) {
  int small_positive = 1;
  int small_negative = -1;
  int large_positive = INT_MAX;
  int large_negative = INT_MIN;
  size_t size_t_small = 1;
  size_t size_t_large = UINT_MAX;

  // Narrow signed destination.
  EXPECT_TRUE(IsValidNumericCast<signed char>(small_positive));
  EXPECT_TRUE(IsValidNumericCast<signed char>(small_negative));
  EXPECT_FALSE(IsValidNumericCast<signed char>(large_positive));
  EXPECT_FALSE(IsValidNumericCast<signed char>(large_negative));
  EXPECT_TRUE(IsValidNumericCast<signed short>(small_positive));
  EXPECT_TRUE(IsValidNumericCast<signed short>(small_negative));

  // Narrow unsigned destination.
  EXPECT_TRUE(IsValidNumericCast<unsigned char>(small_positive));
  EXPECT_FALSE(IsValidNumericCast<unsigned char>(small_negative));
  EXPECT_FALSE(IsValidNumericCast<unsigned char>(large_positive));
  EXPECT_FALSE(IsValidNumericCast<unsigned char>(large_negative));
  EXPECT_FALSE(IsValidNumericCast<unsigned short>(small_negative));
  EXPECT_FALSE(IsValidNumericCast<unsigned short>(large_negative));

  // Same width signed destination.
  EXPECT_TRUE(IsValidNumericCast<signed int>(small_positive));
  EXPECT_TRUE(IsValidNumericCast<signed int>(small_negative));
  EXPECT_TRUE(IsValidNumericCast<signed int>(large_positive));
  EXPECT_TRUE(IsValidNumericCast<signed int>(large_negative));

  // Same width unsigned destination.
  EXPECT_TRUE(IsValidNumericCast<unsigned int>(small_positive));
  EXPECT_FALSE(IsValidNumericCast<unsigned int>(small_negative));
  EXPECT_TRUE(IsValidNumericCast<unsigned int>(large_positive));
  EXPECT_FALSE(IsValidNumericCast<unsigned int>(large_negative));

  // Wider signed destination.
  EXPECT_TRUE(IsValidNumericCast<long long>(small_positive));
  EXPECT_TRUE(IsValidNumericCast<long long>(large_negative));
  EXPECT_TRUE(IsValidNumericCast<long long>(small_positive));
  EXPECT_TRUE(IsValidNumericCast<long long>(large_negative));

  // Wider unsigned destination.
  EXPECT_TRUE(IsValidNumericCast<unsigned long long>(small_positive));
  EXPECT_FALSE(IsValidNumericCast<unsigned long long>(small_negative));
  EXPECT_TRUE(IsValidNumericCast<unsigned long long>(large_positive));
  EXPECT_FALSE(IsValidNumericCast<unsigned long long>(large_negative));

  // Negative to size_t.
  EXPECT_FALSE(IsValidNumericCast<size_t>(small_negative));
  EXPECT_FALSE(IsValidNumericCast<size_t>(large_negative));

  // From unsigned.
  // Small.
  EXPECT_TRUE(IsValidNumericCast<signed char>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<unsigned char>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<short>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<unsigned short>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<int>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<unsigned int>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<long long>(size_t_small));
  EXPECT_TRUE(IsValidNumericCast<unsigned long long>(size_t_small));

  // Large.
  EXPECT_FALSE(IsValidNumericCast<signed char>(size_t_large));
  EXPECT_FALSE(IsValidNumericCast<unsigned char>(size_t_large));
  EXPECT_FALSE(IsValidNumericCast<short>(size_t_large));
  EXPECT_FALSE(IsValidNumericCast<unsigned short>(size_t_large));
  EXPECT_FALSE(IsValidNumericCast<int>(size_t_large));
  EXPECT_TRUE(IsValidNumericCast<unsigned int>(size_t_large));
  EXPECT_TRUE(IsValidNumericCast<long long>(size_t_large));
  EXPECT_TRUE(IsValidNumericCast<unsigned long long>(size_t_large));

  // Various edge cases.
  EXPECT_TRUE(IsValidNumericCast<int>(static_cast<short>(SHRT_MIN)));
  EXPECT_FALSE(
      IsValidNumericCast<unsigned short>(static_cast<short>(SHRT_MIN)));
  EXPECT_FALSE(IsValidNumericCast<unsigned short>(SHRT_MIN));

  // Confirm that checked_numeric_cast<> actually compiles.
  std::vector<int> v;
  unsigned int checked_size =
      base::checked_numeric_cast<unsigned int>(v.size());
  EXPECT_EQ(0u, checked_size);

#ifdef RUN_EXHAUSTIVE_TEST
  ExhaustiveCheckFrom<short>();
  ExhaustiveCheckFrom<unsigned short>();
  ExhaustiveCheckFrom<int>();
  ExhaustiveCheckFrom<unsigned int>();
  ExhaustiveCheckFrom<long long>();
  ExhaustiveCheckFrom<unsigned long long>();
  ExhaustiveCheckFrom<size_t>();
#endif
}

}  // namespace internal
}  // namespace base
