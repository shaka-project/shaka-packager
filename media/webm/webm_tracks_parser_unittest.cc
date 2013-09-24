// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/webm/tracks_builder.h"
#include "media/webm/webm_constants.h"
#include "media/webm/webm_tracks_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace media {

static const int kTypeSubtitlesOrCaptions = 0x11;
static const int kTypeDescriptionsOrMetadata = 0x21;

class WebMTracksParserTest : public testing::Test {
 public:
  WebMTracksParserTest() {}
};

static void VerifyTextTrackInfo(const uint8* buffer,
                                int buffer_size,
                                TextKind text_kind,
                                const std::string& name,
                                const std::string& language) {
  scoped_ptr<WebMTracksParser> parser(new WebMTracksParser(LogCB(), false));

  int result = parser->Parse(buffer, buffer_size);
  EXPECT_GT(result, 0);
  EXPECT_EQ(result, buffer_size);

  const WebMTracksParser::TextTracks& text_tracks = parser->text_tracks();
  EXPECT_EQ(text_tracks.size(), WebMTracksParser::TextTracks::size_type(1));

  const WebMTracksParser::TextTracks::const_iterator itr = text_tracks.begin();
  EXPECT_EQ(itr->first, 1);  // track num

  const WebMTracksParser::TextTrackInfo& info = itr->second;
  EXPECT_EQ(info.kind, text_kind);
  EXPECT_TRUE(info.name == name);
  EXPECT_TRUE(info.language == language);
}

TEST_F(WebMTracksParserTest, SubtitleNoNameNoLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTrack(1, kWebMTrackTypeSubtitlesOrCaptions,
              kWebMCodecSubtitles, "", "");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "", "");
}

TEST_F(WebMTracksParserTest, SubtitleYesNameNoLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTrack(1, kWebMTrackTypeSubtitlesOrCaptions,
              kWebMCodecSubtitles, "Spock", "");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "Spock", "");
}

TEST_F(WebMTracksParserTest, SubtitleNoNameYesLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTrack(1, kWebMTrackTypeSubtitlesOrCaptions,
              kWebMCodecSubtitles, "", "eng");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "", "eng");
}

TEST_F(WebMTracksParserTest, SubtitleYesNameYesLang) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTrack(1, kWebMTrackTypeSubtitlesOrCaptions,
              kWebMCodecSubtitles, "Picard", "fre");

  const std::vector<uint8> buf = tb.Finish();
  VerifyTextTrackInfo(&buf[0], buf.size(), kTextSubtitles, "Picard", "fre");
}

TEST_F(WebMTracksParserTest, IgnoringTextTracks) {
  InSequence s;

  TracksBuilder tb;
  tb.AddTrack(1, kWebMTrackTypeSubtitlesOrCaptions,
              kWebMCodecSubtitles, "Subtitles", "fre");
  tb.AddTrack(2, kWebMTrackTypeSubtitlesOrCaptions,
              kWebMCodecSubtitles, "Commentary", "fre");

  const std::vector<uint8> buf = tb.Finish();
  scoped_ptr<WebMTracksParser> parser(new WebMTracksParser(LogCB(), true));

  int result = parser->Parse(&buf[0], buf.size());
  EXPECT_GT(result, 0);
  EXPECT_EQ(result, static_cast<int>(buf.size()));

  EXPECT_EQ(parser->text_tracks().size(), 0u);

  const std::set<int64>& ignored_tracks = parser->ignored_tracks();
  EXPECT_TRUE(ignored_tracks.find(1) != ignored_tracks.end());
  EXPECT_TRUE(ignored_tracks.find(2) != ignored_tracks.end());

  // Test again w/o ignoring the test tracks.
  parser.reset(new WebMTracksParser(LogCB(), false));

  result = parser->Parse(&buf[0], buf.size());
  EXPECT_GT(result, 0);

  EXPECT_EQ(parser->ignored_tracks().size(), 0u);
  EXPECT_EQ(parser->text_tracks().size(), 2u);
}

}  // namespace media
