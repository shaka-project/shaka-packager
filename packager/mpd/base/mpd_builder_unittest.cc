// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <inttypes.h>
#include <libxml/xmlstring.h>

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_piece.h"
#include "packager/base/strings/string_util.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/content_protection_element.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_utils.h"
#include "packager/mpd/base/representation.h"
#include "packager/mpd/base/xml/scoped_xml_ptr.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/mpd/test/xml_compare.h"
#include "packager/version/version.h"

namespace shaka {

using base::FilePath;
using ::testing::Not;

namespace {
// Any number for {AdaptationSet,Representation} ID. Required to create
// either objects. Not checked in test.
const uint32_t kAnyRepresentationId = 1;
const uint32_t kAnyAdaptationSetId = 1;
const char kNoLanguage[] = "";
const char kSElementTemplate[] =
    "<S t=\"%" PRIu64 "\" d=\"%" PRIu64 "\" r=\"%" PRIu64 "\"/>\n";
const char kSElementTemplateWithoutR[] =
    "<S t=\"%" PRIu64 "\" d=\"%" PRIu64 "\"/>\n";
const int kDefaultStartNumber = 1;

class TestClock : public base::Clock {
 public:
  explicit TestClock(const base::Time& t) : time_(t) {}
  ~TestClock() override {}
  base::Time Now() override { return time_; }

 private:
  base::Time time_;
};

class MockRepresentationStateChangeListener
    : public RepresentationStateChangeListener {
 public:
  MockRepresentationStateChangeListener() {}
  ~MockRepresentationStateChangeListener() {}

  MOCK_METHOD2(OnNewSegmentForRepresentation,
               void(uint64_t start_time, uint64_t duration));

  MOCK_METHOD2(OnSetFrameRateForRepresentation,
               void(uint32_t frame_duration, uint32_t timescale));
};
}  // namespace

template <DashProfile profile>
class MpdBuilderTest : public ::testing::Test {
 public:
  MpdBuilderTest() : mpd_(MpdOptions()), representation_() {
    mpd_options_.dash_profile = profile;
    mpd_.mpd_options_.dash_profile = profile;
  }
  ~MpdBuilderTest() override {}

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
    AdaptationSet* adaptation_set = mpd_.AddAdaptationSet("");
    ASSERT_TRUE(adaptation_set);

    Representation* representation =
        adaptation_set->AddRepresentation(media_info);
    ASSERT_TRUE(representation);

    representation_ = representation;
  }

  // TODO(rkuroiwa): Once std::forward() is allowed by chromium style guide, use
  // variadic template and std::forward() so that we don't need to copy the
  // constructor signatures.
  std::unique_ptr<Representation> CreateRepresentation(
      const MediaInfo& media_info,
      uint32_t representation_id,
      std::unique_ptr<RepresentationStateChangeListener>
          state_change_listener) {
    return std::unique_ptr<Representation>(
        new Representation(media_info, mpd_options_, representation_id,
                           std::move(state_change_listener)));
  }

  std::unique_ptr<AdaptationSet> CreateAdaptationSet(uint32_t adaptation_set_id,
                                                     const std::string& lang) {
    return std::unique_ptr<AdaptationSet>(new AdaptationSet(
        adaptation_set_id, lang, mpd_options_, &representation_counter_));
  }

  // Helper function to return an empty listener for tests that don't need
  // it.
  std::unique_ptr<RepresentationStateChangeListener> NoListener() {
    return std::unique_ptr<RepresentationStateChangeListener>();
  }

  MpdBuilder mpd_;

  // We usually need only one representation.
  Representation* representation_;  // Owned by |mpd_|.

 private:
  MpdOptions mpd_options_;
  base::AtomicSequenceNumber representation_counter_;

  DISALLOW_COPY_AND_ASSIGN(MpdBuilderTest);
};

class OnDemandMpdBuilderTest : public MpdBuilderTest<DashProfile::kOnDemand> {};

// Use this test name for things that are common to both static an dynamic
// mpd builder tests.
typedef OnDemandMpdBuilderTest CommonMpdBuilderTest;

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

  std::string GetDefaultMediaInfo() {
    const char kMediaInfo[] =
        "video_info {\n"
        "  codec: 'avc1.010101'\n"
        "  width: 720\n"
        "  height: 480\n"
        "  time_scale: 10\n"
        "  frame_duration: 5\n"
        "  pixel_width: 1\n"
        "  pixel_height: 1\n"
        "}\n"
        "reference_time_scale: %u\n"
        "container_type: 1\n"
        "init_segment_name: 'init.mp4'\n"
        "segment_template: '$Time$.mp4'\n";

    return base::StringPrintf(kMediaInfo, DefaultTimeScale());
  }

  // TODO(rkuroiwa): Make this a global constant in anonymous namespace.
  uint32_t DefaultTimeScale() const { return 1000; };
};

class SegmentTemplateTest : public LiveMpdBuilderTest {
 public:
  SegmentTemplateTest()
      : bandwidth_estimator_(BandwidthEstimator::kUseAllBlocks) {}
  ~SegmentTemplateTest() override {}

  void SetUp() override {
    LiveMpdBuilderTest::SetUp();
    ASSERT_NO_FATAL_FAILURE(AddRepresentationWithDefaultMediaInfo());
  }

  void AddSegments(uint64_t start_time,
                   uint64_t duration,
                   uint64_t size,
                   uint64_t repeat) {
    DCHECK(representation_);

    SegmentInfo s = {start_time, duration, repeat};
    segment_infos_for_expected_out_.push_back(s);
    if (repeat == 0) {
      expected_s_elements_ +=
          base::StringPrintf(kSElementTemplateWithoutR, start_time, duration);
    } else {
      expected_s_elements_ +=
          base::StringPrintf(kSElementTemplate, start_time, duration, repeat);
    }

    for (uint64_t i = 0; i < repeat + 1; ++i) {
      representation_->AddNewSegment(start_time, duration, size);
      start_time += duration;
      bandwidth_estimator_.AddBlock(
          size, static_cast<double>(duration) / DefaultTimeScale());
    }
  }

 protected:
  void AddRepresentationWithDefaultMediaInfo() {
    ASSERT_NO_FATAL_FAILURE(
        AddRepresentation(ConvertToMediaInfo(GetDefaultMediaInfo())));
  }

  std::string TemplateOutputInsertValues(const std::string& s_elements_string,
                                         uint64_t bandwidth) {
    // Note: Since all the tests have 1 Representation, the AdaptationSet
    // always has segmentAligntment=true.
    const char kOutputTemplate[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" "
        " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        " xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
        " xsi:schemaLocation=\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\" "
        " availabilityStartTime=\"2011-12-25T12:30:00\" "
        " profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" "
        " minBufferTime=\"PT2S\" type=\"dynamic\" "
        " publishTime=\"2016-01-11T15:10:24Z\">\n"
        "  <Period id=\"0\" start=\"PT0S\">\n"
        "    <AdaptationSet id=\"0\" width=\"720\" height=\"480\""
        "                   frameRate=\"10/5\" contentType=\"video\""
        "                   par=\"3:2\" segmentAlignment=\"true\">\n"
        "      <Representation id=\"0\" bandwidth=\"%" PRIu64 "\" "
        "       codecs=\"avc1.010101\" mimeType=\"video/mp4\" sar=\"1:1\">\n"
        "        <SegmentTemplate timescale=\"1000\" "
        "initialization=\"init.mp4\" media=\"$Time$.mp4\">\n"
        "          <SegmentTimeline>\n%s"
        "          </SegmentTimeline>\n"
        "        </SegmentTemplate>\n"
        "      </Representation>\n"
        "    </AdaptationSet>\n"
        "  </Period>\n"
        "</MPD>\n";

    return base::StringPrintf(kOutputTemplate,
                              bandwidth,
                              s_elements_string.c_str());
  }

  void CheckMpdAgainstExpectedResult() {
    std::string mpd_doc;
    ASSERT_TRUE(mpd_.ToString(&mpd_doc));
    ASSERT_TRUE(ValidateMpdSchema(mpd_doc));
    const std::string& expected_output =
        TemplateOutputInsertValues(expected_s_elements_,
                                   bandwidth_estimator_.Estimate());
    ASSERT_TRUE(XmlEqual(expected_output, mpd_doc))
        << "Expected " << expected_output << std::endl << "Actual: " << mpd_doc;
  }

  std::list<SegmentInfo> segment_infos_for_expected_out_;
  std::string expected_s_elements_;
  BandwidthEstimator bandwidth_estimator_;
};

class TimeShiftBufferDepthTest : public SegmentTemplateTest {
 public:
  TimeShiftBufferDepthTest() {}
  ~TimeShiftBufferDepthTest() override {}

