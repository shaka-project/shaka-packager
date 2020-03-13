// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/period.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/mpd/base/mock_mpd_builder.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/mpd/test/xml_compare.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

namespace shaka {
namespace {
const uint32_t kDefaultPeriodId = 9u;
const double kDefaultPeriodStartTime = 5.6;
const bool kOutputPeriodDuration = true;

bool ElementEqual(const Element& lhs, const Element& rhs) {
  const bool all_equal_except_sublement_check =
      lhs.name == rhs.name && lhs.attributes.size() == rhs.attributes.size() &&
      std::equal(lhs.attributes.begin(), lhs.attributes.end(),
                 rhs.attributes.begin()) &&
      lhs.content == rhs.content &&
      lhs.subelements.size() == rhs.subelements.size();

  if (!all_equal_except_sublement_check) {
    return false;
  }

  for (size_t i = 0; i < lhs.subelements.size(); ++i) {
    if (!ElementEqual(lhs.subelements[i], rhs.subelements[i]))
      return false;
  }
  return true;
}

bool ContentProtectionElementEqual(const ContentProtectionElement& lhs,
                                   const ContentProtectionElement& rhs) {
  const bool all_equal_except_sublement_check =
      lhs.value == rhs.value && lhs.scheme_id_uri == rhs.scheme_id_uri &&
      lhs.additional_attributes.size() == rhs.additional_attributes.size() &&
      std::equal(lhs.additional_attributes.begin(),
                 lhs.additional_attributes.end(),
                 rhs.additional_attributes.begin()) &&
      lhs.subelements.size() == rhs.subelements.size();

  if (!all_equal_except_sublement_check)
    return false;

  for (size_t i = 0; i < lhs.subelements.size(); ++i) {
    if (!ElementEqual(lhs.subelements[i], rhs.subelements[i]))
      return false;
  }
  return true;
}

MATCHER_P(ContentProtectionElementEq, expected, "") {
  return ContentProtectionElementEqual(arg, expected);
}

/// A Period class that is capable of injecting mocked AdaptationSet.
class TestablePeriod : public Period {
 public:
  TestablePeriod(const MpdOptions& mpd_options)
      : Period(kDefaultPeriodId,
               kDefaultPeriodStartTime,
               mpd_options,
               &sequence_number_) {}

  MOCK_METHOD3(
      NewAdaptationSet,
      std::unique_ptr<AdaptationSet>(const std::string& lang,
                                     const MpdOptions& options,
                                     uint32_t* representation_counter));

 private:
  // Only for constructing the super class. Not used for testing.
  uint32_t sequence_number_ = 0;
};

}  // namespace

class PeriodTest : public ::testing::Test {
 public:
  PeriodTest()
      : testable_period_(mpd_options_),
        default_adaptation_set_(new StrictMock<MockAdaptationSet>()),
        default_adaptation_set_ptr_(default_adaptation_set_.get()) {}

 protected:
  MpdOptions mpd_options_;
  TestablePeriod testable_period_;
  bool content_protection_in_adaptation_set_ = true;

  // Default mock that can be used for the tests.
  std::unique_ptr<StrictMock<MockAdaptationSet>> default_adaptation_set_;
  StrictMock<MockAdaptationSet>* default_adaptation_set_ptr_;
};

TEST_F(PeriodTest, GetXml) {
  const char kVideoMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))));

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVideoMediaInfo),
                content_protection_in_adaptation_set_));

  const char kExpectedXml[] =
      "<Period id=\"9\">"
      // ContentType and Representation elements are populated after
      // Representation::Init() is called.
      "  <AdaptationSet contentType=\"\"/>"
      "</Period>";
  EXPECT_THAT(testable_period_.GetXml(!kOutputPeriodDuration).get(),
              XmlNodeEqual(kExpectedXml));
}

