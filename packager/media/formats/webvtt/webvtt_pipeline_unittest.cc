// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/file/file_test_util.h"
#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/event/mock_muxer_listener.h"
#include "packager/media/formats/webvtt/text_readers.h"
#include "packager/media/formats/webvtt/webvtt_output_handler.h"
#include "packager/media/formats/webvtt/webvtt_parser.h"
#include "packager/media/formats/webvtt/webvtt_segmenter.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {

TEST(WebVttTextPipelineTest, SegmentedOutput) {
  const uint64_t kSegmentDuration = 10000;  // 10 seconds

  const char* kTemplate = "memory://output/template-$Number$.vtt";
  const char* kOutput1 = "memory://output/template-1.vtt";
  const char* kOutput2 = "memory://output/template-2.vtt";
  const char* kOutput3 = "memory://output/template-3.vtt";
  const char* kOutput4 = "memory://output/template-4.vtt";

  const char* kInputFile = "memory://input.vtt";
  const char* kInput =
      "WEBVTT\n"
      "\n"
      "1\n"
      "00:00:18.700 --> 00:00:21.500\n"
      "This blade has a dark past.\n"
      "\n"
      "2\n"
      "00:00:22.800 --> 00:00:26.800\n"
      "It has shed much innocent blood.\n"
      "\n"
      "3\n"
      "00:00:29.000 --> 00:00:32.450\n"
      "You're a fool for traveling alone,\n"
      "so completely unprepared.\n"
      "\n"
      "4\n"
      "00:00:32.750 --> 00:00:35.800\n"
      "You're lucky your blood's still flowing.\n"
      "\n"
      "5\n"
      "00:00:36.250 --> 00:00:37.300\n"
      "Thank you.\n";

  // Segment One
  // 00:00:00.000 to 00:00:10.000
  const char* kExpectedOutput1 =
      "WEBVTT\n"
      "\n";

  // Segment Two
  // 00:00:10.000 to 00:00:20.000
  const char* kExpectedOutput2 =
      "WEBVTT\n"
      "\n"
      "1\n"
      "00:00:18.700 --> 00:00:21.500\n"
      "This blade has a dark past.\n"
      "\n";

  // Segment Three
  // 00:00:20.000 to 00:00:30.000
  const char* kExpectedOutput3 =
      "WEBVTT\n"
      "\n"
      "1\n"
      "00:00:18.700 --> 00:00:21.500\n"
      "This blade has a dark past.\n"
      "\n"
      "2\n"
      "00:00:22.800 --> 00:00:26.800\n"
      "It has shed much innocent blood.\n"
      "\n"
      "3\n"
      "00:00:29.000 --> 00:00:32.450\n"
      "You're a fool for traveling alone,\nso completely unprepared.\n"
      "\n";

  // Segment Four
  // 00:00:30.000 to 00:00:40.000
  const char* kExpectedOutput4 =
      "WEBVTT\n"
      "\n"
      "3\n"
      "00:00:29.000 --> 00:00:32.450\n"
      "You're a fool for traveling alone,\nso completely unprepared.\n"
      "\n"
      "4\n"
      "00:00:32.750 --> 00:00:35.800\n"
      "You're lucky your blood's still flowing.\n"
      "\n"
      "5\n"
      "00:00:36.250 --> 00:00:37.300\n"
      "Thank you.\n"
      "\n";

  // Create the input file and the file reader.
  ASSERT_TRUE(File::WriteStringToFile(kInputFile, kInput));
  std::unique_ptr<FileReader> reader;
  ASSERT_OK(FileReader::Open(kInputFile, &reader));
  std::shared_ptr<OriginHandler> parser(new WebVttParser(std::move(reader)));

  std::shared_ptr<MediaHandler> segmenter(
      new WebVttSegmenter(kSegmentDuration));

  // Create the output handler. Because we are not verifying the manifest, we
  // are using the combined muxer listener as a noop.
  MuxerOptions options;
  options.segment_template = kTemplate;
  std::unique_ptr<MuxerListener> listener(
      new testing::NiceMock<MockMuxerListener>());
  std::shared_ptr<MediaHandler> output(
      new WebVttSegmentedOutputHandler(options, std::move(listener)));

  // Set up the graph
  ASSERT_OK(segmenter->AddHandler(std::move(output)));
  ASSERT_OK(parser->AddHandler(std::move(segmenter)));

  // Initialize and start the pipeline
  ASSERT_OK(parser->Initialize());
  ASSERT_OK(parser->Run());

  // Make sure all the files appear as expected.
  ASSERT_FILE_STREQ(kOutput1, kExpectedOutput1);
  ASSERT_FILE_STREQ(kOutput2, kExpectedOutput2);
  ASSERT_FILE_STREQ(kOutput3, kExpectedOutput3);
  ASSERT_FILE_STREQ(kOutput4, kExpectedOutput4);
}
}  // namespace media
}  // namespace shaka