  // This function is tricky. It does not call SegmentTemplateTest::Setup() so
  // that it does not automatically add a representation, that has $Time$
  // template.
  void SetUp() override {
    LiveMpdBuilderTest::SetUp();

    // The only diff with current GetDefaultMediaInfo() is that this uses
    // $Number$ for segment template.
    const char kMediaInfo[] =
        "video_info {\n"
        "  codec: 'avc1.010101'\n"
        "  width: 720\n"
        "  height: 480\n"
        "  time_scale: 10\n"
        "  frame_duration: 2\n"
        "  pixel_width: 1\n"
        "  pixel_height: 1\n"
        "}\n"
        "reference_time_scale: %u\n"
        "container_type: 1\n"
        "init_segment_name: 'init.mp4'\n"
        "segment_template: '$Number$.mp4'\n";

    const std::string& number_template_media_info =
        base::StringPrintf(kMediaInfo, DefaultTimeScale());
    ASSERT_NO_FATAL_FAILURE(
        AddRepresentation(ConvertToMediaInfo(number_template_media_info)));
  }

  void CheckTimeShiftBufferDepthResult(const std::string& expected_s_element,
                                       int expected_time_shift_buffer_depth,
                                       int expected_start_number) {
    // Note: Since all the tests have 1 Representation, the AdaptationSet
    // always has segmentAligntment=true.
    const char kOutputTemplate[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" "
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
        "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
        "xsi:schemaLocation=\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\" "
        "availabilityStartTime=\"2011-12-25T12:30:00\" "
        "profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" "
        "minBufferTime=\"PT2S\" type=\"dynamic\" "
        "publishTime=\"2016-01-11T15:10:24Z\" "
        "timeShiftBufferDepth=\"PT%dS\">\n"
        "  <Period id=\"0\" start=\"PT0S\">\n"
        "    <AdaptationSet id=\"0\" width=\"720\" height=\"480\""
        "                   frameRate=\"10/2\" contentType=\"video\""
        "                   par=\"3:2\" segmentAlignment=\"true\">\n"
        "      <Representation id=\"0\" bandwidth=\"%" PRIu64 "\" "
        "       codecs=\"avc1.010101\" mimeType=\"video/mp4\" sar=\"1:1\">\n"
        "        <SegmentTemplate timescale=\"1000\" "
        "         initialization=\"init.mp4\" media=\"$Number$.mp4\" "
        "         startNumber=\"%d\">\n"
        "          <SegmentTimeline>\n"
        "            %s\n"
        "          </SegmentTimeline>\n"
        "        </SegmentTemplate>\n"
        "      </Representation>\n"
        "    </AdaptationSet>\n"
        "  </Period>\n"
        "</MPD>\n";

    std::string expected_out =
        base::StringPrintf(kOutputTemplate,
                           expected_time_shift_buffer_depth,
                           bandwidth_estimator_.Estimate(),
                           expected_start_number,
                           expected_s_element.c_str());

    std::string mpd_doc;
    ASSERT_TRUE(mpd_.ToString(&mpd_doc));
    ASSERT_TRUE(ValidateMpdSchema(mpd_doc));
    ASSERT_TRUE(XmlEqual(expected_out, mpd_doc))
        << "Expected " << expected_out << std::endl << "Actual: " << mpd_doc;
  }
};

TEST_F(CommonMpdBuilderTest, AddAdaptationSetSwitching) {
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  adaptation_set->AddAdaptationSetSwitching(1);
  adaptation_set->AddAdaptationSetSwitching(2);
  adaptation_set->AddAdaptationSetSwitching(8);

  // The empty contentType is sort of a side effect of being able to generate an
  // MPD without adding any Representations.
  const char kExpectedOutput[] =
      "<AdaptationSet id=\"1\" contentType=\"\">"
      "  <SupplementalProperty "
      "   schemeIdUri=\"urn:mpeg:dash:adaptation-set-switching:2016\" "
      "   value=\"1,2,8\"/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput));
}

// Verify that Representation::Init() works with all "required" fields of
// MedieInfo proto.
TEST_F(CommonMpdBuilderTest, ValidMediaInfo) {
  const char kTestMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());
  EXPECT_TRUE(representation->Init());
}

// Verify that if VideoInfo, AudioInfo, or TextInfo is not set, Init() fails.
TEST_F(CommonMpdBuilderTest, VideoAudioTextInfoNotSet) {
  const char kTestMediaInfo[] = "container_type: 1";

  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());
  EXPECT_FALSE(representation->Init());
}

// Verify that if more than one of VideoInfo, AudioInfo, or TextInfo is set,
// then Init() fails.
TEST_F(CommonMpdBuilderTest, VideoAndAudioInfoSet) {
  const char kTestMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "}\n"
      "container_type: CONTAINER_MP4\n";

  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());
  EXPECT_FALSE(representation->Init());
}

// Verify that Representation::Init() fails if a required field is missing.
TEST_F(CommonMpdBuilderTest, InvalidMediaInfo) {
  // Missing width.
  const char kTestMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());
  EXPECT_FALSE(representation->Init());
}

// Basic check that the fields in video info are in the XML.
TEST_F(CommonMpdBuilderTest, CheckVideoInfoReflectedInXml) {
  const char kTestMediaInfo[] =
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
  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  const char kExpectedOutput[] =
      "<Representation id=\"1\" bandwidth=\"0\" "
      " codecs=\"avc1\" mimeType=\"video/mp4\" "
      " sar=\"1:1\" width=\"1280\" height=\"720\" "
      " frameRate=\"10/10\"/>";
  EXPECT_THAT(representation->GetXml().get(), XmlNodeEqual(kExpectedOutput));
}

TEST_F(CommonMpdBuilderTest, CheckVideoInfoVp8CodecInMp4) {
  const char kTestMediaInfoCodecVp8[] =
      "video_info {\n"
      "  codec: 'vp08.00.00.08.01.01.00.00'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTestMediaInfoCodecVp8),
                           kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(),
              AttributeEqual("codecs", "vp08.00.00.08.01.01.00.00"));
}

// Check that vp8 codec string will be updated for backward compatibility
// support in webm.
TEST_F(CommonMpdBuilderTest, CheckVideoInfoVp8CodecInWebm) {
  const char kTestMediaInfoCodecVp8[] =
      "video_info {\n"
      "  codec: 'vp08.00.00.08.01.01.00.00'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 3\n";
  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTestMediaInfoCodecVp8),
                           kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(), AttributeEqual("codecs", "vp8"));
}

// Check that vp9 codec string will be updated for backward compatibility
// support in webm.
TEST_F(CommonMpdBuilderTest, CheckVideoInfoVp9CodecInWebm) {
  const char kTestMediaInfoCodecVp9[] =
      "video_info {\n"
      "  codec: 'vp09.00.00.08.01.01.00.00'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 3\n";
  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTestMediaInfoCodecVp9),
                           kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(), AttributeEqual("codecs", "vp9"));
}

// Make sure RepresentationStateChangeListener::OnNewSegmentForRepresentation()
// is called.
TEST_F(CommonMpdBuilderTest,
       RepresentationStateChangeListenerOnNewSegmentForRepresentation) {
  const char kTestMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  const uint64_t kStartTime = 199238u;
  const uint64_t kDuration = 98u;
  std::unique_ptr<MockRepresentationStateChangeListener> listener(
      new MockRepresentationStateChangeListener());
  EXPECT_CALL(*listener, OnNewSegmentForRepresentation(kStartTime, kDuration));
  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTestMediaInfo),
                           kAnyRepresentationId, std::move(listener));
  EXPECT_TRUE(representation->Init());

  representation->AddNewSegment(kStartTime, kDuration, 10 /* any size */);
}

