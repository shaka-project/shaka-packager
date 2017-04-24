// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/codecs/vp8_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

namespace shaka {
namespace media {
namespace {
MATCHER_P5(EqualVPxFrame,
           frame_size,
           uncompressed_header_size,
           is_keyframe,
           width,
           height,
           "") {
  *result_listener << "which is (" << arg.frame_size << ", "
                   << arg.uncompressed_header_size << ", " << arg.is_keyframe
                   << ", " << arg.width << ", " << arg.height << ").";
  return arg.frame_size == frame_size &&
         arg.uncompressed_header_size == uncompressed_header_size &&
         arg.is_keyframe == is_keyframe && arg.width == width &&
         arg.height == height;
}
}  // namespace

TEST(VP8ParserTest, Keyframe) {
  const uint8_t kData[] = {
      0x54, 0x04, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00, 0x00, 0x47,
      0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x01, 0x24, 0x10, 0x17, 0x67,
      0x63, 0x3f, 0xbb, 0xe5, 0xcf, 0x9b, 0x7d, 0x53, 0xec, 0x67, 0xa2, 0xcf,
  };

  EXPECT_TRUE(VP8Parser::IsKeyframe(kData, arraysize(kData)));

  VP8Parser parser;
  std::vector<VPxFrameInfo> frames;
  ASSERT_TRUE(parser.Parse(kData, arraysize(kData), &frames));
  EXPECT_EQ("vp08.02.10.08.01.02.02.02.00",
            parser.codec_config().GetCodecString(kCodecVP8));
  EXPECT_THAT(frames, ElementsAre(EqualVPxFrame(arraysize(kData), 22u, true,
                                                320u, 240u)));
}

TEST(VP8ParserTest, NonKeyframe) {
  const uint8_t kData[] = {
      0x31, 0x03, 0x00, 0x11, 0x10, 0xa4, 0x00, 0x1a, 0xea, 0xd8, 0xaf, 0x40,
      0xcf, 0x80, 0x2f, 0xdc, 0x9d, 0x42, 0x4b, 0x19, 0xc8, 0x04, 0x97, 0x28,
      0x34, 0x7b, 0x47, 0xfc, 0x2d, 0xaa, 0x0b, 0xbb, 0xc6, 0xc3, 0xc1, 0x12,
  };

  EXPECT_FALSE(VP8Parser::IsKeyframe(kData, arraysize(kData)));

  VP8Parser parser;
  std::vector<VPxFrameInfo> frames;
  ASSERT_TRUE(parser.Parse(kData, arraysize(kData), &frames));
  EXPECT_THAT(frames,
              ElementsAre(EqualVPxFrame(arraysize(kData), 8u, false, 0u, 0u)));
}

TEST(VP8ParserTest, InsufficientData) {
  const uint8_t kData[] = {0x00, 0x0a};
  EXPECT_FALSE(VP8Parser::IsKeyframe(kData, arraysize(kData)));
  VP8Parser parser;
  std::vector<VPxFrameInfo> frames;
  ASSERT_FALSE(parser.Parse(kData, arraysize(kData), &frames));
}

TEST(VP8ParserTest, CorruptedSynccode) {
  const uint8_t kData[] = {
      0x54, 0x04, 0x00, 0x9d, 0x21, 0x2a, 0x40, 0x01, 0xf0, 0x00, 0x00, 0x47,
      0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x01, 0x24, 0x10, 0x17, 0x67,
      0x63, 0x3f, 0xbb, 0xe5, 0xcf, 0x9b, 0x7d, 0x53, 0xec, 0x67, 0xa2, 0xcf,
  };
  EXPECT_FALSE(VP8Parser::IsKeyframe(kData, arraysize(kData)));
  VP8Parser parser;
  std::vector<VPxFrameInfo> frames;
  ASSERT_FALSE(parser.Parse(kData, arraysize(kData), &frames));
}

TEST(VP8ParserTest, NotEnoughBytesForHeaderSize) {
  const uint8_t kData[] = {
      0x54, 0x06, 0x00, 0x9d, 0x01, 0x2a, 0x40, 0x01, 0xf0, 0x00, 0x00, 0x47,
      0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x01, 0x24, 0x10, 0x17, 0x67,
      0x63, 0x3f, 0xbb, 0xe5, 0xcf, 0x9b, 0x7d, 0x53, 0xec, 0x67, 0xa2, 0xcf,
  };

  // IsKeyframe only parses the bytes that is necessary to determine whether it
  // is a keyframe.
  EXPECT_TRUE(VP8Parser::IsKeyframe(kData, arraysize(kData)));

  VP8Parser parser;
  std::vector<VPxFrameInfo> frames;
  EXPECT_FALSE(parser.Parse(kData, arraysize(kData), &frames));
}

}  // namespace media
}  // namespace shaka
