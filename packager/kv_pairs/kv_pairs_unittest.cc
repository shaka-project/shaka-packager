// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/kv_pairs/kv_pairs.h"

namespace shaka {

using ::testing::ElementsAre;
using ::std::make_pair;

TEST(KVPairs, Empty) {
  ASSERT_THAT(
      SplitStringIntoKeyValuePairs(""),
      ElementsAre());
}

TEST(KVPairs, Single) {
  ASSERT_THAT(
      SplitStringIntoKeyValuePairs("a=b"),
      ElementsAre(make_pair("a", "b")));
}

TEST(KVPairs, Multiple) {
  ASSERT_THAT(
      SplitStringIntoKeyValuePairs("a=b&c=d&e=f"),
      ElementsAre(
          make_pair("a", "b"),
          make_pair("c", "d"),
          make_pair("e", "f")));
}

TEST(KVPairs, ExtraEqualsSigns) {
  ASSERT_THAT(
      SplitStringIntoKeyValuePairs("a=b&c==d&e=f=g=h"),
      ElementsAre(
          make_pair("a", "b"),
          make_pair("c", "=d"),
          make_pair("e", "f=g=h")));
}

}  // namespace shaka