// Make sure
// RepresentationStateChangeListener::OnSetFrameRateForRepresentation()
// is called.
TEST_F(CommonMpdBuilderTest,
       RepresentationStateChangeListenerOnSetFrameRateForRepresentation) {
  const char kTestMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 1000\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  const uint64_t kTimeScale = 1000u;
  const uint64_t kFrameDuration = 33u;
  std::unique_ptr<MockRepresentationStateChangeListener> listener(
      new MockRepresentationStateChangeListener());
  EXPECT_CALL(*listener,
              OnSetFrameRateForRepresentation(kFrameDuration, kTimeScale));
  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTestMediaInfo),
                           kAnyRepresentationId, std::move(listener));
  EXPECT_TRUE(representation->Init());

  representation->SetSampleDuration(kFrameDuration);
}

// Verify that content type is set correctly if video info is present in
// MediaInfo.
TEST_F(CommonMpdBuilderTest, CheckAdaptationSetVideoContentType) {
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
      "container_type: CONTAINER_MP4\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo));
  EXPECT_THAT(adaptation_set->GetXml().get(),
              AttributeEqual("contentType", "video"));
}

// Verify that content type is set correctly if audio info is present in
// MediaInfo.
TEST_F(CommonMpdBuilderTest, CheckAdaptationSetAudioContentType) {
  const char kAudioMediaInfo[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "}\n"
      "container_type: CONTAINER_MP4\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  adaptation_set->AddRepresentation(ConvertToMediaInfo(kAudioMediaInfo));
  EXPECT_THAT(adaptation_set->GetXml().get(),
              AttributeEqual("contentType", "audio"));
}

// Verify that content type is set correctly if text info is present in
// MediaInfo.
TEST_F(CommonMpdBuilderTest, CheckAdaptationSetTextContentType) {
  const char kTextMediaInfo[] =
      "text_info {\n"
      "  format: 'ttml'\n"
      "  language: 'en'\n"
      "}\n"
      "container_type: CONTAINER_TEXT\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, "en");
  adaptation_set->AddRepresentation(ConvertToMediaInfo(kTextMediaInfo));
  EXPECT_THAT(adaptation_set->GetXml().get(),
              AttributeEqual("contentType", "text"));
}

TEST_F(CommonMpdBuilderTest, TtmlXmlMimeType) {
  const char kTtmlXmlMediaInfo[] =
      "text_info {\n"
      "  format: 'ttml'\n"
      "}\n"
      "container_type: CONTAINER_TEXT\n";

  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTtmlXmlMediaInfo),
                           kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(),
              AttributeEqual("mimeType", "application/ttml+xml"));
}

TEST_F(CommonMpdBuilderTest, TtmlMp4MimeType) {
  const char kTtmlMp4MediaInfo[] =
      "text_info {\n"
      "  format: 'ttml'\n"
      "}\n"
      "container_type: CONTAINER_MP4\n";

  auto representation =
      CreateRepresentation(ConvertToMediaInfo(kTtmlMp4MediaInfo),
                           kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(),
              AttributeEqual("mimeType", "application/mp4"));
}

TEST_F(CommonMpdBuilderTest, WebVttMimeType) {
  const char kWebVttMediaInfo[] =
      "text_info {\n"
      "  format: 'vtt'\n"
      "}\n"
      "container_type: CONTAINER_TEXT\n";

  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kWebVttMediaInfo), kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(),
              AttributeEqual("mimeType", "text/vtt"));
}

// Verify that language passed to the constructor sets the @lang field is set.
TEST_F(CommonMpdBuilderTest, CheckLanguageAttributeSet) {
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, "en");
  EXPECT_THAT(adaptation_set->GetXml().get(), AttributeEqual("lang", "en"));
}

// Verify that language tags with subtags can still be converted.
TEST_F(CommonMpdBuilderTest, CheckConvertLanguageWithSubtag) {
  // "por-BR" is the long tag for Brazillian Portuguese.  The short tag
  // is "pt-BR", which is what should appear in the manifest.
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, "por-BR");
  EXPECT_THAT(adaptation_set->GetXml().get(), AttributeEqual("lang", "pt-BR"));
}

TEST_F(CommonMpdBuilderTest, CheckAdaptationSetId) {
  const uint32_t kAdaptationSetId = 42;
  auto adaptation_set = CreateAdaptationSet(kAdaptationSetId, kNoLanguage);
  EXPECT_THAT(adaptation_set->GetXml().get(),
              AttributeEqual("id", std::to_string(kAdaptationSetId)));
}

// Verify AdaptationSet::AddRole() works for "main" role.
TEST_F(CommonMpdBuilderTest, AdaptationAddRoleElementMain) {
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  adaptation_set->AddRole(AdaptationSet::kRoleMain);

  // The empty contentType is sort of a side effect of being able to generate an
  // MPD without adding any Representations.
  const char kExpectedOutput[] =
      "<AdaptationSet id=\"1\" contentType=\"\">\n"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"main\"/>\n"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput));
}

// Add Role, ContentProtection, and Representation elements. Verify that
// ContentProtection -> Role -> Representation are in order.
TEST_F(CommonMpdBuilderTest, CheckContentProtectionRoleRepresentationOrder) {
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  adaptation_set->AddRole(AdaptationSet::kRoleMain);
  ContentProtectionElement any_content_protection;
  any_content_protection.scheme_id_uri = "any_scheme";
  adaptation_set->AddContentProtectionElement(any_content_protection);
  const char kAudioMediaInfo[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "}\n"
      "container_type: 1\n";
  adaptation_set->AddRepresentation(ConvertToMediaInfo(kAudioMediaInfo));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  const char kExpectedOutput[] =
      "<AdaptationSet id=\"1\" contentType=\"audio\">\n"
      "  <ContentProtection schemeIdUri=\"any_scheme\"/>\n"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"main\"/>\n"
      "  <Representation id=\"0\" bandwidth=\"0\" codecs=\"mp4a.40.2\"\n"
      "   mimeType=\"audio/mp4\" audioSamplingRate=\"44100\">\n"
      "    <AudioChannelConfiguration\n"
      "     schemeIdUri=\n"
      "      \"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\"\n"
      "     value=\"2\"/>\n"
      "  </Representation>\n"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput));
}

// Verify that if all video Representations in an AdaptationSet have the same
// frame rate, AdaptationSet also has a frameRate attribute.
TEST_F(CommonMpdBuilderTest, AdapatationSetFrameRate) {
  const char kVideoMediaInfo1[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 3\n"
      "}\n"
      "container_type: 1\n";
  const char kVideoMediaInfo2[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 3\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo1)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo2)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("frameRate", "10/3"));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("maxFrameRate")));
}

// Verify that if there are videos with different frame rates, the maxFrameRate
// is set.
TEST_F(CommonMpdBuilderTest, AdapatationSetMaxFrameRate) {
  // 30fps video.
  const char kVideoMediaInfo30fps[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";
  const char kVideoMediaInfo15fps[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 3000\n"
      "  frame_duration: 200\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kVideoMediaInfo30fps)));
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kVideoMediaInfo15fps)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(),
              AttributeEqual("maxFrameRate", "3000/100"));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("frameRate")));
}

