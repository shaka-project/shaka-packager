// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/bind.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/stream_info.h"
#include "packager/media/formats/webvtt/webvtt_media_parser.h"

namespace shaka {
namespace media {

typedef testing::MockFunction<void(const std::vector<scoped_refptr<StreamInfo>>&
                                       stream_info)> MockInitCallback;
typedef testing::MockFunction<bool(
    uint32_t track_id,
    const scoped_refptr<MediaSample>& media_sample)> MockNewSampleCallback;

using testing::_;
using testing::InSequence;
using testing::Return;

class WebVttMediaParserTest : public ::testing::Test {
 public:
  WebVttMediaParserTest() {}
  ~WebVttMediaParserTest() override {}

  void InitializeParser() {
    parser_.Init(
        base::Bind(&MockInitCallback::Call, base::Unretained(&init_callback_)),
        base::Bind(&MockNewSampleCallback::Call,
                   base::Unretained(&new_sample_callback_)),
        nullptr);
  }

  MockInitCallback init_callback_;
  MockNewSampleCallback new_sample_callback_;

  WebVttMediaParser parser_;
};

TEST_F(WebVttMediaParserTest, Init) {
  InitializeParser();
}

TEST_F(WebVttMediaParserTest, ParseOneCue) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, _)).WillOnce(Return(true));

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "00:01:00.000 --> 01:00:00.000\n"
      "subtitle";
  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));

  EXPECT_TRUE(parser_.Flush());
}

// Verify that different types of line breaks work.
TEST_F(WebVttMediaParserTest, DifferentLineBreaks) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, _)).WillOnce(Return(true));

  const char kWebVtt[] =
      "WEBVTT\r\n"
      "\r\n"
      "00:01:00.000 --> 01:00:00.000\n"
      "subtitle\r";
  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));

  EXPECT_TRUE(parser_.Flush());
}

TEST_F(WebVttMediaParserTest, ParseMultpleCues) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, _))
      .Times(2)
      .WillRepeatedly(Return(true));

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "00:01:00.000 --> 01:00:00.000\n"
      "subtitle\n"
      "\n"
      "02:01:00.000 --> 02:02:00.000\n"
      "more subtitle";
  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));

  EXPECT_TRUE(parser_.Flush());
}

MATCHER_P2(MatchesStartTimeAndDuration, start_time, duration, "") {
  return arg->pts() == start_time && arg->duration() == duration;
}

// Verify that the timing parsing is done correctly and gets the right start
// time and duration in milliseconds.
TEST_F(WebVttMediaParserTest, VerifyTimingParsing) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_,
              Call(_, MatchesStartTimeAndDuration(61004, 204088)))
      .WillOnce(Return(true));

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "00:01:01.004 --> 00:04:25.092\n"
      "subtitle";
  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));

  EXPECT_TRUE(parser_.Flush());
}

// Expect parse failure if hour part of the timestamp is too short.
TEST_F(WebVttMediaParserTest, MalformedHourTimestamp) {
  EXPECT_CALL(new_sample_callback_, Call(_, _)).Times(0);

  const char kHourStringTooShort[] =
      "WEBVTT\n"
      "\n"
      "0:01:01.004 --> 00:04:25.092\n"
      "subtitle\n";
  InitializeParser();

  EXPECT_FALSE(
      parser_.Parse(reinterpret_cast<const uint8_t*>(kHourStringTooShort),
                    arraysize(kHourStringTooShort) - 1));
}

// Each component of the timestamp is correct but contains spaces.
TEST_F(WebVttMediaParserTest, SpacesInTimestamp) {
  EXPECT_CALL(new_sample_callback_, Call(_, _)).Times(0);

  const char kSpacesInTimestamp[] =
      "WEBVTT\n"
      "\n"
      "0:01: 1.004 --> 0 :04:25.092\n"
      "subtitle\n";
  InitializeParser();

  EXPECT_FALSE(
      parser_.Parse(reinterpret_cast<const uint8_t*>(kSpacesInTimestamp),
                    arraysize(kSpacesInTimestamp) - 1));
}

MATCHER_P(MatchesPayload, data, "") {
  std::vector<uint8_t> arg_data(arg->data(), arg->data() + arg->data_size());
  return arg_data == data;
}

