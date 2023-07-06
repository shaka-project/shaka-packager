// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/ttml/ttml_generator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shaka {
namespace media {
namespace ttml {

namespace {

const int64_t kMsTimeScale = 1000;

const TextFragmentStyle kNoStyles{};
const bool kNewline = true;
const std::string kNoId = "";

TextSettings DefaultSettings() {
  TextSettings settings;
  // Override default value so TTML doesn't print this setting by default.
  settings.text_alignment = TextAlignment::kStart;
  return settings;
}

struct TestProperties {
  std::string id;
  int64_t start = 5000;
  int64_t end = 6000;
  TextSettings settings = DefaultSettings();
  TextFragment body;

  std::map<std::string, TextRegion> regions;
  std::string language = "";
  int32_t time_scale = kMsTimeScale;
};

}  // namespace

class TtmlMuxerTest : public testing::Test {
 protected:
  void ParseSingleCue(const std::string& expected_body,
                      const TestProperties& properties) {
    TtmlGenerator generator;
    generator.Initialize(properties.regions, properties.language,
                         properties.time_scale);
    generator.AddSample(TextSample(properties.id, properties.start,
                                   properties.end, properties.settings,
                                   properties.body));

    std::string results;
    ASSERT_TRUE(generator.Dump(&results));
    ASSERT_EQ(results, expected_body);
  }
};

TEST_F(TtmlMuxerTest, WithOneSegmentAndWithOneSample) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\">payload</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.body.body = "payload";
  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, MultipleFragmentsWithNewlines) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\">foo bar<br/>baz</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.body.sub_fragments.emplace_back(kNoStyles, "foo ");
  properties.body.sub_fragments.emplace_back(kNoStyles, "bar");
  properties.body.sub_fragments.emplace_back(kNoStyles, kNewline);
  properties.body.sub_fragments.emplace_back(kNoStyles, "baz");

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesStyles) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\">\n"
      "        <span tts:fontWeight=\"bold\">foo</span>\n"
      "        <span tts:fontStyle=\"italic\">bar</span>\n"
      "        <span tts:textDecoration=\"underline\">baz</span>\n"
      "      </p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.body.sub_fragments.emplace_back(kNoStyles, "foo");
  properties.body.sub_fragments.back().style.bold = true;
  properties.body.sub_fragments.emplace_back(kNoStyles, "bar");
  properties.body.sub_fragments.back().style.italic = true;
  properties.body.sub_fragments.emplace_back(kNoStyles, "baz");
  properties.body.sub_fragments.back().style.underline = true;

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesRegions) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head>\n"
      "    <region xml:id=\"foo\" tts:origin=\"20px 40px\" "
      "tts:extent=\"22% 33%\"/>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\" region=\"foo\">bar</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.settings.region = "foo";
  properties.body.body = "bar";

  TextRegion region;
  region.width = TextNumber(22, TextUnitType::kPercent);
  region.height = TextNumber(33, TextUnitType::kPercent);
  region.window_anchor_x = TextNumber(20, TextUnitType::kPixels);
  region.window_anchor_y = TextNumber(40, TextUnitType::kPixels);
  properties.regions.emplace("foo", region);

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesLanguage) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"foo\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\">bar</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.body.body = "bar";
  properties.language = "foo";

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesPosition) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <region xml:id=\"_shaka_region_0\" tts:origin=\"30% 4em\" "
      "tts:extent=\"100px 1em\"/>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\" region=\"_shaka_region_0\">bar</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.settings.position.emplace(30, TextUnitType::kPercent);
  properties.settings.line.emplace(4, TextUnitType::kLines);
  properties.settings.width.emplace(100, TextUnitType::kPixels);
  properties.settings.height.emplace(1, TextUnitType::kLines);
  properties.body.body = "bar";

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesOtherSettings) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\" tts:writingMode=\"tblr\" "
      "tts:textAlign=\"end\">bar</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.settings.writing_direction =
      WritingDirection::kVerticalGrowingRight;
  properties.settings.text_alignment = TextAlignment::kEnd;
  properties.body.body = "bar";

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesCueId) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\" xml:id=\"foo\">bar</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.id = "foo";
  properties.body.body = "bar";

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, EscapesSpecialChars) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" "
      "xml:lang=\"foo&amp;&quot;a\">\n"
      "  <head>\n"
      "    <region xml:id=\"&lt;a&amp;&quot;\" tts:origin=\"0% 0%\" "
      "tts:extent=\"100% 100%\"/>\n"
      "  </head>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\" xml:id=\"foo&lt;a&amp;&quot;\" "
      "region=\"&lt;a&amp;&quot;\">&lt;tag&gt;\"foo&amp;bar\"</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.id = "foo<a&\"";
  properties.settings.region = "<a&\"";
  properties.body.body = "<tag>\"foo&bar\"";
  properties.language = "foo&\"a";
  properties.regions.emplace("<a&\"", TextRegion());

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, HandlesReset) {
  const char* kExpectedOutput1 =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"foobar\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\">foo</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";
  const char* kExpectedOutput2 =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"foobar\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:08.000\" "
      "end=\"00:00:09.000\">bar</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TtmlGenerator generator;
  generator.Initialize({}, "foobar", kMsTimeScale);
  generator.AddSample(TextSample(kNoId, 5000, 6000, DefaultSettings(),
                                 TextFragment(kNoStyles, "foo")));

  std::string results;
  ASSERT_TRUE(generator.Dump(&results));
  ASSERT_EQ(results, kExpectedOutput1);

  results.clear();
  generator.Reset();
  generator.AddSample(TextSample(kNoId, 8000, 9000, DefaultSettings(),
                                 TextFragment(kNoStyles, "bar")));

  ASSERT_TRUE(generator.Dump(&results));
  ASSERT_EQ(results, kExpectedOutput2);
}

TEST_F(TtmlMuxerTest, HandlesImage) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\" "
      "xmlns:smpte=\"http://www.smpte-ra.org/schemas/2052-1/2010/smpte-tt\">\n"
      "  <head/>\n"
      "  <metadata>\n"
      "    <smpte:image imageType=\"PNG\" encoding=\"Base64\" xml:id=\"img_1\">"
      "AQID</smpte:image>\n"
      "  </metadata>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:05.000\" "
      "end=\"00:00:06.000\" smpte:backgroundImage=\"#img_1\" xml:id=\"foo\"/>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.id = "foo";
  properties.body.image = {1, 2, 3};

  ParseSingleCue(kExpectedOutput, properties);
}

TEST_F(TtmlMuxerTest, FormatsTimeWithFixedNumberOfDigits) {
  const char* kExpectedOutput =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<tt xmlns=\"http://www.w3.org/ns/ttml\" "
      "xmlns:tts=\"http://www.w3.org/ns/ttml#styling\" xml:lang=\"\">\n"
      "  <head/>\n"
      "  <body>\n"
      "    <div>\n"
      "      <p xml:space=\"preserve\" begin=\"00:00:00.000\" "
      "end=\"00:00:00.001\">payload</p>\n"
      "    </div>\n"
      "  </body>\n"
      "</tt>\n";

  TestProperties properties;
  properties.body.body = "payload";
  properties.start = 0;
  properties.end = 1;
  ParseSingleCue(kExpectedOutput, properties);
}

}  // namespace ttml
}  // namespace media
}  // namespace shaka