// Verify that (max)FrameRate can be set by calling
// Representation::SetSampleDuration().
TEST_F(CommonMpdBuilderTest,
       SetAdaptationFrameRateUsingRepresentationSetSampleDuration) {
  // Note that frame duration is not set in the MediaInfos. It could be there
  // and should not affect the behavior of the program.
  // But to make it closer to a real live-profile use case,
  // the frame duration is not set in the MediaInfo, instead it is set using
  // SetSampleDuration().
  const char k480pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  pixel_width: 8\n"
      "  pixel_height: 9\n"
      "}\n"
      "container_type: 1\n";
  const char k360pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  Representation* representation_480p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k480pMediaInfo));
  Representation* representation_360p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k360pMediaInfo));

  // First, make sure that maxFrameRate nor frameRate are set because
  // frame durations were not provided in the MediaInfo.
  xml::scoped_xml_ptr<xmlNode> no_frame_rate(adaptation_set->GetXml());
  EXPECT_THAT(no_frame_rate.get(), Not(AttributeSet("maxFrameRate")));
  EXPECT_THAT(no_frame_rate.get(), Not(AttributeSet("frameRate")));

  // Then set same frame duration for the representations. (Given that the
  // time scales match).
  const uint32_t kSameFrameDuration = 3u;
  representation_480p->SetSampleDuration(kSameFrameDuration);
  representation_360p->SetSampleDuration(kSameFrameDuration);

  xml::scoped_xml_ptr<xmlNode> same_frame_rate(adaptation_set->GetXml());
  EXPECT_THAT(same_frame_rate.get(), Not(AttributeSet("maxFrameRate")));
  EXPECT_THAT(same_frame_rate.get(), AttributeEqual("frameRate", "10/3"));

  // Then set 480p to be 5fps (10/2) so that maxFrameRate is set.
  const uint32_t k5FPSFrameDuration = 2;
  static_assert(k5FPSFrameDuration < kSameFrameDuration,
                "frame_duration_must_be_shorter_for_max_frame_rate");
  representation_480p->SetSampleDuration(k5FPSFrameDuration);

  xml::scoped_xml_ptr<xmlNode> max_frame_rate(adaptation_set->GetXml());
  EXPECT_THAT(max_frame_rate.get(), AttributeEqual("maxFrameRate", "10/2"));
  EXPECT_THAT(max_frame_rate.get(), Not(AttributeSet("frameRate")));
}

// Verify that if the picture aspect ratio of all the Representations are the
// same, @par attribute is present.
TEST_F(CommonMpdBuilderTest, AdaptationSetParAllSame) {
  const char k480pVideoInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width:  854\n"
      "  height: 480\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  const char k720pVideoInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  const char k1080pVideoInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  // Note that this has non-1 pixel width and height.
  // Which makes the par 16:9.
  const char k360pVideoInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 360\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "  pixel_width: 8\n"
      "  pixel_height: 9\n"
      "}\n"
      "container_type: 1\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k480pVideoInfo)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k720pVideoInfo)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k1080pVideoInfo)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k360pVideoInfo)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("par", "16:9"));
}

// Verify that adding Representations with different par will generate
// AdaptationSet without @par.
TEST_F(CommonMpdBuilderTest, AdaptationSetParDifferent) {
  const char k16by9VideoInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  // Note that 720:360 is 2:1 where as 720p (above) is 16:9.
  const char k2by1VideoInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 360\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k16by9VideoInfo)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k2by1VideoInfo)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("par")));
}

// Verify that adding Representation without pixel_width and pixel_height will
// not set @par.
TEST_F(CommonMpdBuilderTest, AdaptationSetParUnknown) {
  const char kUknownPixelWidthAndHeight[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kUknownPixelWidthAndHeight)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("par")));
}

// Catch the case where it ends up wrong if integer division is used to check
// the frame rate.
// IOW, A/B != C/D but when using integer division A/B == C/D.
// SO, maxFrameRate should be set instead of frameRate.
TEST_F(CommonMpdBuilderTest,
       AdapatationSetMaxFrameRateIntegerDivisionEdgeCase) {
  // 11/3 != 10/3 but IntegerDiv(11,3) == IntegerDiv(10,3).
  const char kVideoMediaInfo1[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 11\n"
      "  frame_duration: 3\n"
      "}\n"
      "container_type: 1\n";
  const char kVideoMediaInfo2[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 3\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo1)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo2)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("maxFrameRate", "11/3"));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("frameRate")));
}

// Verify that Suppress*() methods work.
TEST_F(CommonMpdBuilderTest, SuppressRepresentationAttributes) {
  const char kTestMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());

  representation->SuppressOnce(Representation::kSuppressWidth);
  xml::scoped_xml_ptr<xmlNode> no_width(representation->GetXml());
  EXPECT_THAT(no_width.get(), Not(AttributeSet("width")));
  EXPECT_THAT(no_width.get(), AttributeEqual("height", "480"));
  EXPECT_THAT(no_width.get(), AttributeEqual("frameRate", "10/10"));

  representation->SuppressOnce(Representation::kSuppressHeight);
  xml::scoped_xml_ptr<xmlNode> no_height(representation->GetXml());
  EXPECT_THAT(no_height.get(), Not(AttributeSet("height")));
  EXPECT_THAT(no_height.get(), AttributeEqual("width", "720"));
  EXPECT_THAT(no_height.get(), AttributeEqual("frameRate", "10/10"));

  representation->SuppressOnce(Representation::kSuppressFrameRate);
  xml::scoped_xml_ptr<xmlNode> no_frame_rate(representation->GetXml());
  EXPECT_THAT(no_frame_rate.get(), Not(AttributeSet("frameRate")));
  EXPECT_THAT(no_frame_rate.get(), AttributeEqual("width", "720"));
  EXPECT_THAT(no_frame_rate.get(), AttributeEqual("height", "480"));
}

// Attribute values that are common to all the children Representations should
// propagate up to AdaptationSet. Otherwise, each Representation should have
// its own values.
TEST_F(CommonMpdBuilderTest, BubbleUpAttributesToAdaptationSet) {
  const char k1080p[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 30\n"
      "  frame_duration: 1\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  // Different width from the one above.
  const char kDifferentWidth[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1080\n"
      "  height: 1080\n"
      "  time_scale: 30\n"
      "  frame_duration: 1\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  // Different height from ones above
  const char kDifferentHeight[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1440\n"
      "  height: 900\n"
      "  time_scale: 30\n"
      "  frame_duration: 1\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  const char kDifferentFrameRate[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 15\n"
      "  frame_duration: 1\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(adaptation_set->AddRepresentation(ConvertToMediaInfo(k1080p)));

  xml::scoped_xml_ptr<xmlNode> all_attributes_on_adaptation_set(
      adaptation_set->GetXml());
  EXPECT_THAT(all_attributes_on_adaptation_set.get(),
              AttributeEqual("width", "1920"));
  EXPECT_THAT(all_attributes_on_adaptation_set.get(),
              AttributeEqual("height", "1080"));
  EXPECT_THAT(all_attributes_on_adaptation_set.get(),
              AttributeEqual("frameRate", "30/1"));

  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kDifferentWidth)));
  xml::scoped_xml_ptr<xmlNode> width_not_set(adaptation_set->GetXml());
  EXPECT_THAT(width_not_set.get(), Not(AttributeSet("width")));
  EXPECT_THAT(width_not_set.get(), AttributeEqual("height", "1080"));
  EXPECT_THAT(width_not_set.get(), AttributeEqual("frameRate", "30/1"));

  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kDifferentHeight)));
  xml::scoped_xml_ptr<xmlNode> width_height_not_set(adaptation_set->GetXml());
  EXPECT_THAT(width_height_not_set.get(), Not(AttributeSet("width")));
  EXPECT_THAT(width_height_not_set.get(), Not(AttributeSet("height")));
  EXPECT_THAT(width_height_not_set.get(), AttributeEqual("frameRate", "30/1"));

  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kDifferentFrameRate)));
  xml::scoped_xml_ptr<xmlNode> no_common_attributes(adaptation_set->GetXml());
  EXPECT_THAT(no_common_attributes.get(), Not(AttributeSet("width")));
  EXPECT_THAT(no_common_attributes.get(), Not(AttributeSet("height")));
  EXPECT_THAT(no_common_attributes.get(), Not(AttributeSet("frameRate")));
}

