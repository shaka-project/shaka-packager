// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/av1_parser.h"

#include <gtest/gtest.h>

#include "packager/media/test/test_data_util.h"

namespace shaka {
namespace media {

TEST(AV1ParserTest, ParseIFrameSuccess) {
  const std::vector<uint8_t> buffer = ReadTestDataFile("av1-I-frame-320x240");

  AV1Parser parser;
  ASSERT_TRUE(parser.Parse(buffer.data(), buffer.size()));
}

}  // namespace media
}  // namespace shaka
