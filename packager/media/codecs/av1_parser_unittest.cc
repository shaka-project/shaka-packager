// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/av1_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/media/test/test_data_util.h"

using ::testing::ElementsAre;

namespace shaka {
namespace media {

inline bool operator==(const AV1Parser::Tile& lhs, const AV1Parser::Tile& rhs) {
  return lhs.start_offset_in_bytes == rhs.start_offset_in_bytes &&
         lhs.size_in_bytes == rhs.size_in_bytes;
}

TEST(AV1ParserTest, ParseIFrameSuccess) {
  const std::vector<uint8_t> buffer = ReadTestDataFile("av1-I-frame-320x240");

  AV1Parser parser;
  std::vector<AV1Parser::Tile> tiles;
  ASSERT_TRUE(parser.Parse(buffer.data(), buffer.size(), &tiles));
  EXPECT_THAT(tiles, ElementsAre(AV1Parser::Tile{0x1d, 0x4e1}));
}

}  // namespace media
}  // namespace shaka