// Verify that subsegmentAlignment is set to true if all the Representations'
// segments are aligned and the MPD type is static.
// Also checking that not all Representations have to be added before calling
// AddNewSegment() on a Representation.
TEST_F(OnDemandMpdBuilderTest, SubsegmentAlignment) {
  const char k480pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 8\n"
      "  pixel_height: 9\n"
      "}\n"
      "container_type: 1\n";
  const char k360pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";

  // First use same start time and duration, and verify that subsegmentAlignment
  // is set to true.
  const uint64_t kStartTime = 0u;
  const uint64_t kDuration = 10u;
  const uint64_t kAnySize = 19834u;

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  Representation* representation_480p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k480pMediaInfo));
  // Add a subsegment immediately before adding the 360p Representation.
  // This should still work for VOD.
  representation_480p->AddNewSegment(kStartTime, kDuration, kAnySize);

  Representation* representation_360p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k360pMediaInfo));
  representation_360p->AddNewSegment(kStartTime, kDuration, kAnySize);

  xml::scoped_xml_ptr<xmlNode> aligned(adaptation_set->GetXml());
  EXPECT_THAT(aligned.get(), AttributeEqual("subsegmentAlignment", "true"));

  // Unknown because 480p has an extra subsegments.
  representation_480p->AddNewSegment(11, 20, kAnySize);
  xml::scoped_xml_ptr<xmlNode> alignment_unknown(adaptation_set->GetXml());
  EXPECT_THAT(alignment_unknown.get(),
              Not(AttributeSet("subsegmentAlignment")));

  // Add segments that make them not aligned.
  representation_360p->AddNewSegment(10, 1, kAnySize);
  representation_360p->AddNewSegment(11, 19, kAnySize);

  xml::scoped_xml_ptr<xmlNode> unaligned(adaptation_set->GetXml());
  EXPECT_THAT(unaligned.get(), Not(AttributeSet("subsegmentAlignment")));
}

// Verify that subsegmentAlignment can be force set to true.
TEST_F(OnDemandMpdBuilderTest, ForceSetsubsegmentAlignment) {
  const char k480pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 8\n"
      "  pixel_height: 9\n"
      "}\n"
      "container_type: 1\n";
  const char k360pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  Representation* representation_480p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k480pMediaInfo));
  Representation* representation_360p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k360pMediaInfo));

  // Use different starting times to make the segments "not aligned".
  const uint64_t kStartTime1 = 1u;
  const uint64_t kStartTime2 = 2u;
  static_assert(kStartTime1 != kStartTime2, "StartTimesShouldBeDifferent");
  const uint64_t kDuration = 10u;
  const uint64_t kAnySize = 19834u;
  representation_480p->AddNewSegment(kStartTime1, kDuration, kAnySize);
  representation_360p->AddNewSegment(kStartTime2, kDuration, kAnySize);
  xml::scoped_xml_ptr<xmlNode> unaligned(adaptation_set->GetXml());
  EXPECT_THAT(unaligned.get(), Not(AttributeSet("subsegmentAlignment")));

  // Then force set the segment alignment attribute to true.
  adaptation_set->ForceSetSegmentAlignment(true);
  xml::scoped_xml_ptr<xmlNode> aligned(adaptation_set->GetXml());
  EXPECT_THAT(aligned.get(), AttributeEqual("subsegmentAlignment", "true"));
}

// Verify that segmentAlignment is set to true if all the Representations
// segments' are aligned and the MPD type is dynamic.
TEST_F(LiveMpdBuilderTest, SegmentAlignment) {
  const char k480pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 8\n"
      "  pixel_height: 9\n"
      "}\n"
      "container_type: 1\n";
  const char k360pMediaInfo[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 640\n"
      "  height: 360\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  Representation* representation_480p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k480pMediaInfo));
  Representation* representation_360p =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(k360pMediaInfo));

  // First use same start time and duration, and verify that that
  // segmentAlignment is set.
  const uint64_t kStartTime = 0u;
  const uint64_t kDuration = 10u;
  const uint64_t kAnySize = 19834u;
  representation_480p->AddNewSegment(kStartTime, kDuration, kAnySize);
  representation_360p->AddNewSegment(kStartTime, kDuration, kAnySize);
  xml::scoped_xml_ptr<xmlNode> aligned(adaptation_set->GetXml());
  EXPECT_THAT(aligned.get(), AttributeEqual("segmentAlignment", "true"));

  // Add segments that make them not aligned.
  representation_480p->AddNewSegment(11, 20, kAnySize);
  representation_360p->AddNewSegment(10, 1, kAnySize);
  representation_360p->AddNewSegment(11, 19, kAnySize);

  xml::scoped_xml_ptr<xmlNode> unaligned(adaptation_set->GetXml());
  EXPECT_THAT(unaligned.get(), Not(AttributeSet("segmentAlignment")));
}

// Verify that the width and height attribute are set if all the video
// representations have the same width and height.
TEST_F(OnDemandMpdBuilderTest, AdapatationSetWidthAndHeight) {
  // Both 720p.
  const char kVideoMediaInfo1[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";
  const char kVideoMediaInfo2[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 3000\n"
      "  frame_duration: 200\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo1)));
  ASSERT_TRUE(
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kVideoMediaInfo2)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("width", "1280"));
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("height", "720"));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("maxWidth")));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("maxHeight")));
}

// Verify that the maxWidth and maxHeight attribute are set if there are
// multiple video resolutions.
TEST_F(OnDemandMpdBuilderTest, AdaptationSetMaxWidthAndMaxHeight) {
  const char kVideoMediaInfo1080p[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";
  const char kVideoMediaInfo720p[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";
  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kVideoMediaInfo1080p)));
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kVideoMediaInfo720p)));

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("maxWidth", "1920"));
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("maxHeight", "1080"));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("width")));
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("height")));
}

TEST_F(CommonMpdBuilderTest, CheckRepresentationId) {
  const MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  const uint32_t kRepresentationId = 1;

  auto representation =
      CreateRepresentation(video_media_info, kRepresentationId, NoListener());
  EXPECT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(),
              AttributeEqual("id", std::to_string(kRepresentationId)));
}

// Verify that Representation::SetSampleDuration() works by checking that
// AdaptationSet@frameRate is in the XML.
TEST_F(CommonMpdBuilderTest, SetSampleDuration) {
  // Omit frame_duration so that SetSampleDuration() will set a new frameRate.
  const char kVideoMediaInfo[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 3000\n"
      "}\n"
      "container_type: 1\n";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);

  const MediaInfo video_media_info = ConvertToMediaInfo(kVideoMediaInfo);
  Representation* representation =
      adaptation_set->AddRepresentation(video_media_info);
  EXPECT_TRUE(representation->Init());

  xml::scoped_xml_ptr<xmlNode> adaptation_set_xml(adaptation_set->GetXml());
  EXPECT_THAT(adaptation_set_xml.get(), Not(AttributeSet("frameRate")));

  representation->SetSampleDuration(2u);
  adaptation_set_xml = adaptation_set->GetXml();
  EXPECT_THAT(adaptation_set_xml.get(), AttributeEqual("frameRate", "3000/2"));
}

// Verify that AdaptationSet::AddContentProtection() and
// UpdateContentProtectionPssh() works.
TEST_F(CommonMpdBuilderTest, AdaptationSetAddContentProtectionAndUpdate) {
  const char kVideoMediaInfo1080p[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";
  ContentProtectionElement content_protection;
  content_protection.scheme_id_uri =
      "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
  content_protection.value = "some value";
  Element pssh;
  pssh.name = "cenc:pssh";
  pssh.content = "any value";
  content_protection.subelements.push_back(pssh);

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kVideoMediaInfo1080p)));
  adaptation_set->AddContentProtectionElement(content_protection);

  const char kExpectedOutput1[] =
      "<AdaptationSet id=\"1\" contentType=\"video\" width=\"1920\""
      " height=\"1080\" frameRate=\"3000/100\">"
      "  <ContentProtection"
      "   schemeIdUri=\"urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\""
      "   value=\"some value\">"
      "    <cenc:pssh>any value</cenc:pssh>"
      "  </ContentProtection>"
      "  <Representation id=\"0\" bandwidth=\"0\" codecs=\"avc1\""
      "   mimeType=\"video/mp4\"/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput1));

  adaptation_set->UpdateContentProtectionPssh(
      "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed", "new pssh value");
  const char kExpectedOutput2[] =
      "<AdaptationSet id=\"1\" contentType=\"video\" width=\"1920\""
      " height=\"1080\" frameRate=\"3000/100\">"
      "  <ContentProtection"
      "   schemeIdUri=\"urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\""
      "   value=\"some value\">"
      // TODO(rkuroiwa): Commenting this out for now because we want to remove
      // the PSSH from the MPD. Uncomment this when the player supports updating
      // pssh.
      //"    <cenc:pssh>new pssh value</cenc:pssh>"
      "  </ContentProtection>"
      "  <Representation id=\"0\" bandwidth=\"0\" codecs=\"avc1\""
      "   mimeType=\"video/mp4\"/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput2));
}

