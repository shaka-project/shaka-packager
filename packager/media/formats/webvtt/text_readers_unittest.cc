// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/file/file.h"
#include "packager/media/formats/webvtt/text_readers.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {
const char* kFilename = "memory://test-file";
}  // namespace

TEST(TextReadersTest, ReadWholeStream) {
  const char* text = "abcd";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

  char c;
  ASSERT_TRUE(source->Next(&c));
  ASSERT_EQ(c, 'a');
  ASSERT_TRUE(source->Next(&c));
  ASSERT_EQ(c, 'b');
  ASSERT_TRUE(source->Next(&c));
  ASSERT_EQ(c, 'c');
  ASSERT_TRUE(source->Next(&c));
  ASSERT_EQ(c, 'd');
  ASSERT_FALSE(source->Next(&c));
}

TEST(TextReadersTest, Peeking) {
  const char* text = "abc";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

  PeekingReader reader(std::move(source));

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
  const char* text = "a\nb\nc";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

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
  const char* text = "a\r\nb\r\nc";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

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
  const char* text = "a\n\rb\n\rc";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

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
  const char* text = "a\r\nb\r\nc\r";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

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
  const char* text =
      "block 1 - line 1\n"
      "block 1 - line 2";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

  BlockReader reader(std::move(source));

  std::vector<std::string> block;

  ASSERT_TRUE(reader.Next(&block));
  ASSERT_EQ(2u, block.size());
  ASSERT_EQ("block 1 - line 1", block[0]);
  ASSERT_EQ("block 1 - line 2", block[1]);

  ASSERT_FALSE(reader.Next(&block));
}

TEST(TextReadersTest, ReadBlocksSkipBlankLinesBeforeBlocks) {
  const char* text =
      "\n"
      "\n"
      "block 1\n"
      "\n"
      "\n"
      "block 2\n";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

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
  const char* text = "\n\n\n\n";

  ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

  std::unique_ptr<FileReader> source;
  ASSERT_OK(FileReader::Open(kFilename, &source));

  BlockReader reader(std::move(source));

  std::vector<std::string> block;
  ASSERT_FALSE(reader.Next(&block));
}
}  // namespace media
}  // namespace shaka