TEST_F(PeriodTest, DynamicMpdGetXml) {
  const char kVideoMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  mpd_options_.mpd_type = MpdType::kDynamic;

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))));

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVideoMediaInfo),
                content_protection_in_adaptation_set_));

  const char kExpectedXml[] =
      "<Period id=\"9\" start=\"PT5.6S\">"
      // ContentType and Representation elements are populated after
      // Representation::Init() is called.
      "  <AdaptationSet contentType=\"\"/>"
      "</Period>";
  EXPECT_THAT(testable_period_.GetXml(!kOutputPeriodDuration).get(),
              XmlNodeEqual(kExpectedXml));
}

TEST_F(PeriodTest, SetDurationAndGetXml) {
  const char kVideoMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))));

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVideoMediaInfo),
                content_protection_in_adaptation_set_));

  testable_period_.set_duration_seconds(100.234);

  const char kExpectedXml[] =
      "<Period id=\"9\" duration=\"PT100.234S\">"
      // ContentType and Representation elements are populated after
      // Representation::Init() is called.
      "  <AdaptationSet contentType=\"\"/>"
      "</Period>";
  EXPECT_THAT(testable_period_.GetXml(kOutputPeriodDuration).get(),
              XmlNodeEqual(kExpectedXml));
  const char kExpectedXmlSuppressDuration[] =
      "<Period id=\"9\">"
      // ContentType and Representation elements are populated after
      // Representation::Init() is called.
      "  <AdaptationSet contentType=\"\"/>"
      "</Period>";
  EXPECT_THAT(testable_period_.GetXml(!kOutputPeriodDuration).get(),
              XmlNodeEqual(kExpectedXmlSuppressDuration));
}

// Verify ForceSetSegmentAlignment is called.
TEST_F(PeriodTest, Text) {
  const char kTextMediaInfo[] =
      "text_info {\n"
      "  codec: 'ttml'\n"
      "  language: 'en'\n"
      "}\n"
      "container_type: CONTAINER_TEXT\n";

  EXPECT_CALL(testable_period_, NewAdaptationSet(Eq("en"), _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))));
  EXPECT_CALL(*default_adaptation_set_ptr_, ForceSetSegmentAlignment(true));

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kTextMediaInfo),
                content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, TrickPlayWithMatchingAdaptationSet) {
  const char kVideoMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  const char kTrickPlayMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "  playback_rate: 10\n"
      "}\n"
      "container_type: 1\n";

  std::unique_ptr<StrictMock<MockAdaptationSet>> trick_play_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* trick_play_adaptation_set_ptr = trick_play_adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))))
      .WillOnce(Return(ByMove(std::move(trick_play_adaptation_set))));

  EXPECT_CALL(*trick_play_adaptation_set_ptr,
              AddTrickPlayReference(Eq(default_adaptation_set_ptr_)));

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVideoMediaInfo),
                content_protection_in_adaptation_set_));
  ASSERT_EQ(trick_play_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kTrickPlayMediaInfo),
                content_protection_in_adaptation_set_));
}

// Verify no AdaptationSet is returned on trickplay media info.
TEST_F(PeriodTest, TrickPlayWithNoMatchingAdaptationSet) {
  const char kVideoMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  const char kVp9TrickPlayMediaInfo[] =
      "video_info {\n"
      "  codec: 'vp9'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "  playback_rate: 10\n"
      "}\n"
      "container_type: 1\n";

  std::unique_ptr<StrictMock<MockAdaptationSet>> trick_play_adaptation_set(
      new StrictMock<MockAdaptationSet>());

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))))
      .WillOnce(Return(ByMove(std::move(trick_play_adaptation_set))));

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVideoMediaInfo),
                content_protection_in_adaptation_set_));
  // A nullptr is returned if it is not able to find matching AdaptationSet.
  ASSERT_FALSE(testable_period_.GetOrCreateAdaptationSet(
      ConvertToMediaInfo(kVp9TrickPlayMediaInfo),
      content_protection_in_adaptation_set_));
}

