// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/file/file_test_util.h"
#include "packager/media/base/media_handler_test_base.h"
#include "packager/media/base/text_stream_info.h"
#include "packager/media/event/combined_muxer_listener.h"
#include "packager/media/formats/webvtt/webvtt_output_handler.h"
#include "packager/status_test_util.h"

namespace shaka {
namespace media {
namespace {
const size_t kInputCount = 1;
const size_t kOutputCount = 0;
const size_t kInputIndex = 0;
const size_t kStreamIndex = 0;

const bool kEncrypted = true;
const char* kNoId = "";

const char* kSegmentedFileTemplate = "memory://output/template-$Number$.vtt";
const char* kSegmentedFileOutput1 = "memory://output/template-1.vtt";
const char* kSegmentedFileOutput2 = "memory://output/template-2.vtt";
}  // namespace

class WebVttSegmentedOutputTest : public MediaHandlerTestBase {
 protected:
  void SetUp() {
    MuxerOptions muxer_options;
    muxer_options.segment_template = kSegmentedFileTemplate;
    std::unique_ptr<MuxerListener> muxer_listener(new CombinedMuxerListener());

    out_ = std::make_shared<WebVttSegmentedOutputHandler>(
        muxer_options, std::move(muxer_listener));

    ASSERT_OK(SetUpAndInitializeGraph(out_, kInputCount, kOutputCount));
  }

  std::shared_ptr<WebVttSegmentedOutputHandler> out_;
};

TEST_F(WebVttSegmentedOutputTest, WithNoSegmentAndWithNoSamples) {
  // Expected output - No files should be created as there were no
  //                   samples.

  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromStreamInfo(kStreamIndex,
                                                      GetTextStreamInfo())));
  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromSegmentInfo(
              kStreamIndex, GetSegmentInfo(kStreamIndex, 10000, !kEncrypted))));
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());
}

TEST_F(WebVttSegmentedOutputTest, WithOneSegmentAndWithOneSample) {
  const char* kExpectedOutput =
      "WEBVTT\n"
      "\n"
      "00:00:05.000 --> 00:00:06.000\n"
      "payload\n"
      "\n";

  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromStreamInfo(kStreamIndex,
                                                      GetTextStreamInfo())));
  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromTextSample(
              kStreamIndex, GetTextSample(kNoId, 5000, 6000, "payload"))));
  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromSegmentInfo(
                    kStreamIndex, GetSegmentInfo(0, 10000, !kEncrypted))));
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());

  ASSERT_FILE_STREQ(kSegmentedFileOutput1, kExpectedOutput);
}

TEST_F(WebVttSegmentedOutputTest, WithTwoSegmentAndWithOneSample) {
  const char* kExpectedOutput1 =
      "WEBVTT\n"
      "\n"
      "00:00:05.000 --> 00:00:06.000\n"
      "payload 1\n"
      "\n";

  const char* kExpectedOutput2 =
      "WEBVTT\n"
      "\n"
      "00:00:15.000 --> 00:00:16.000\n"
      "payload 2\n"
      "\n";

  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromStreamInfo(0, GetTextStreamInfo())));

  // Segment One
  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromTextSample(
              kStreamIndex, GetTextSample(kNoId, 5000, 6000, "payload 1"))));
  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromSegmentInfo(
                    kStreamIndex, GetSegmentInfo(0, 10000, !kEncrypted))));
  // Segment Two
  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromTextSample(
              kStreamIndex, GetTextSample(kNoId, 15000, 16000, "payload 2"))));
  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromSegmentInfo(
                    kStreamIndex, GetSegmentInfo(10000, 10000, !kEncrypted))));
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());

  ASSERT_FILE_STREQ(kSegmentedFileOutput1, kExpectedOutput1);
  ASSERT_FILE_STREQ(kSegmentedFileOutput2, kExpectedOutput2);
}

TEST_F(WebVttSegmentedOutputTest, WithAnEmptySegment) {
  const char* kExpectedOutput =
      "WEBVTT\n"
      "\n"
      "00:00:15.000 --> 00:00:16.000\n"
      "payload 2\n"
      "\n";

  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromStreamInfo(0, GetTextStreamInfo())));
  // Segment One
  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromSegmentInfo(
                    kStreamIndex, GetSegmentInfo(0, 10000, !kEncrypted))));
  // Segment Two
  ASSERT_OK(
      Input(kInputIndex)
          ->Dispatch(StreamData::FromTextSample(
              kStreamIndex, GetTextSample(kNoId, 15000, 16000, "payload 2"))));
  ASSERT_OK(Input(kInputIndex)
                ->Dispatch(StreamData::FromSegmentInfo(
                    kStreamIndex, GetSegmentInfo(10000, 10000, !kEncrypted))));
  ASSERT_OK(Input(kInputIndex)->FlushAllDownstreams());

  // The empty segment will not write to disk, but it will use segment's
  // filename.
  ASSERT_FILE_STREQ(kSegmentedFileOutput2, kExpectedOutput);
}
}  // namespace media
}  // namespace shaka
