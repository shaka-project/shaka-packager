// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/period.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/version/version.h"

namespace shaka {

namespace {

class TestClock : public base::Clock {
 public:
  explicit TestClock(const base::Time& t) : time_(t) {}
  ~TestClock() override {}
  base::Time Now() override { return time_; }

 private:
  base::Time time_;
};

}  // namespace

template <DashProfile profile>
class MpdBuilderTest : public ::testing::Test {
 public:
  MpdBuilderTest() : mpd_(MpdOptions()), representation_() {
    mpd_.mpd_options_.dash_profile = profile;
  }
  ~MpdBuilderTest() override {}

  void SetUp() override {
    const double kPeriodStartTimeSeconds = 0.0;
    period_ = mpd_.GetOrCreatePeriod(kPeriodStartTimeSeconds);
    ASSERT_TRUE(period_);
  }

  MpdOptions* mutable_mpd_options() { return &mpd_.mpd_options_; }

  void CheckMpd(const std::string& expected_output_file) {
    std::string mpd_doc;
    ASSERT_TRUE(mpd_.ToString(&mpd_doc));
    ASSERT_TRUE(ValidateMpdSchema(mpd_doc));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMpdToEqualExpectedOutputFile(mpd_doc, expected_output_file));
  }

 protected:
  // Creates a new AdaptationSet and adds a Representation element using
  // |media_info|.
  void AddRepresentation(const MediaInfo& media_info) {
    AdaptationSet* adaptation_set =
        period_->GetOrCreateAdaptationSet(media_info, true);
    ASSERT_TRUE(adaptation_set);

    Representation* representation =
        adaptation_set->AddRepresentation(media_info);
    ASSERT_TRUE(representation);

    representation_ = representation;
  }

  MpdBuilder mpd_;

  // We usually need only one representation.
  Representation* representation_;  // Owned by |mpd_|.

 private:
  Period* period_ = nullptr;
  base::AtomicSequenceNumber representation_counter_;

  DISALLOW_COPY_AND_ASSIGN(MpdBuilderTest);
};

class OnDemandMpdBuilderTest : public MpdBuilderTest<DashProfile::kOnDemand> {};

class LiveMpdBuilderTest : public MpdBuilderTest<DashProfile::kLive> {
 public:
  ~LiveMpdBuilderTest() override {}

  // Anchors availabilityStartTime so that the test result doesn't depend on the
  // current time.
  void SetUp() override {
    SetPackagerVersionForTesting("<tag>-<hash>-<test>");
    mpd_.mpd_options_.mpd_type = MpdType::kDynamic;
    mpd_.availability_start_time_ = "2011-12-25T12:30:00";
    InjectTestClock();
  }

  // Injects a clock that always returns 2016 Jan 11 15:10:24 in UTC.
  void InjectTestClock() {
    base::Time::Exploded test_time = { 2016,  // year.
                                       1,  // month
                                       1,  // day_of_week = Monday.
                                       11,  // day_of_month
                                       15,  // hour.
                                       10,  // minute.
                                       24,  // second.
                                       0 };  // millisecond.
    ASSERT_TRUE(test_time.HasValidValues());
    mpd_.InjectClockForTesting(std::unique_ptr<base::Clock>(
        new TestClock(base::Time::FromUTCExploded(test_time))));
  }
};

// Add one video check the output.
TEST_F(OnDemandMpdBuilderTest, Video) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  ASSERT_NO_FATAL_FAILURE(AddRepresentation(video_media_info));
  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputVideo1));
}

TEST_F(OnDemandMpdBuilderTest, TwoVideosWithDifferentResolutions) {
  MediaInfo media_info1 = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  MediaInfo media_info2 = GetTestMediaInfo(kFileNameVideoMediaInfo2);
  // The order matters here to check against expected output.
  ASSERT_NO_FATAL_FAILURE(AddRepresentation(media_info1));
  ASSERT_NO_FATAL_FAILURE(AddRepresentation(media_info2));

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputVideo1And2));
}

// Add both video and audio and check the output.
TEST_F(OnDemandMpdBuilderTest, VideoAndAudio) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  MediaInfo audio_media_info = GetTestMediaInfo(kFileNameAudioMediaInfo1);
  // The order matters here to check against expected output.
  ASSERT_NO_FATAL_FAILURE(AddRepresentation(video_media_info));
  ASSERT_NO_FATAL_FAILURE(AddRepresentation(audio_media_info));

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputAudio1AndVideo1));
}

// Static profile requires bandwidth to be set because it has no other way to
// get the bandwidth for the Representation.
TEST_F(OnDemandMpdBuilderTest, MediaInfoMissingBandwidth) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  video_media_info.clear_bandwidth();
  AddRepresentation(video_media_info);

  std::string mpd_doc;
  ASSERT_FALSE(mpd_.ToString(&mpd_doc));
}