// Don't put different audio languages or codecs in the same AdaptationSet.
TEST_F(PeriodTest, SplitAdaptationSetsByLanguageAndCodec) {
  const char kAacEnglishAudioContent[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'eng'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";
  const char kAacGermanAudioContent[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";
  const char kVorbisGermanAudioContent1[] =
      "audio_info {\n"
      "  codec: 'vorbis'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_WEBM\n"
      "media_duration_seconds: 10.5\n";
  const char kVorbisGermanAudioContent2[] =
      "audio_info {\n"
      "  codec: 'vorbis'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_WEBM\n"
      "media_duration_seconds: 10.5\n";

  std::unique_ptr<StrictMock<MockAdaptationSet>> aac_eng_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* aac_eng_adaptation_set_ptr = aac_eng_adaptation_set.get();
  std::unique_ptr<StrictMock<MockAdaptationSet>> aac_ger_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* aac_ger_adaptation_set_ptr = aac_ger_adaptation_set.get();
  std::unique_ptr<StrictMock<MockAdaptationSet>> vorbis_german_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* vorbis_german_adaptation_set_ptr = vorbis_german_adaptation_set.get();

  // We expect three AdaptationSets.
  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(aac_eng_adaptation_set))))
      .WillOnce(Return(ByMove(std::move(aac_ger_adaptation_set))))
      .WillOnce(Return(ByMove(std::move(vorbis_german_adaptation_set))));

  ASSERT_EQ(aac_eng_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kAacEnglishAudioContent),
                content_protection_in_adaptation_set_));
  ASSERT_EQ(aac_ger_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kAacGermanAudioContent),
                content_protection_in_adaptation_set_));
  ASSERT_EQ(vorbis_german_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVorbisGermanAudioContent1),
                content_protection_in_adaptation_set_));
  // The same AdaptationSet is returned.
  ASSERT_EQ(vorbis_german_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVorbisGermanAudioContent2),
                content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, GetAdaptationSets) {
  const char kContent1[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'eng'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";
  const char kContent2[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";

  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set_1(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_1_ptr = adaptation_set_1.get();
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set_2(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_2_ptr = adaptation_set_2.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set_1))))
      .WillOnce(Return(ByMove(std::move(adaptation_set_2))));

  ASSERT_EQ(adaptation_set_1_ptr, testable_period_.GetOrCreateAdaptationSet(
                                      ConvertToMediaInfo(kContent1),
                                      content_protection_in_adaptation_set_));
  EXPECT_THAT(testable_period_.GetAdaptationSets(),
              ElementsAre(adaptation_set_1_ptr));

  ASSERT_EQ(adaptation_set_2_ptr, testable_period_.GetOrCreateAdaptationSet(
                                      ConvertToMediaInfo(kContent2),
                                      content_protection_in_adaptation_set_));
  EXPECT_THAT(testable_period_.GetAdaptationSets(),
              ElementsAre(adaptation_set_1_ptr, adaptation_set_2_ptr));
}

TEST_F(PeriodTest, OrderedByAdaptationSetId) {
  const char kContent1[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'eng'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";
  const char kContent2[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";

  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set_1(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_1_ptr = adaptation_set_1.get();
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set_2(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_2_ptr = adaptation_set_2.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set_1))))
      .WillOnce(Return(ByMove(std::move(adaptation_set_2))));

  ASSERT_EQ(adaptation_set_1_ptr, testable_period_.GetOrCreateAdaptationSet(
                                      ConvertToMediaInfo(kContent1),
                                      content_protection_in_adaptation_set_));
  ASSERT_EQ(adaptation_set_2_ptr, testable_period_.GetOrCreateAdaptationSet(
                                      ConvertToMediaInfo(kContent2),
                                      content_protection_in_adaptation_set_));

  adaptation_set_1_ptr->set_id(2);
  adaptation_set_2_ptr->set_id(1);
  const char kExpectedXml[] =
      R"(<Period id="9">)"
      // ContentType and Representation elements are populated after
      // Representation::Init() is called.
      R"(  <AdaptationSet id="1" contentType=""/>)"
      R"(  <AdaptationSet id="2" contentType=""/>)"
      R"(</Period>)";
  EXPECT_THAT(testable_period_.GetXml(!kOutputPeriodDuration).get(),
              XmlNodeEqual(kExpectedXml));
}

