// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/webvtt/text_readers.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/file.h>
#include <packager/status/status_test_util.h>

namespace shaka {
namespace media {

using testing::ElementsAre;

TEST(TextReadersTest, ReadLinesWithNewLine) {
  const uint8_t text[] = "a\nb\nc";

  LineReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::string s;
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "a");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "b");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "c");
  ASSERT_FALSE(reader.Next(&s));
}

TEST(TextReadersTest, ReadLinesWithReturnsAndNewLine) {
  const uint8_t text[] = "a\r\nb\r\nc";

  LineReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::string s;
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "a");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "b");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "c");
  ASSERT_FALSE(reader.Next(&s));
}

TEST(TextReadersTest, ReadLinesWithNewLineAndReturns) {
  const uint8_t text[] = "a\n\rb\n\rc";

  LineReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::string s;
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "a");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "b");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "c");
  ASSERT_FALSE(reader.Next(&s));
}

TEST(TextReadersTest, ReadLinesWithReturnAtEnd) {
  const uint8_t text[] = "a\r\nb\r\nc\r";

  LineReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::string s;
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "a");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "b");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "c");
  ASSERT_FALSE(reader.Next(&s));
}

TEST(TextReadersTest, ReadLinesWithMultiplePushes) {
  const uint8_t text1[] = "a\nb";
  const uint8_t text2[] = "c\nd";

  LineReader reader;
  reader.PushData(text1, sizeof(text1) - 1);

  std::string s;
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "a");
  ASSERT_FALSE(reader.Next(&s));

  reader.PushData(text2, sizeof(text2) - 1);
  reader.Flush();
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "bc");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "d");
  ASSERT_FALSE(reader.Next(&s));
}

TEST(TextReadersTest, ReadBlocksReadMultilineBlock) {
  const uint8_t text[] =
      "block 1 - line 1\n"
      "block 1 - line 2";

  BlockReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::vector<std::string> block;
  ASSERT_TRUE(reader.Next(&block));
  EXPECT_THAT(block, ElementsAre("block 1 - line 1", "block 1 - line 2"));
  ASSERT_FALSE(reader.Next(&block));
}

TEST(TextReadersTest, ReadBlocksSkipBlankLinesBeforeBlocks) {
  const uint8_t text[] =
      "\n"
      "\n"
      "block 1\n"
      "\n"
      "\n"
      "block 2\n";

  BlockReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::vector<std::string> block;

  ASSERT_TRUE(reader.Next(&block));
  EXPECT_THAT(block, ElementsAre("block 1"));

  ASSERT_TRUE(reader.Next(&block));
  EXPECT_THAT(block, ElementsAre("block 2"));
  ASSERT_FALSE(reader.Next(&block));
}

TEST(TextReadersTest, ReadBlocksWithOnlyBlankLines) {
  const uint8_t text[] = "\n\n\n\n";

  BlockReader reader;
  reader.PushData(text, sizeof(text) - 1);
  reader.Flush();

  std::vector<std::string> block;
  ASSERT_FALSE(reader.Next(&block));
}

TEST(TextReadersTest, ReadBlocksMultipleReads) {
  const uint8_t text1[] = "block 1\n";
  const uint8_t text2[] =
      "block 2\n"
      "\n"
      "\n"
      "end";

  BlockReader reader;
  reader.PushData(text1, sizeof(text1) - 1);

  std::vector<std::string> block;
  ASSERT_FALSE(reader.Next(&block));
  reader.PushData(text2, sizeof(text2) - 1);
  reader.Flush();

  ASSERT_TRUE(reader.Next(&block));
  EXPECT_THAT(block, ElementsAre("block 1", "block 2"));
}

}  // namespace media
}  // namespace shaka