// Verify that if the ContentProtection element for the DRM without <cenc:pssh>
// element is updated via UpdateContentProtectionPssh(), the element gets added.
// TODO(rkuroiwa): Until the player supports PSSH update, we remove the pssh
// element. Rename this test once it is supported.
TEST_F(CommonMpdBuilderTest, UpdateToRemovePsshElement) {
  const char kVideoMediaInfo1080p[] =
      "video_info {\n"
      "  codec: \"avc1\"\n"
      "  width: 1920\n"
      "  height: 1080\n"
      "  time_scale: 3000\n"
      "  frame_duration: 100\n"
      "}\n"
      "container_type: 1\n";
  ContentProtectionElement content_protection;
  content_protection.scheme_id_uri =
      "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
  content_protection.value = "some value";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  ASSERT_TRUE(adaptation_set->AddRepresentation(
      ConvertToMediaInfo(kVideoMediaInfo1080p)));
  adaptation_set->AddContentProtectionElement(content_protection);

  const char kExpectedOutput1[] =
      "<AdaptationSet id=\"1\" contentType=\"video\" width=\"1920\""
      " height=\"1080\" frameRate=\"3000/100\">"
      "  <ContentProtection"
      "   schemeIdUri=\"urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\""
      "   value=\"some value\">"
      "  </ContentProtection>"
      "  <Representation id=\"0\" bandwidth=\"0\" codecs=\"avc1\""
      "   mimeType=\"video/mp4\"/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput1));

  adaptation_set->UpdateContentProtectionPssh(
      "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed", "added pssh value");
  const char kExpectedOutput2[] =
      "<AdaptationSet id=\"1\" contentType=\"video\" width=\"1920\""
      " height=\"1080\" frameRate=\"3000/100\">"
      "  <ContentProtection"
      "   schemeIdUri=\"urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\""
      "   value=\"some value\">"
      // TODO(rkuroiwa): Commenting this out for now because we want to remove
      // teh PSSH from the MPD. Uncomment this when the player supports updating
      // pssh.
      //"    <cenc:pssh>added pssh value</cenc:pssh>"
      "  </ContentProtection>"
      "  <Representation id=\"0\" bandwidth=\"0\" codecs=\"avc1\""
      "   mimeType=\"video/mp4\"/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput2));
}

// Add one video check the output.
TEST_F(OnDemandMpdBuilderTest, Video) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  ASSERT_NO_FATAL_FAILURE(AddRepresentation(video_media_info));
  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputVideo1));
}

TEST_F(OnDemandMpdBuilderTest, TwoVideosWithDifferentResolutions) {
  AdaptationSet* adaptation_set = mpd_.AddAdaptationSet("");

  MediaInfo media_info1 = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  ASSERT_TRUE(adaptation_set->AddRepresentation(media_info1));

  MediaInfo media_info2 = GetTestMediaInfo(kFileNameVideoMediaInfo2);
  ASSERT_TRUE(adaptation_set->AddRepresentation(media_info2));

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputVideo1And2));
}

// Add both video and audio and check the output.
TEST_F(OnDemandMpdBuilderTest, VideoAndAudio) {
  MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  MediaInfo audio_media_info = GetTestMediaInfo(kFileNameAudioMediaInfo1);

  // The order matters here to check against expected output.
  AdaptationSet* video_adaptation_set = mpd_.AddAdaptationSet("");
  ASSERT_TRUE(video_adaptation_set);

  AdaptationSet* audio_adaptation_set = mpd_.AddAdaptationSet("");
  ASSERT_TRUE(audio_adaptation_set);

  Representation* audio_representation =
      audio_adaptation_set->AddRepresentation(audio_media_info);
  ASSERT_TRUE(audio_representation);

  Representation* video_representation =
      video_adaptation_set->AddRepresentation(video_media_info);
  ASSERT_TRUE(video_representation);

  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputAudio1AndVideo1));
}

// MPD schema has strict ordering. AudioChannelConfiguration must appear before
// ContentProtection.
// Also test that Representation::AddContentProtection() works.
TEST_F(OnDemandMpdBuilderTest, AudioChannelConfigurationWithContentProtection) {
  const char kTestMediaInfo[] =
      "bandwidth: 195857\n"
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 44100\n"
      "  num_channels: 2\n"
      "}\n"
      "init_range {\n"
      "  begin: 0\n"
      "  end: 863\n"
      "}\n"
      "index_range {\n"
      "  begin: 864\n"
      "  end: 931\n"
      "}\n"
      "media_file_name: 'encrypted_audio.mp4'\n"
      "media_duration_seconds: 24.009434\n"
      "reference_time_scale: 44100\n"
      "container_type: CONTAINER_MP4\n";

  const char kExpectedOutput[] =
      "<AdaptationSet id=\"1\" contentType=\"audio\">"
      "  <Representation id=\"0\" bandwidth=\"195857\" codecs=\"mp4a.40.2\""
      "   mimeType=\"audio/mp4\" audioSamplingRate=\"44100\" "
      // Temporary attribute. Will be removed when generating final mpd.
      "   duration=\"24.0094\">"
      "    <AudioChannelConfiguration"
      "     schemeIdUri="
      "      \"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\""
      "     value=\"2\"/>"
      "    <ContentProtection schemeIdUri=\"http://foo.com/\">"
      "      <cenc:pssh>anything</cenc:pssh>"
      "    </ContentProtection>"
      "    <BaseURL>encrypted_audio.mp4</BaseURL>"
      "    <SegmentBase indexRange=\"864-931\" timescale=\"44100\">"
      "      <Initialization range=\"0-863\"/>"
      "    </SegmentBase>"
      "  </Representation>"
      "</AdaptationSet>";

  ContentProtectionElement content_protection;
  content_protection.scheme_id_uri = "http://foo.com/";
  Element pssh;
  pssh.name = "cenc:pssh";
  pssh.content = "anything";
  content_protection.subelements.push_back(pssh);

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, kNoLanguage);
  Representation* audio_representation =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kTestMediaInfo));
  ASSERT_TRUE(audio_representation);
  audio_representation->AddContentProtectionElement(content_protection);
  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput));
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

// Verify that a text path works.
TEST_F(OnDemandMpdBuilderTest, Text) {
  const char kTextMediaInfo[] =
      "text_info {\n"
      "  format: 'ttml'\n"
      "  language: 'en'\n"
      "  type: SUBTITLE\n"
      "}\n"
      "media_duration_seconds: 35\n"
      "bandwidth: 1000\n"
      "media_file_name: 'subtitle.xml'\n"
      "container_type: CONTAINER_TEXT\n";

  const char kExpectedOutput[] =
      "<AdaptationSet id=\"1\" contentType=\"text\" lang=\"en\">"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\""
      "   value=\"subtitle\"/>\n"
      "  <Representation id=\"0\" bandwidth=\"1000\""
      "   mimeType=\"application/ttml+xml\" "
      // Temporary attribute. Will be removed when generating final mpd.
      "   duration=\"35\">"
      "    <BaseURL>subtitle.xml</BaseURL>"
      "  </Representation>"
      "</AdaptationSet>";

  auto adaptation_set = CreateAdaptationSet(kAnyAdaptationSetId, "en");
  Representation* text_representation =
      adaptation_set->AddRepresentation(ConvertToMediaInfo(kTextMediaInfo));
  ASSERT_TRUE(text_representation);

  EXPECT_THAT(adaptation_set->GetXml().get(), XmlNodeEqual(kExpectedOutput));
}