TEST_F(PeriodTest, AudioAdaptationSetDefaultLanguage) {
  mpd_options_.mpd_params.default_language = "en";
  const char kEnglishAudioContent[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'en'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_ptr = adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set))));
  EXPECT_CALL(*adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));
  ASSERT_EQ(adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                    ConvertToMediaInfo(kEnglishAudioContent),
                                    content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, AudioAdaptationSetNonDefaultLanguage) {
  mpd_options_.mpd_params.default_language = "fr";
  const char kEnglishAudioContent[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'en'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_ptr = adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set))));
  EXPECT_CALL(*adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain)).Times(0);
  ASSERT_EQ(adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                    ConvertToMediaInfo(kEnglishAudioContent),
                                    content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, TextAdaptationSetDefaultLanguage) {
  mpd_options_.mpd_params.default_language = "en";
  const char kEnglishTextContent[] =
      "text_info {\n"
      "  codec: 'webvtt'\n"
      "  language: 'en'\n"
      "  type: SUBTITLE\n"
      "}";
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_ptr = adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set))));
  EXPECT_CALL(*adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));
  EXPECT_CALL(*adaptation_set_ptr, ForceSetSegmentAlignment(true));
  ASSERT_EQ(adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                    ConvertToMediaInfo(kEnglishTextContent),
                                    content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, TextAdaptationSetNonDefaultLanguage) {
  mpd_options_.mpd_params.default_language = "fr";
  const char kEnglishTextContent[] =
      "text_info {\n"
      "  codec: 'webvtt'\n"
      "  language: 'en'\n"
      "  type: SUBTITLE\n"
      "}";
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_ptr = adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set))));
  EXPECT_CALL(*adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain)).Times(0);
  EXPECT_CALL(*adaptation_set_ptr, ForceSetSegmentAlignment(true));
  ASSERT_EQ(adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                    ConvertToMediaInfo(kEnglishTextContent),
                                    content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, TextAdaptationSetNonDefaultLanguageButDefaultTextLanguage) {
  mpd_options_.mpd_params.default_language = "fr";
  mpd_options_.mpd_params.default_text_language = "en";
  const char kEnglishTextContent[] =
      "text_info {\n"
      "  codec: 'webvtt'\n"
      "  language: 'en'\n"
      "  type: SUBTITLE\n"
      "}";
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_ptr = adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set))));
  EXPECT_CALL(*adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));
  EXPECT_CALL(*adaptation_set_ptr, ForceSetSegmentAlignment(true));
  ASSERT_EQ(adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                    ConvertToMediaInfo(kEnglishTextContent),
                                    content_protection_in_adaptation_set_));
}

TEST_F(PeriodTest, TextAdaptationSetDefaultLanguageButNonDefaultTextLanguage) {
  mpd_options_.mpd_params.default_language = "en";
  mpd_options_.mpd_params.default_text_language = "fr";
  const char kEnglishTextContent[] =
      "text_info {\n"
      "  codec: 'webvtt'\n"
      "  language: 'en'\n"
      "  type: SUBTITLE\n"
      "}";
  std::unique_ptr<StrictMock<MockAdaptationSet>> adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* adaptation_set_ptr = adaptation_set.get();

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(adaptation_set))));
  EXPECT_CALL(*adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain)).Times(0);
  EXPECT_CALL(*adaptation_set_ptr, ForceSetSegmentAlignment(true));
  ASSERT_EQ(adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                    ConvertToMediaInfo(kEnglishTextContent),
                                    content_protection_in_adaptation_set_));
}

class PeriodTestWithContentProtection
    : public PeriodTest,
      public ::testing::WithParamInterface<bool> {
  void SetUp() override { content_protection_in_adaptation_set_ = GetParam(); }
};

