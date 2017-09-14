// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/media/formats/webvtt/text_readers.h"

namespace shaka {
namespace media {

TEST(TextReadersTest, ReadWholeStream) {
  const std::string input = "abcd";
  StringCharReader source(input);

  char c;
  ASSERT_TRUE(source.Next(&c));
  ASSERT_EQ(c, 'a');
  ASSERT_TRUE(source.Next(&c));
  ASSERT_EQ(c, 'b');
  ASSERT_TRUE(source.Next(&c));
  ASSERT_EQ(c, 'c');
  ASSERT_TRUE(source.Next(&c));
  ASSERT_EQ(c, 'd');
  ASSERT_FALSE(source.Next(&c));
}

TEST(TextReadersTest, Peeking) {
  const std::string input = "abc";
  std::unique_ptr<CharReader> source(new StringCharReader(input));
  PeekingCharReader reader(std::move(source));

  char c;
  ASSERT_TRUE(reader.Peek(&c));
  ASSERT_EQ(c, 'a');
  ASSERT_TRUE(reader.Next(&c));
  ASSERT_EQ(c, 'a');
  ASSERT_TRUE(reader.Peek(&c));
  ASSERT_EQ(c, 'b');
  ASSERT_TRUE(reader.Next(&c));
  ASSERT_EQ(c, 'b');
  ASSERT_TRUE(reader.Peek(&c));
  ASSERT_EQ(c, 'c');
  ASSERT_TRUE(reader.Next(&c));
  ASSERT_EQ(c, 'c');
  ASSERT_FALSE(reader.Peek(&c));
  ASSERT_FALSE(reader.Next(&c));
}

TEST(TextReadersTest, ReadLinesWithNewLine) {
  const std::string input = "a\nb\nc";
  std::unique_ptr<CharReader> source(new StringCharReader(input));
  LineReader reader(std::move(source));

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
  const std::string input = "a\r\nb\r\nc";
  std::unique_ptr<CharReader> source(new StringCharReader(input));
  LineReader reader(std::move(source));

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
  const std::string input = "a\n\rb\n\rc";
  std::unique_ptr<CharReader> source(new StringCharReader(input));
  LineReader reader(std::move(source));

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
  const std::string input = "a\r\nb\r\nc\r";
  std::unique_ptr<CharReader> source(new StringCharReader(input));
  LineReader reader(std::move(source));

  std::string s;
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "a");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "b");
  ASSERT_TRUE(reader.Next(&s));
  ASSERT_EQ(s, "c");
  ASSERT_FALSE(reader.Next(&s));
}

TEST(TextReadersTest, ReadBlocksReadMultilineBlock) {
  const std::string input =
      "block 1 - line 1\n"
      "block 1 - line 2";

  std::unique_ptr<CharReader> source(new StringCharReader(input));
  BlockReader reader(std::move(source));

  std::vector<std::string> block;

  ASSERT_TRUE(reader.Next(&block));
  ASSERT_EQ(2u, block.size());
  ASSERT_EQ("block 1 - line 1", block[0]);
  ASSERT_EQ("block 1 - line 2", block[1]);

  ASSERT_FALSE(reader.Next(&block));
}

TEST(TextReadersTest, ReadBlocksSkipBlankLinesBeforeBlocks) {
  const std::string input =
      "\n"
      "\n"
      "block 1\n"
      "\n"
      "\n"
      "block 2\n";

  std::unique_ptr<CharReader> source(new StringCharReader(input));
  BlockReader reader(std::move(source));

  std::vector<std::string> block;

  ASSERT_TRUE(reader.Next(&block));
  ASSERT_EQ(1u, block.size());
  ASSERT_EQ("block 1", block[0]);

  ASSERT_TRUE(reader.Next(&block));
  ASSERT_EQ(1u, block.size());
  ASSERT_EQ("block 2", block[0]);

  ASSERT_FALSE(reader.Next(&block));
}

TEST(TextReadersTest, ReadBlocksWithOnlyBlankLines) {
  const std::string input = "\n\n\n\n";

  std::unique_ptr<CharReader> source(new StringCharReader(input));
  BlockReader reader(std::move(source));

  std::vector<std::string> block;
  ASSERT_FALSE(reader.Next(&block));
}
}  // namespace media
}  // namespace shaka