// Check whether the attributes are set correctly for dynamic <MPD> element.
// This test must use ASSERT_EQ for comparison because XmlEqual() cannot
// handle namespaces correctly yet.
TEST_F(LiveMpdBuilderTest, DynamicCheckMpdAttributes) {
  static const char kExpectedOutput[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!--Generated with https://github.com/google/shaka-packager "
      "version <tag>-<hash>-<test>-->\n"
      "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" "
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
      "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
      "xsi:schemaLocation="
      "\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\" "
      "xmlns:cenc=\"urn:mpeg:cenc:2013\" "
      "profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" "
      "minBufferTime=\"PT2S\" "
      "type=\"dynamic\" "
      "publishTime=\"2016-01-11T15:10:24Z\" "
      "availabilityStartTime=\"2011-12-25T12:30:00\">\n"
      "  <Period id=\"0\" start=\"PT0S\"/>\n"
      "</MPD>\n";

  std::string mpd_doc;
  mutable_mpd_options()->mpd_type = MpdType::kDynamic;
  ASSERT_TRUE(mpd_.ToString(&mpd_doc));
  ASSERT_EQ(kExpectedOutput, mpd_doc);
}

TEST_F(LiveMpdBuilderTest, StaticCheckMpdAttributes) {
  static const char kExpectedOutput[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!--Generated with https://github.com/google/shaka-packager "
      "version <tag>-<hash>-<test>-->\n"
      "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" "
      "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
      "xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
      "xsi:schemaLocation="
      "\"urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd\" "
      "xmlns:cenc=\"urn:mpeg:cenc:2013\" "
      "profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" "
      "minBufferTime=\"PT2S\" "
      "type=\"static\" "
      "mediaPresentationDuration=\"PT0S\">\n"
      "  <Period id=\"0\"/>\n"
      "</MPD>\n";

  std::string mpd_doc;
  mutable_mpd_options()->mpd_type = MpdType::kStatic;
  ASSERT_TRUE(mpd_.ToString(&mpd_doc));
  ASSERT_EQ(kExpectedOutput, mpd_doc);
}

// Estimate the bandwidth given the info from AddNewSegment().
TEST_F(SegmentTemplateTest, OneSegmentNormal) {
  const uint64_t kStartTime = 0;
  const uint64_t kDuration = 10;
  const uint64_t kSize = 128;
  AddSegments(kStartTime, kDuration, kSize, 0);

  // TODO(rkuroiwa): Clean up the test/data directory. It's a mess.
  EXPECT_NO_FATAL_FAILURE(CheckMpd(kFileNameExpectedMpdOutputDynamicNormal));
}

TEST_F(SegmentTemplateTest, NormalRepeatedSegmentDuration) {
  const uint64_t kSize = 256;
  uint64_t start_time = 0;
  uint64_t duration = 40000;
  uint64_t repeat = 2;
  AddSegments(start_time, duration, kSize, repeat);

  start_time += duration * (repeat + 1);
  duration = 54321;
  repeat = 0;
  AddSegments(start_time, duration, kSize, repeat);

  start_time += duration * (repeat + 1);
  duration = 12345;
  repeat = 0;
  AddSegments(start_time, duration, kSize, repeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

TEST_F(SegmentTemplateTest, RepeatedSegmentsFromNonZeroStartTime) {
  const uint64_t kSize = 100000;
  uint64_t start_time = 0;
  uint64_t duration = 100000;
  uint64_t repeat = 2;
  AddSegments(start_time, duration, kSize, repeat);

  start_time += duration * (repeat + 1);
  duration = 20000;
  repeat = 3;
  AddSegments(start_time, duration, kSize, repeat);

  start_time += duration * (repeat + 1);
  duration = 32123;
  repeat = 3;
  AddSegments(start_time, duration, kSize, repeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

// Segments not starting from 0.
// Start time is 10. Make sure r gets set correctly.
TEST_F(SegmentTemplateTest, NonZeroStartTime) {
  const uint64_t kStartTime = 10;
  const uint64_t kDuration = 22000;
  const uint64_t kSize = 123456;
  const uint64_t kRepeat = 1;
  AddSegments(kStartTime, kDuration, kSize, kRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

// There is a gap in the segments, but still valid.
TEST_F(SegmentTemplateTest, NonContiguousLiveInfo) {
  const uint64_t kStartTime = 10;
  const uint64_t kDuration = 22000;
  const uint64_t kSize = 123456;
  const uint64_t kRepeat = 0;
  AddSegments(kStartTime, kDuration, kSize, kRepeat);

  const uint64_t kStartTimeOffset = 100;
  AddSegments(kDuration + kStartTimeOffset, kDuration, kSize, kRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

// Add segments out of order. Segments that start before the previous segment
// cannot be added.
TEST_F(SegmentTemplateTest, OutOfOrder) {
  const uint64_t kEarlierStartTime = 0;
  const uint64_t kLaterStartTime = 1000;
  const uint64_t kDuration = 1000;
  const uint64_t kSize = 123456;
  const uint64_t kRepeat = 0;

  AddSegments(kLaterStartTime, kDuration, kSize, kRepeat);
  AddSegments(kEarlierStartTime, kDuration, kSize, kRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

// No segments should be overlapping.
TEST_F(SegmentTemplateTest, OverlappingSegments) {
  const uint64_t kEarlierStartTime = 0;
  const uint64_t kDuration = 1000;
  const uint64_t kSize = 123456;
  const uint64_t kRepeat = 0;

  const uint64_t kOverlappingSegmentStartTime = kDuration / 2;
  CHECK_GT(kDuration, kOverlappingSegmentStartTime);

  AddSegments(kEarlierStartTime, kDuration, kSize, kRepeat);
  AddSegments(kOverlappingSegmentStartTime, kDuration, kSize, kRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

// Some segments can be overlapped due to rounding errors. As long as it falls
// in the range of rounding error defined inside MpdBuilder, the segment gets
// accepted.
TEST_F(SegmentTemplateTest, OverlappingSegmentsWithinErrorRange) {
  const uint64_t kEarlierStartTime = 0;
  const uint64_t kDuration = 1000;
  const uint64_t kSize = 123456;
  const uint64_t kRepeat = 0;

  const uint64_t kOverlappingSegmentStartTime = kDuration - 1;
  CHECK_GT(kDuration, kOverlappingSegmentStartTime);

  AddSegments(kEarlierStartTime, kDuration, kSize, kRepeat);
  AddSegments(kOverlappingSegmentStartTime, kDuration, kSize, kRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckMpdAgainstExpectedResult());
}

// All segments have the same duration and size.
TEST_F(TimeShiftBufferDepthTest, Normal) {
  const int kTimeShiftBufferDepth = 10;  // 10 sec.
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  // Trick to make every segment 1 second long.
  const uint64_t kDuration = DefaultTimeScale();
  const uint64_t kSize = 10000;
  const uint64_t kRepeat = 1234;
  const uint64_t kLength = kRepeat;

  CHECK_EQ(kDuration / DefaultTimeScale() * kRepeat, kLength);

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  // There should only be the last 11 segments because timeshift is 10 sec and
  // each segment is 1 sec and the latest segments start time is "current
  // time" i.e., the latest segment does not count as part of timeshift buffer
  // depth.
  // Also note that S@r + 1 is the actual number of segments.
  const int kExpectedRepeatsLeft = kTimeShiftBufferDepth;
  const std::string expected_s_element =
      base::StringPrintf(kSElementTemplate,
                         kDuration * (kRepeat - kExpectedRepeatsLeft),
                         kDuration,
                         static_cast<uint64_t>(kExpectedRepeatsLeft));

  const int kExpectedStartNumber = kRepeat - kExpectedRepeatsLeft + 1;
  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element, kTimeShiftBufferDepth, kExpectedStartNumber));
}

// TimeShiftBufferDepth is shorter than a segment. This should not discard the
// segment that can play TimeShiftBufferDepth.
// For example if TimeShiftBufferDepth = 1 min. and a 10 min segment was just
// added. Before that 9 min segment was added. The 9 min segment should not be
// removed from the MPD.
TEST_F(TimeShiftBufferDepthTest, TimeShiftBufferDepthShorterThanSegmentLength) {
  const int kTimeShiftBufferDepth = 10;  // 10 sec.
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  // Each duration is a second longer than timeShiftBufferDepth.
  const uint64_t kDuration = DefaultTimeScale() * (kTimeShiftBufferDepth + 1);
  const uint64_t kSize = 10000;
  const uint64_t kRepeat = 1;

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  // The two segments should be both present.
  const std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kInitialStartTime, kDuration, kRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element, kTimeShiftBufferDepth, kDefaultStartNumber));
}

// More generic version the normal test.
TEST_F(TimeShiftBufferDepthTest, Generic) {
  const int kTimeShiftBufferDepth = 30;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 123;
  const uint64_t kDuration = DefaultTimeScale();
  const uint64_t kSize = 10000;
  const uint64_t kRepeat = 1000;

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);
  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration * (kRepeat + 1);

  // Now add 2 kTimeShiftBufferDepth long segments.
  const int kNumMoreSegments = 2;
  const int kMoreSegmentsRepeat = kNumMoreSegments - 1;
  const uint64_t kTimeShiftBufferDepthDuration =
      DefaultTimeScale() * kTimeShiftBufferDepth;
  AddSegments(first_s_element_end_time,
              kTimeShiftBufferDepthDuration,
              kSize,
              kMoreSegmentsRepeat);

  // Expect only the latest S element with 2 segments.
  const std::string expected_s_element =
      base::StringPrintf(kSElementTemplate,
                         first_s_element_end_time,
                         kTimeShiftBufferDepthDuration,
                         static_cast<uint64_t>(kMoreSegmentsRepeat));

  const int kExpectedRemovedSegments = kRepeat + 1;
  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element,
      kTimeShiftBufferDepth,
      kDefaultStartNumber + kExpectedRemovedSegments));
}

// More than 1 S element in the result.
// Adds 100 one-second segments. Then add 21 two-second segments.
// This should have all of the two-second segments and 60 one-second
// segments. Note that it expects 60 segments from the first S element because
// the most recent segment added does not count
TEST_F(TimeShiftBufferDepthTest, MoreThanOneS) {
  const int kTimeShiftBufferDepth = 100;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kSize = 20000;

  const uint64_t kOneSecondDuration = DefaultTimeScale();
  const uint64_t kOneSecondSegmentRepeat = 99;
  AddSegments(
      kInitialStartTime, kOneSecondDuration, kSize, kOneSecondSegmentRepeat);
  const uint64_t first_s_element_end_time =
      kInitialStartTime + kOneSecondDuration * (kOneSecondSegmentRepeat + 1);

  const uint64_t kTwoSecondDuration = 2 * DefaultTimeScale();
  const uint64_t kTwoSecondSegmentRepeat = 20;
  AddSegments(first_s_element_end_time,
              kTwoSecondDuration,
              kSize,
              kTwoSecondSegmentRepeat);

  const uint64_t kExpectedRemovedSegments =
      (kOneSecondSegmentRepeat + 1 + kTwoSecondSegmentRepeat * 2) -
      kTimeShiftBufferDepth;

  std::string expected_s_element =
      base::StringPrintf(kSElementTemplate,
                         kOneSecondDuration * kExpectedRemovedSegments,
                         kOneSecondDuration,
                         kOneSecondSegmentRepeat - kExpectedRemovedSegments);
  expected_s_element += base::StringPrintf(kSElementTemplate,
                                           first_s_element_end_time,
                                           kTwoSecondDuration,
                                           kTwoSecondSegmentRepeat);

  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element,
      kTimeShiftBufferDepth,
      kDefaultStartNumber + kExpectedRemovedSegments));
}


// Edge case where the last segment in S element should still be in the MPD.
// Example:
// Assuming timescale = 1 so that duration of 1 means 1 second.
// TimeShiftBufferDepth is 9 sec and we currently have
// <S t=0 d=1.5 r=1 />
// <S t=3 d=2 r=3 />
// and we add another contiguous 2 second segment.
// Then the first S element's last segment should still be in the MPD.
TEST_F(TimeShiftBufferDepthTest, UseLastSegmentInS) {
  const int kTimeShiftBufferDepth = 9;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 1;
  const uint64_t kDuration1 = static_cast<uint64_t>(DefaultTimeScale() * 1.5);
  const uint64_t kSize = 20000;
  const uint64_t kRepeat1 = 1;

  AddSegments(kInitialStartTime, kDuration1, kSize, kRepeat1);

  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration1 * (kRepeat1 + 1);

  const uint64_t kTwoSecondDuration = 2 * DefaultTimeScale();
  const uint64_t kTwoSecondSegmentRepeat = 4;

  AddSegments(first_s_element_end_time,
              kTwoSecondDuration,
              kSize,
              kTwoSecondSegmentRepeat);

  std::string expected_s_element = base::StringPrintf(
      kSElementTemplateWithoutR,
      kInitialStartTime + kDuration1,  // Expect one segment removed.
      kDuration1);

  expected_s_element += base::StringPrintf(kSElementTemplate,
                                           first_s_element_end_time,
                                           kTwoSecondDuration,
                                           kTwoSecondSegmentRepeat);
  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element, kTimeShiftBufferDepth, 2));
}

// Gap between S elements but both should be included.
TEST_F(TimeShiftBufferDepthTest, NormalGap) {
  const int kTimeShiftBufferDepth = 10;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kDuration = DefaultTimeScale();
  const uint64_t kSize = 20000;
  const uint64_t kRepeat = 6;
  // CHECK here so that the when next S element is added with 1 segment, this S
  // element doesn't go away.
  CHECK_LT(kRepeat - 1u, static_cast<uint64_t>(kTimeShiftBufferDepth));
  CHECK_EQ(kDuration, DefaultTimeScale());

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration * (kRepeat + 1);

  const uint64_t gap_s_element_start_time = first_s_element_end_time + 1;
  AddSegments(gap_s_element_start_time, kDuration, kSize, /* no repeat */ 0);

  std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kInitialStartTime, kDuration, kRepeat);
  expected_s_element += base::StringPrintf(
      kSElementTemplateWithoutR, gap_s_element_start_time, kDuration);

  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element, kTimeShiftBufferDepth, kDefaultStartNumber));
}