// With content_protection_adaptation_set_ == true, verify with different
// MediaInfo::ProtectedContent, two AdaptationSets should be created.
// AdaptationSets with different DRM won't be switchable.
// Otherwise, only one AdaptationSet is created.
TEST_P(PeriodTestWithContentProtection, DifferentProtectedContent) {
  // Note they both have different (bogus) pssh, like real use case.
  // default Key ID = _default_key_id_
  const char kSdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh1'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "container_type: 1\n";
  // default Key ID = .default.key.id.
  const char kHdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'anotheruuid'\n"
      "    name_version: 'SomeOtherProtection version 3'\n"
      "    pssh: 'pssh2'\n"
      "  }\n"
      "  default_key_id: '.default.key.id.'\n"
      "}\n"
      "container_type: 1\n";

  // Check that the right ContentProtectionElements for SD is created.
  // HD is the same case, so not checking.
  ContentProtectionElement mp4_protection;
  mp4_protection.scheme_id_uri = "urn:mpeg:dash:mp4protection:2011";
  mp4_protection.value = "cenc";
  // This should match the "_default_key_id_" above, but taking it as hex data
  // and converted to UUID format.
  mp4_protection.additional_attributes["cenc:default_KID"] =
      "5f646566-6175-6c74-5f6b-65795f69645f";
  ContentProtectionElement sd_my_drm;
  sd_my_drm.scheme_id_uri = "urn:uuid:myuuid";
  sd_my_drm.value = "MyContentProtection version 1";
  Element cenc_pssh;
  cenc_pssh.name = "cenc:pssh";
  cenc_pssh.content = "cHNzaDE=";  // Base64 encoding of 'pssh1'.
  sd_my_drm.subelements.push_back(cenc_pssh);

  // Not using default mocks in this test so that we can keep track of
  // mocks by named mocks.
  std::unique_ptr<StrictMock<MockAdaptationSet>> sd_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* sd_adaptation_set_ptr = sd_adaptation_set.get();
  std::unique_ptr<StrictMock<MockAdaptationSet>> hd_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* hd_adaptation_set_ptr = hd_adaptation_set.get();

  InSequence in_sequence;

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(sd_adaptation_set))));

  if (content_protection_in_adaptation_set_) {
    EXPECT_CALL(*sd_adaptation_set_ptr,
                AddContentProtectionElement(
                    ContentProtectionElementEq(mp4_protection)));
    EXPECT_CALL(
        *sd_adaptation_set_ptr,
        AddContentProtectionElement(ContentProtectionElementEq(sd_my_drm)));

    EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
        .WillOnce(Return(ByMove(std::move(hd_adaptation_set))));

    // Add main Role here for both.
    EXPECT_CALL(*sd_adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));
    EXPECT_CALL(*hd_adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));

    // Called twice for the same reason as above.
    EXPECT_CALL(*hd_adaptation_set_ptr, AddContentProtectionElement(_))
        .Times(2);
  }

  ASSERT_EQ(sd_adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                       ConvertToMediaInfo(kSdProtectedContent),
                                       content_protection_in_adaptation_set_));
  ASSERT_EQ(content_protection_in_adaptation_set_ ? hd_adaptation_set_ptr
                                                  : sd_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kHdProtectedContent),
                content_protection_in_adaptation_set_));
}

