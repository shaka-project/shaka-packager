// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/hash_tables.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HashPairTest : public testing::Test {
};

#define INSERT_PAIR_TEST(Type, value1, value2) \
  { \
    Type pair(value1, value2); \
    base::hash_map<Type, int> map; \
    map[pair] = 1; \
  }

// Verify that a hash_map can be constructed for pairs of integers of various
// sizes.
TEST_F(HashPairTest, IntegerPairs) {
  typedef std::pair<int16, int16> Int16Int16Pair;
  typedef std::pair<int16, int32> Int16Int32Pair;
  typedef std::pair<int16, int64> Int16Int64Pair;

  INSERT_PAIR_TEST(Int16Int16Pair, 4, 6);
  INSERT_PAIR_TEST(Int16Int32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(Int16Int64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));

  typedef std::pair<int32, int16> Int32Int16Pair;
  typedef std::pair<int32, int32> Int32Int32Pair;
  typedef std::pair<int32, int64> Int32Int64Pair;

  INSERT_PAIR_TEST(Int32Int16Pair, 4, 6);
  INSERT_PAIR_TEST(Int32Int32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(Int32Int64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));

  typedef std::pair<int64, int16> Int64Int16Pair;
  typedef std::pair<int64, int32> Int64Int32Pair;
  typedef std::pair<int64, int64> Int64Int64Pair;

  INSERT_PAIR_TEST(Int64Int16Pair, 4, 6);
  INSERT_PAIR_TEST(Int64Int32Pair, 9, (1 << 29) + 378128932);
  INSERT_PAIR_TEST(Int64Int64Pair, 10,
                   (GG_INT64_C(1) << 60) + GG_INT64_C(78931732321));
}

}  // namespace