// Case where there is a huge gap so the first S element is removed.
TEST_F(TimeShiftBufferDepthTest, HugeGap) {
  const int kTimeShiftBufferDepth = 10;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kDuration = DefaultTimeScale();
  const uint64_t kSize = 20000;
  const uint64_t kRepeat = 6;
  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration * (kRepeat + 1);

  // Big enough gap so first S element should not be there.
  const uint64_t gap_s_element_start_time =
      first_s_element_end_time +
      (kTimeShiftBufferDepth + 1) * DefaultTimeScale();
  const uint64_t kSecondSElementRepeat = 9;
  static_assert(
      kSecondSElementRepeat < static_cast<uint64_t>(kTimeShiftBufferDepth),
      "second_s_element_repeat_must_be_less_than_time_shift_buffer_depth");
  AddSegments(gap_s_element_start_time, kDuration, kSize, kSecondSElementRepeat);

  std::string expected_s_element = base::StringPrintf(kSElementTemplate,
                                                      gap_s_element_start_time,
                                                      kDuration,
                                                      kSecondSElementRepeat);
  const int kExpectedRemovedSegments = kRepeat + 1;
  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element,
      kTimeShiftBufferDepth,
      kDefaultStartNumber + kExpectedRemovedSegments));
}

// Check if startNumber is working correctly.
TEST_F(TimeShiftBufferDepthTest, ManySegments) {
  const int kTimeShiftBufferDepth = 1;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kDuration = DefaultTimeScale();
  const uint64_t kSize = 20000;
  const uint64_t kRepeat = 10000;
  const uint64_t kTotalNumSegments = kRepeat + 1;
  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  const int kExpectedSegmentsLeft = kTimeShiftBufferDepth + 1;
  const int kExpectedSegmentsRepeat = kExpectedSegmentsLeft - 1;
  const int kExpectedRemovedSegments =
      kTotalNumSegments - kExpectedSegmentsLeft;

  std::string expected_s_element =
      base::StringPrintf(kSElementTemplate,
                         kExpectedRemovedSegments * kDuration,
                         kDuration,
                         static_cast<uint64_t>(kExpectedSegmentsRepeat));

  ASSERT_NO_FATAL_FAILURE(CheckTimeShiftBufferDepthResult(
      expected_s_element,
      kTimeShiftBufferDepth,
      kDefaultStartNumber + kExpectedRemovedSegments));
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