// Verify with the same MediaInfo::ProtectedContent, only one AdaptationSets
// should be created regardless of the value of
// content_protection_in_adaptation_set_.
TEST_P(PeriodTestWithContentProtection, SameProtectedContent) {
  // These have the same default key ID and PSSH.
  const char kSdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'psshbox'\n"
      "  }\n"
      "  default_key_id: '.DEFAULT.KEY.ID.'\n"
      "}\n"
      "container_type: 1\n";
  const char kHdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'psshbox'\n"
      "  }\n"
      "  default_key_id: '.DEFAULT.KEY.ID.'\n"
      "}\n"
      "container_type: 1\n";

  ContentProtectionElement mp4_protection;
  mp4_protection.scheme_id_uri = "urn:mpeg:dash:mp4protection:2011";
  mp4_protection.value = "cenc";
  // This should match the ".DEFAULT.KEY.ID." above, but taking it as hex data
  // and converted to UUID format.
  mp4_protection.additional_attributes["cenc:default_KID"] =
      "2e444546-4155-4c54-2e4b-45592e49442e";
  ContentProtectionElement my_drm;
  my_drm.scheme_id_uri = "urn:uuid:myuuid";
  my_drm.value = "MyContentProtection version 1";
  Element cenc_pssh;
  cenc_pssh.name = "cenc:pssh";
  cenc_pssh.content = "cHNzaGJveA==";  // Base64 encoding of 'psshbox'.
  my_drm.subelements.push_back(cenc_pssh);

  InSequence in_sequence;

  // Only called once.
  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(default_adaptation_set_))));

  if (content_protection_in_adaptation_set_) {
    EXPECT_CALL(*default_adaptation_set_ptr_,
                AddContentProtectionElement(
                    ContentProtectionElementEq(mp4_protection)));
    EXPECT_CALL(
        *default_adaptation_set_ptr_,
        AddContentProtectionElement(ContentProtectionElementEq(my_drm)));
  }

  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kSdProtectedContent),
                content_protection_in_adaptation_set_));
  ASSERT_EQ(default_adaptation_set_ptr_,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kHdProtectedContent),
                content_protection_in_adaptation_set_));
}

// With content_protection_in_adaptation_set_ == true,
//   Default Key IDs are different but if the content protection UUIDs match,
//   then the AdaptationSet they belong to should be switchable. This is a long
//   test. Basically this
//   1. Add an SD protected content. This should make an AdaptationSet.
//   2. Add an HD protected content. This should make another AdaptationSet that
//      is different from the SD version. SD AdaptationSet and HD AdaptationSet
//      should be switchable.
//   3. Add a 4k protected content. This should also make a new AdaptationSet.
//      It should be switchable with SD/HD AdaptationSet.
// Otherwise only one AdaptationSet is created.
TEST_P(PeriodTestWithContentProtection, SetAdaptationSetSwitching) {
  // These have the same default key ID and PSSH.
  const char kSdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_sd'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "container_type: 1\n";
  const char kHdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_hd'\n"
      "  }\n"
      "  default_key_id: '.DEFAULT.KEY.ID.'\n"
      "}\n"
      "container_type: 1\n";

  const uint32_t kSdAdaptationSetId = 6u;
  const uint32_t kHdAdaptationSetId = 7u;
  std::unique_ptr<StrictMock<MockAdaptationSet>> sd_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* sd_adaptation_set_ptr = sd_adaptation_set.get();
  sd_adaptation_set_ptr->set_id(kSdAdaptationSetId);
  std::unique_ptr<StrictMock<MockAdaptationSet>> hd_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* hd_adaptation_set_ptr = hd_adaptation_set.get();
  hd_adaptation_set_ptr->set_id(kHdAdaptationSetId);

  InSequence in_sequence;

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(sd_adaptation_set))));

  if (content_protection_in_adaptation_set_) {
    EXPECT_CALL(*sd_adaptation_set_ptr, AddContentProtectionElement(_))
        .Times(2);

    EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
        .WillOnce(Return(ByMove(std::move(hd_adaptation_set))));

    // Add main Role here for both.
    EXPECT_CALL(*sd_adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));
    EXPECT_CALL(*hd_adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));

    EXPECT_CALL(*hd_adaptation_set_ptr, AddContentProtectionElement(_))
        .Times(2);

    EXPECT_CALL(*sd_adaptation_set_ptr,
                AddAdaptationSetSwitching(hd_adaptation_set_ptr));
    EXPECT_CALL(*hd_adaptation_set_ptr,
                AddAdaptationSetSwitching(sd_adaptation_set_ptr));
  }

  ASSERT_EQ(sd_adaptation_set_ptr, testable_period_.GetOrCreateAdaptationSet(
                                       ConvertToMediaInfo(kSdProtectedContent),
                                       content_protection_in_adaptation_set_));
  ASSERT_EQ(content_protection_in_adaptation_set_ ? hd_adaptation_set_ptr
                                                  : sd_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kHdProtectedContent),
                content_protection_in_adaptation_set_));

  // Add another content that has the same protected content and make sure that
  // adaptation set switching is set correctly.
  const char k4kProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 4096\n"
      "  height: 2160\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_4k'\n"
      "  }\n"
      "  default_key_id: 'some16bytestring'\n"
      "}\n"
      "container_type: 1\n";

  const uint32_t k4kAdaptationSetId = 4000u;
  std::unique_ptr<StrictMock<MockAdaptationSet>> fourk_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* fourk_adaptation_set_ptr = fourk_adaptation_set.get();
  fourk_adaptation_set_ptr->set_id(k4kAdaptationSetId);

  if (content_protection_in_adaptation_set_) {
    EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
        .WillOnce(Return(ByMove(std::move(fourk_adaptation_set))));

    EXPECT_CALL(*fourk_adaptation_set_ptr, AddRole(AdaptationSet::kRoleMain));
    EXPECT_CALL(*fourk_adaptation_set_ptr, AddContentProtectionElement(_))
        .Times(2);

    EXPECT_CALL(*sd_adaptation_set_ptr,
                AddAdaptationSetSwitching(fourk_adaptation_set_ptr));
    EXPECT_CALL(*fourk_adaptation_set_ptr,
                AddAdaptationSetSwitching(sd_adaptation_set_ptr));
    EXPECT_CALL(*hd_adaptation_set_ptr,
                AddAdaptationSetSwitching(fourk_adaptation_set_ptr));
    EXPECT_CALL(*fourk_adaptation_set_ptr,
                AddAdaptationSetSwitching(hd_adaptation_set_ptr));
  }

  ASSERT_EQ(content_protection_in_adaptation_set_ ? fourk_adaptation_set_ptr
                                                  : sd_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(k4kProtectedContent),
                content_protection_in_adaptation_set_));
}