TEST_F(WebVttMediaParserTest, VerifyCuePayload) {
  const char kExpectedPayload1[] = "subtitle";
  const char kExpectedPayload2[] = "hello";
  std::vector<uint8_t> expected_payload(
      kExpectedPayload1, kExpectedPayload1 + arraysize(kExpectedPayload1) - 1);

  InSequence s;
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, MatchesPayload(expected_payload)))
      .WillOnce(Return(true));

  expected_payload.assign(kExpectedPayload2,
                          kExpectedPayload2 + arraysize(kExpectedPayload2) - 1);
  EXPECT_CALL(new_sample_callback_, Call(_, MatchesPayload(expected_payload)))
      .WillOnce(Return(true));

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "00:01:01.004 --> 00:01:22.088\n"
      "subtitle\n"
      "\n"
      "02:06:00.000 --> 02:30:02.006\n"
      "hello";

  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));

  EXPECT_TRUE(parser_.Flush());
}

// Verify that a sample can be created from multiple calls to Parse(), i.e. one
// Parse() is not a full sample.
TEST_F(WebVttMediaParserTest, PartialParse) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, _)).WillOnce(Return(true));

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "00:01:01.004 --> 00:04:25.092\n"
      "subtitle";
  InitializeParser();
  // Pass in the first 8 bytes, i.e. right before the first cue.
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt), 8));
  // Pass in the rest of the cue.
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt) + 8,
                            arraysize(kWebVtt) - 1 - 8));

  EXPECT_TRUE(parser_.Flush());
}

// Verify that metadata header with --> is rejected.
TEST_F(WebVttMediaParserTest, BadMetadataHeader) {
  EXPECT_CALL(init_callback_, Call(_)).Times(0);
  EXPECT_CALL(new_sample_callback_, Call(_, _)).Times(0);
  const char kBadWebVtt[] =
      "WEBVTT\n"
      "00:01:01.004 --> 00:04:25.092\n";
  InitializeParser();
  EXPECT_FALSE(parser_.Parse(reinterpret_cast<const uint8_t*>(kBadWebVtt),
                             arraysize(kBadWebVtt) - 1));
  EXPECT_TRUE(parser_.Flush());
}

MATCHER_P(MatchesComment, comment, "") {
  std::vector<uint8_t> arg_comment(arg->side_data(),
                                   arg->side_data() + arg->side_data_size());
  return arg_comment == comment;
}

// Verify that comment is parsed.
TEST_F(WebVttMediaParserTest, Comment) {
  const char kExpectedComment[] = "NOTE This is a comment";
  std::vector<uint8_t> expected_comment(
      kExpectedComment, kExpectedComment + arraysize(kExpectedComment) - 1);

  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, MatchesComment(expected_comment)))
      .WillOnce(Return(true));

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "NOTE This is a comment\n";

  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));
  EXPECT_TRUE(parser_.Flush());
}

// Verify that comment with --> is rejected.
TEST_F(WebVttMediaParserTest, BadComment) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, _)).Times(0);

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "NOTE BAD Comment -->.\n";

  InitializeParser();
  EXPECT_FALSE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                             arraysize(kWebVtt) - 1));
  EXPECT_TRUE(parser_.Flush());
}

MATCHER_P(HeaderMatches, header, "") {
  const std::vector<uint8_t>& codec_config = arg.at(0)->codec_config();
  return codec_config == header;
}

// Verify that the metadata header and the WEBVTT magic string is there.
TEST_F(WebVttMediaParserTest, Header) {
  const char kHeader[] = "WEBVTT\nRegion: id=anything width=40%";
  std::vector<uint8_t> expected_header(kHeader,
                                       kHeader + arraysize(kHeader) - 1);

  EXPECT_CALL(init_callback_, Call(HeaderMatches(expected_header)));
  ON_CALL(new_sample_callback_, Call(_, _)).WillByDefault(Return(true));
  const char kWebVtt[] =
      "WEBVTT\n"
      "Region: id=anything width=40%\n"
      "\n"
      "00:01:01.004 --> 00:04:25.092\n"
      "subtitle";

  InitializeParser();
  EXPECT_TRUE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                            arraysize(kWebVtt) - 1));
  EXPECT_TRUE(parser_.Flush());
}

// Verify that if timing is not present after an identifier, the parser errors.
TEST_F(WebVttMediaParserTest, NoTimingAfterIdentifier) {
  EXPECT_CALL(init_callback_, Call(_));
  EXPECT_CALL(new_sample_callback_, Call(_, _)).Times(0);

  const char kWebVtt[] =
      "WEBVTT\n"
      "\n"
      "anyid\n"
      "00:12.000 00:13.000\n";  // This line doesn't have -->, so error.
  InitializeParser();
  EXPECT_FALSE(parser_.Parse(reinterpret_cast<const uint8_t*>(kWebVtt),
                             arraysize(kWebVtt) - 1));
  EXPECT_TRUE(parser_.Flush());
}

}  // namespace media
}  // namespace shaka