TEST_F(LiveMpdBuilderTest, MultiplePeriodTest) {
  const double kPeriodStartTimeSeconds = 1.0;
  Period* period = mpd_.GetOrCreatePeriod(kPeriodStartTimeSeconds);
  ASSERT_TRUE(period);
  ASSERT_EQ(kPeriodStartTimeSeconds, period->start_time_in_seconds());

  const double kPeriodStartTimeSeconds2 = 1.1;
  Period* period2 = mpd_.GetOrCreatePeriod(kPeriodStartTimeSeconds2);
  ASSERT_TRUE(period2);
  // The old Period is re-used if they are closed to each other.
  ASSERT_EQ(period, period2);
  ASSERT_EQ(kPeriodStartTimeSeconds, period2->start_time_in_seconds());

  const double kPeriodStartTimeSeconds3 = 5.0;
  Period* period3 = mpd_.GetOrCreatePeriod(kPeriodStartTimeSeconds3);
  ASSERT_TRUE(period3);
  ASSERT_NE(period, period3);
  ASSERT_EQ(kPeriodStartTimeSeconds3, period3->start_time_in_seconds());
}

// Check whether the attributes are set correctly for dynamic <MPD> element.
// This test must use ASSERT_EQ for comparison because XmlEqual() cannot
// handle namespaces correctly yet.
TEST_F(LiveMpdBuilderTest, DynamicCheckMpdAttributes) {
  static const char kExpectedOutput[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!--Generated with https://github.com/google/shaka-packager"
      " version <tag>-<hash>-<test>-->\n"
      "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\""
      " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
      " xsi:schemaLocation=\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\""
      " xmlns:cenc=\"urn:mpeg:cenc:2013\""
      " profiles=\"urn:mpeg:dash:profile:isoff-live:2011\""
      " minBufferTime=\"PT2S\""
      " type=\"dynamic\""
      " publishTime=\"2016-01-11T15:10:24Z\""
      " availabilityStartTime=\"2011-12-25T12:30:00\""
      " minimumUpdatePeriod=\"PT2S\"/>\n";

  std::string mpd_doc;
  mutable_mpd_options()->mpd_type = MpdType::kDynamic;
  mutable_mpd_options()->mpd_params.minimum_update_period = 2;
  ASSERT_TRUE(mpd_.ToString(&mpd_doc));
  ASSERT_EQ(kExpectedOutput, mpd_doc);
}

TEST_F(LiveMpdBuilderTest, StaticCheckMpdAttributes) {
  static const char kExpectedOutput[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!--Generated with https://github.com/google/shaka-packager"
      " version <tag>-<hash>-<test>-->\n"
      "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\""
      " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
      " xmlns:xlink=\"http://www.w3.org/1999/xlink\""
      " xsi:schemaLocation=\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\""
      " xmlns:cenc=\"urn:mpeg:cenc:2013\""
      " profiles=\"urn:mpeg:dash:profile:isoff-live:2011\""
      " minBufferTime=\"PT2S\""
      " type=\"static\""
      " mediaPresentationDuration=\"PT0S\"/>\n";

  std::string mpd_doc;
  mutable_mpd_options()->mpd_type = MpdType::kStatic;
  ASSERT_TRUE(mpd_.ToString(&mpd_doc));
  ASSERT_EQ(kExpectedOutput, mpd_doc);
}

namespace {
const char kMediaFile[] = "foo/bar/media.mp4";
const char kMediaFileBase[] = "media.mp4";
const char kInitSegment[] = "foo/bar/init.mp4";
const char kInitSegmentBase[] = "init.mp4";
const char kSegmentTemplate[] = "foo/bar/segment-$Number$.mp4";
const char kSegmentTemplateBase[] = "segment-$Number$.mp4";
const char kPathModifiedMpd[] = "foo/bar/media.mpd";
const char kPathNotModifiedMpd[] = "foo/baz/media.mpd";
}  // namespace

TEST(RelativePaths, PathsModified) {
  MediaInfo media_info;

  media_info.set_media_file_name(kMediaFile);
  media_info.set_init_segment_name(kInitSegment);
  media_info.set_segment_template(kSegmentTemplate);
  MpdBuilder::MakePathsRelativeToMpd(kPathModifiedMpd, &media_info);
  EXPECT_EQ(kMediaFileBase, media_info.media_file_name());
  EXPECT_EQ(kInitSegmentBase, media_info.init_segment_name());
  EXPECT_EQ(kSegmentTemplateBase, media_info.segment_template());
}

TEST(RelativePaths, PathsNotModified) {
  MediaInfo media_info;

  media_info.set_media_file_name(kMediaFile);
  media_info.set_init_segment_name(kInitSegment);
  media_info.set_segment_template(kSegmentTemplate);
  MpdBuilder::MakePathsRelativeToMpd(kPathNotModifiedMpd, &media_info);
  EXPECT_EQ(kMediaFile, media_info.media_file_name());
  EXPECT_EQ(kInitSegment, media_info.init_segment_name());
  EXPECT_EQ(kSegmentTemplate, media_info.segment_template());
}

}  // namespace shaka