// Even if the UUIDs match, video and audio AdaptationSets should not be
// switchable.
TEST_P(PeriodTestWithContentProtection,
       DoNotSetAdaptationSetSwitchingIfContentTypesDifferent) {
  // These have the same default key ID and PSSH.
  const char kVideoContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_video'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "container_type: 1\n";
  const char kAudioContent[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_audio'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: 1\n"
      "media_duration_seconds: 10.5\n";

  const uint32_t kVideoAdaptationSetId = 6u;
  const uint32_t kAudioAdaptationSetId = 7u;
  std::unique_ptr<StrictMock<MockAdaptationSet>> video_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* video_adaptation_set_ptr = video_adaptation_set.get();
  video_adaptation_set_ptr->set_id(kVideoAdaptationSetId);
  std::unique_ptr<StrictMock<MockAdaptationSet>> audio_adaptation_set(
      new StrictMock<MockAdaptationSet>());
  auto* audio_adaptation_set_ptr = audio_adaptation_set.get();
  audio_adaptation_set_ptr->set_id(kAudioAdaptationSetId);

  InSequence in_sequence;

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(video_adaptation_set))));
  if (content_protection_in_adaptation_set_) {
    EXPECT_CALL(*video_adaptation_set_ptr, AddContentProtectionElement(_))
        .Times(2);
  }

  EXPECT_CALL(testable_period_, NewAdaptationSet(_, _, _))
      .WillOnce(Return(ByMove(std::move(audio_adaptation_set))));
  if (content_protection_in_adaptation_set_) {
    EXPECT_CALL(*audio_adaptation_set_ptr, AddContentProtectionElement(_))
        .Times(2);
  }

  ASSERT_EQ(video_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kVideoContent),
                content_protection_in_adaptation_set_));
  ASSERT_EQ(audio_adaptation_set_ptr,
            testable_period_.GetOrCreateAdaptationSet(
                ConvertToMediaInfo(kAudioContent),
                content_protection_in_adaptation_set_));
}

INSTANTIATE_TEST_CASE_P(ContentProtectionInAdaptationSet,
                        PeriodTestWithContentProtection,
                        ::testing::Bool());

}  // namespace shaka
