// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/file/file.h"
#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/formats/webvtt/text_readers.h"
#include "packager/media/formats/webvtt/webvtt_parser.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {
const size_t kInputCount = 0;
const size_t kOutputCount = 1;
const size_t kOutputIndex = 0;

const size_t kStreamIndex = 0;
const size_t kTimeScale = 1000;
const bool kEncrypted = true;

const char* kNoId = "";
const char* kNoSettings = "";
}  // namespace

class WebVttParserTest : public MediaHandlerTestBase {
 protected:
  void SetUpAndInitializeGraph(const char* text) {
    const char* kFilename = "memory://test-file";

    // Create the input file from the text passed to the test.
    ASSERT_TRUE(File::WriteStringToFile(kFilename, text));

    // Read from the file we just wrote.
    std::unique_ptr<FileReader> reader;
    ASSERT_OK(FileReader::Open(kFilename, &reader));

    parser_ = std::make_shared<WebVttParser>(std::move(reader));

    ASSERT_OK(MediaHandlerTestBase::SetUpAndInitializeGraph(
        parser_, kInputCount, kOutputCount));
  }

  std::shared_ptr<OriginHandler> parser_;
};

TEST_F(WebVttParserTest, FailToParseEmptyFile) {
  const char* text = "";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  EXPECT_CALL(*Output(kOutputIndex), OnProcess(testing::_)).Times(0);
  EXPECT_CALL(*Output(kOutputIndex), OnFlush(testing::_)).Times(0);

  ASSERT_NE(Status::OK, parser_->Run());
}

TEST_F(WebVttParserTest, ParseOnlyHeader) {
  const char* text =
      "WEBVTT\n"
      "\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

TEST_F(WebVttParserTest, ParseHeaderWithBOM) {
  const char* text =
      "\xFE\xFFWEBVTT\n"
      "\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

TEST_F(WebVttParserTest, FailToParseHeaderWrongWord) {
  const char* text =
      "NOT WEBVTT\n"
      "\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  EXPECT_CALL(*Output(kOutputIndex), OnProcess(testing::_)).Times(0);
  EXPECT_CALL(*Output(kOutputIndex), OnFlush(testing::_)).Times(0);

  ASSERT_NE(Status::OK, parser_->Run());
}

TEST_F(WebVttParserTest, FailToParseHeaderNotOneLine) {
  const char* text =
      "WEBVTT\n"
      "WEBVTT\n"
      "\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  EXPECT_CALL(*Output(kOutputIndex), OnProcess(testing::_)).Times(0);
  EXPECT_CALL(*Output(kOutputIndex), OnFlush(testing::_)).Times(0);

  ASSERT_NE(Status::OK, parser_->Run());
}

// TODO: Add style blocks support to WebVttParser.
// This test is disabled until WebVTT parses STYLE blocks.
TEST_F(WebVttParserTest, DISABLED_ParseStyleBlocks) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "STYLE\n"
      "::cue {\n"
      "  background-image: linear-gradient(to bottom, dimgray, lightgray);\n"
      "  color: papayawhip;\n"
      "}";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

TEST_F(WebVttParserTest, ParseOneCue) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "00:01:00.000 --> 01:00:00.000\n"
      "subtitle\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 60000u, 3600000u, kNoSettings,
                                       "subtitle")));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

TEST_F(WebVttParserTest, FailToParseCueWithArrowInId) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "-->\n"
      "00:01:00.000 --> 01:00:00.000\n"
      "subtitle\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  ASSERT_NE(Status::OK, parser_->Run());
}

TEST_F(WebVttParserTest, ParseOneCueWithId) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "id\n"
      "00:01:00.000 --> 01:00:00.000\n"
      "subtitle\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample("id", 60000u, 3600000u, kNoSettings,
                                       "subtitle")));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

TEST_F(WebVttParserTest, ParseOneCueWithSettings) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "00:01:00.000 --> 01:00:00.000 size:50%\n"
      "subtitle\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 60000u, 3600000u, "size:50%",
                                       "subtitle")));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

// Verify that a typical case with mulitple cues work.
TEST_F(WebVttParserTest, ParseMultipleCues) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "00:00:01.000 --> 00:00:05.200\n"
      "subtitle A\n"
      "\n"
      "00:00:02.321 --> 00:00:07.000\n"
      "subtitle B\n"
      "\n"
      "00:00:05.800 --> 00:00:08.000\n"
      "subtitle C\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 1000u, 5200u, kNoSettings,
                                       "subtitle A")));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 2321u, 7000u, kNoSettings,
                                       "subtitle B")));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 5800u, 8000u, kNoSettings,
                                       "subtitle C")));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}

// Verify that a typical case with mulitple cues work even when comments are
// present.
TEST_F(WebVttParserTest, ParseWithComments) {
  const char* text =
      "WEBVTT\n"
      "\n"
      "NOTE This is a one line comment\n"
      "\n"
      "00:00:01.000 --> 00:00:05.200\n"
      "subtitle A\n"
      "\n"
      "NOTE\n"
      "This is a multi-line comment\n"
      "\n"
      "00:00:02.321 --> 00:00:07.000\n"
      "subtitle B\n"
      "\n"
      "NOTE This is a single line comment that\n"
      "spans two lines\n"
      "\n"
      "NOTE\tThis is a comment that using a tab\n"
      "\n"
      "00:00:05.800 --> 00:00:08.000\n"
      "subtitle C\n";

  ASSERT_NO_FATAL_FAILURE(SetUpAndInitializeGraph(text));

  {
    testing::InSequence s;
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsStreamInfo(kStreamIndex, kTimeScale, !kEncrypted)));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 1000u, 5200u, kNoSettings,
                                       "subtitle A")));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 2321u, 7000u, kNoSettings,
                                       "subtitle B")));
    EXPECT_CALL(*Output(kOutputIndex),
                OnProcess(IsTextSample(kNoId, 5800u, 8000u, kNoSettings,
                                       "subtitle C")));
    EXPECT_CALL(*Output(kOutputIndex), OnFlush(kStreamIndex));
  }

  ASSERT_OK(parser_->Run());
}
}  // namespace media
}  // namespace shaka
