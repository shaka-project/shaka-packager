// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/representation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <inttypes.h>

#include "packager/base/strings/stringprintf.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/mpd/test/xml_compare.h"

using ::testing::Not;

namespace shaka {
namespace {

const uint32_t kAnyRepresentationId = 1;

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

class RepresentationTest : public ::testing::Test {
 public:
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

  std::unique_ptr<Representation> CopyRepresentation(
      const Representation& representation,
      uint64_t presentation_time_offset,
      std::unique_ptr<RepresentationStateChangeListener>
          state_change_listener) {
    return std::unique_ptr<Representation>(
        new Representation(representation, presentation_time_offset,
                           std::move(state_change_listener)));
  }

  std::unique_ptr<RepresentationStateChangeListener> NoListener() {
    return std::unique_ptr<RepresentationStateChangeListener>();
  }

 protected:
  MpdOptions mpd_options_;
};

// Verify that Representation::Init() works with all "required" fields of
// MedieInfo proto.
TEST_F(RepresentationTest, ValidMediaInfo) {
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
TEST_F(RepresentationTest, VideoAudioTextInfoNotSet) {
  const char kTestMediaInfo[] = "container_type: 1";

  auto representation = CreateRepresentation(
      ConvertToMediaInfo(kTestMediaInfo), kAnyRepresentationId, NoListener());
  EXPECT_FALSE(representation->Init());
}

// Verify that if more than one of VideoInfo, AudioInfo, or TextInfo is set,
// then Init() fails.
TEST_F(RepresentationTest, VideoAndAudioInfoSet) {
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
TEST_F(RepresentationTest, InvalidMediaInfo) {
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
TEST_F(RepresentationTest, CheckVideoInfoReflectedInXml) {
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

TEST_F(RepresentationTest, CheckVideoInfoVp8CodecInMp4) {
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
TEST_F(RepresentationTest, CheckVideoInfoVp8CodecInWebm) {
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
TEST_F(RepresentationTest, CheckVideoInfoVp9CodecInWebm) {
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
TEST_F(RepresentationTest,
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
TEST_F(RepresentationTest,
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

TEST_F(RepresentationTest, TtmlXmlMimeType) {
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

TEST_F(RepresentationTest, TtmlMp4MimeType) {
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

TEST_F(RepresentationTest, WebVttMimeType) {
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

// Verify that Suppress*() methods work.
TEST_F(RepresentationTest, SuppressRepresentationAttributes) {
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

TEST_F(RepresentationTest, CheckRepresentationId) {
  const MediaInfo video_media_info = GetTestMediaInfo(kFileNameVideoMediaInfo1);
  const uint32_t kRepresentationId = 1;

  auto representation =
      CreateRepresentation(video_media_info, kRepresentationId, NoListener());
  EXPECT_TRUE(representation->Init());
  EXPECT_THAT(representation->GetXml().get(),
              AttributeEqual("id", std::to_string(kRepresentationId)));
}

namespace {

// Any number for {AdaptationSet,Representation} ID. Required to create
// either objects. Not checked in test.
const char kSElementTemplate[] =
    "<S t=\"%" PRIu64 "\" d=\"%" PRIu64 "\" r=\"%" PRIu64 "\"/>\n";
const char kSElementTemplateWithoutR[] =
    "<S t=\"%" PRIu64 "\" d=\"%" PRIu64 "\"/>\n";
const int kDefaultStartNumber = 1;
const uint32_t kDefaultTimeScale = 1000u;

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

  return base::StringPrintf(kMediaInfo, kDefaultTimeScale);
}

}  // namespace

class SegmentTemplateTest : public RepresentationTest {
 public:
  SegmentTemplateTest()
      : bandwidth_estimator_(BandwidthEstimator::kUseAllBlocks) {}
  ~SegmentTemplateTest() override {}

  void SetUp() override {
    mpd_options_.mpd_type = MpdType::kDynamic;
    representation_ =
        CreateRepresentation(ConvertToMediaInfo(GetDefaultMediaInfo()),
                             kAnyRepresentationId, NoListener());
    ASSERT_TRUE(representation_->Init());
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
          size, static_cast<double>(duration) / kDefaultTimeScale);
    }
  }

 protected:
  std::string ExpectedXml() {
    const char kOutputTemplate[] =
        "<Representation id=\"1\" bandwidth=\"%" PRIu64
        "\" "
        " codecs=\"avc1.010101\" mimeType=\"video/mp4\" sar=\"1:1\" "
        " width=\"720\" height=\"480\" frameRate=\"10/5\">\n"
        "  <SegmentTemplate timescale=\"1000\" "
        "   initialization=\"init.mp4\" media=\"$Time$.mp4\">\n"
        "    <SegmentTimeline>\n"
        "      %s\n"
        "    </SegmentTimeline>\n"
        "  </SegmentTemplate>\n"
        "</Representation>\n";
    return base::StringPrintf(kOutputTemplate, bandwidth_estimator_.Estimate(),
                              expected_s_elements_.c_str());
  }

  std::unique_ptr<Representation> representation_;
  std::list<SegmentInfo> segment_infos_for_expected_out_;
  std::string expected_s_elements_;
  BandwidthEstimator bandwidth_estimator_;
};

// Estimate the bandwidth given the info from AddNewSegment().
TEST_F(SegmentTemplateTest, OneSegmentNormal) {
  const uint64_t kStartTime = 0;
  const uint64_t kDuration = 10;
  const uint64_t kSize = 128;
  AddSegments(kStartTime, kDuration, kSize, 0);

  const char kExpectedXml[] =
      "<Representation id=\"1\" bandwidth=\"102400\" "
      " codecs=\"avc1.010101\" mimeType=\"video/mp4\" sar=\"1:1\" "
      " width=\"720\" height=\"480\" frameRate=\"10/5\">\n"
      "  <SegmentTemplate timescale=\"1000\" "
      "   initialization=\"init.mp4\" media=\"$Time$.mp4\">\n"
      "    <SegmentTimeline>\n"
      "      <S t=\"0\" d=\"10\"/>\n"
      "    </SegmentTimeline>\n"
      "  </SegmentTemplate>\n"
      "</Representation>\n";
  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(kExpectedXml));
}

TEST_F(SegmentTemplateTest, RepresentationClone) {
  MediaInfo media_info = ConvertToMediaInfo(GetDefaultMediaInfo());
  media_info.set_segment_template("$Number$.mp4");
  representation_ =
      CreateRepresentation(media_info, kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation_->Init());

  const uint64_t kStartTime = 0;
  const uint64_t kDuration = 10;
  const uint64_t kSize = 128;
  AddSegments(kStartTime, kDuration, kSize, 0);

  const uint64_t kPresentationTimeOffset = 100;
  auto cloned_representation = CopyRepresentation(
      *representation_, kPresentationTimeOffset, NoListener());
  const char kExpectedXml[] =
      "<Representation id=\"1\" bandwidth=\"0\" "
      " codecs=\"avc1.010101\" mimeType=\"video/mp4\" sar=\"1:1\" "
      " width=\"720\" height=\"480\" frameRate=\"10/5\">\n"
      "  <SegmentTemplate presentationTimeOffset=\"100\" timescale=\"1000\" "
      "   initialization=\"init.mp4\" media=\"$Number$.mp4\" "
      "   startNumber=\"2\">\n"
      "    <SegmentTimeline/>\n"
      "  </SegmentTemplate>\n"
      "</Representation>\n";
  EXPECT_THAT(cloned_representation->GetXml().get(),
              XmlNodeEqual(kExpectedXml));
}

TEST_F(SegmentTemplateTest, GetEarliestTimestamp) {
  double earliest_timestamp;
  // No segments.
  EXPECT_FALSE(representation_->GetEarliestTimestamp(&earliest_timestamp));

  const uint64_t kStartTime = 88;
  const uint64_t kDuration = 10;
  const uint64_t kSize = 128;
  AddSegments(kStartTime, kDuration, kSize, 0);
  AddSegments(kStartTime + kDuration, kDuration, kSize, 0);
  ASSERT_TRUE(representation_->GetEarliestTimestamp(&earliest_timestamp));
  EXPECT_EQ(static_cast<double>(kStartTime) / kDefaultTimeScale,
            earliest_timestamp);
}

TEST_F(SegmentTemplateTest, GetDuration) {
  const float kMediaDurationSeconds = 88.8f;
  MediaInfo media_info = ConvertToMediaInfo(GetDefaultMediaInfo());
  media_info.set_media_duration_seconds(kMediaDurationSeconds);
  representation_ =
      CreateRepresentation(media_info, kAnyRepresentationId, NoListener());
  ASSERT_TRUE(representation_->Init());

  EXPECT_EQ(kMediaDurationSeconds, representation_->GetDurationSeconds());
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

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
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

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
}

// Segments not starting from 0.
// Start time is 10. Make sure r gets set correctly.
TEST_F(SegmentTemplateTest, NonZeroStartTime) {
  const uint64_t kStartTime = 10;
  const uint64_t kDuration = 22000;
  const uint64_t kSize = 123456;
  const uint64_t kRepeat = 1;
  AddSegments(kStartTime, kDuration, kSize, kRepeat);

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
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

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
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

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
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

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
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

  EXPECT_THAT(representation_->GetXml().get(), XmlNodeEqual(ExpectedXml()));
}

class TimeShiftBufferDepthTest : public SegmentTemplateTest {
 public:
  void SetUp() override {
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
        base::StringPrintf(kMediaInfo, kDefaultTimeScale);
    mpd_options_.mpd_type = MpdType::kDynamic;
    representation_ =
        CreateRepresentation(ConvertToMediaInfo(number_template_media_info),
                             kAnyRepresentationId, NoListener());
    ASSERT_TRUE(representation_->Init());
  }

  std::string ExpectedXml(const std::string& expected_s_element,
                          int expected_start_number) {
    const char kOutputTemplate[] =
        "<Representation id=\"1\" bandwidth=\"%" PRIu64
        "\" "
        " codecs=\"avc1.010101\" mimeType=\"video/mp4\" sar=\"1:1\" "
        " width=\"720\" height=\"480\" frameRate=\"10/2\">\n"
        "  <SegmentTemplate timescale=\"1000\" "
        "   initialization=\"init.mp4\" media=\"$Number$.mp4\" "
        "   startNumber=\"%d\">\n"
        "    <SegmentTimeline>\n"
        "      %s\n"
        "    </SegmentTimeline>\n"
        "  </SegmentTemplate>\n"
        "</Representation>\n";

    return base::StringPrintf(kOutputTemplate, bandwidth_estimator_.Estimate(),
                              expected_start_number,
                              expected_s_element.c_str());
  }

  MpdOptions* mutable_mpd_options() { return &mpd_options_; }
};

// All segments have the same duration and size.
TEST_F(TimeShiftBufferDepthTest, Normal) {
  const int kTimeShiftBufferDepth = 10;  // 10 sec.
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  // Trick to make every segment 1 second long.
  const uint64_t kDuration = kDefaultTimeScale;
  const uint64_t kSize = 10000;
  const uint64_t kRepeat = 1234;
  const uint64_t kLength = kRepeat;

  CHECK_EQ(kDuration / kDefaultTimeScale * kRepeat, kLength);

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  // There should only be the last 11 segments because timeshift is 10 sec and
  // each segment is 1 sec and the latest segments start time is "current
  // time" i.e., the latest segment does not count as part of timeshift buffer
  // depth.
  // Also note that S@r + 1 is the actual number of segments.
  const int kExpectedRepeatsLeft = kTimeShiftBufferDepth;
  const std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kDuration * (kRepeat - kExpectedRepeatsLeft),
      kDuration, static_cast<uint64_t>(kExpectedRepeatsLeft));

  const int kExpectedStartNumber = kRepeat - kExpectedRepeatsLeft + 1;
  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(expected_s_element, kExpectedStartNumber)));
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
  const uint64_t kDuration = kDefaultTimeScale * (kTimeShiftBufferDepth + 1);
  const uint64_t kSize = 10000;
  const uint64_t kRepeat = 1;

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  // The two segments should be both present.
  const std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kInitialStartTime, kDuration, kRepeat);

  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(expected_s_element, kDefaultStartNumber)));
}

// More generic version the normal test.
TEST_F(TimeShiftBufferDepthTest, Generic) {
  const int kTimeShiftBufferDepth = 30;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 123;
  const uint64_t kDuration = kDefaultTimeScale;
  const uint64_t kSize = 10000;
  const uint64_t kRepeat = 1000;

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);
  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration * (kRepeat + 1);

  // Now add 2 kTimeShiftBufferDepth long segments.
  const int kNumMoreSegments = 2;
  const int kMoreSegmentsRepeat = kNumMoreSegments - 1;
  const uint64_t kTimeShiftBufferDepthDuration =
      kDefaultTimeScale * kTimeShiftBufferDepth;
  AddSegments(first_s_element_end_time, kTimeShiftBufferDepthDuration, kSize,
              kMoreSegmentsRepeat);

  // Expect only the latest S element with 2 segments.
  const std::string expected_s_element =
      base::StringPrintf(kSElementTemplate, first_s_element_end_time,
                         kTimeShiftBufferDepthDuration,
                         static_cast<uint64_t>(kMoreSegmentsRepeat));

  const int kExpectedRemovedSegments = kRepeat + 1;
  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(
          expected_s_element, kDefaultStartNumber + kExpectedRemovedSegments)));
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

  const uint64_t kOneSecondDuration = kDefaultTimeScale;
  const uint64_t kOneSecondSegmentRepeat = 99;
  AddSegments(kInitialStartTime, kOneSecondDuration, kSize,
              kOneSecondSegmentRepeat);
  const uint64_t first_s_element_end_time =
      kInitialStartTime + kOneSecondDuration * (kOneSecondSegmentRepeat + 1);

  const uint64_t kTwoSecondDuration = 2 * kDefaultTimeScale;
  const uint64_t kTwoSecondSegmentRepeat = 20;
  AddSegments(first_s_element_end_time, kTwoSecondDuration, kSize,
              kTwoSecondSegmentRepeat);

  const uint64_t kExpectedRemovedSegments =
      (kOneSecondSegmentRepeat + 1 + kTwoSecondSegmentRepeat * 2) -
      kTimeShiftBufferDepth;

  std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kOneSecondDuration * kExpectedRemovedSegments,
      kOneSecondDuration, kOneSecondSegmentRepeat - kExpectedRemovedSegments);
  expected_s_element +=
      base::StringPrintf(kSElementTemplate, first_s_element_end_time,
                         kTwoSecondDuration, kTwoSecondSegmentRepeat);

  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(
          expected_s_element, kDefaultStartNumber + kExpectedRemovedSegments)));
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
  const uint64_t kDuration1 = static_cast<uint64_t>(kDefaultTimeScale * 1.5);
  const uint64_t kSize = 20000;
  const uint64_t kRepeat1 = 1;

  AddSegments(kInitialStartTime, kDuration1, kSize, kRepeat1);

  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration1 * (kRepeat1 + 1);

  const uint64_t kTwoSecondDuration = 2 * kDefaultTimeScale;
  const uint64_t kTwoSecondSegmentRepeat = 4;

  AddSegments(first_s_element_end_time, kTwoSecondDuration, kSize,
              kTwoSecondSegmentRepeat);

  std::string expected_s_element = base::StringPrintf(
      kSElementTemplateWithoutR,
      kInitialStartTime + kDuration1,  // Expect one segment removed.
      kDuration1);

  expected_s_element +=
      base::StringPrintf(kSElementTemplate, first_s_element_end_time,
                         kTwoSecondDuration, kTwoSecondSegmentRepeat);
  EXPECT_THAT(representation_->GetXml().get(),
              XmlNodeEqual(ExpectedXml(expected_s_element, 2)));
}

// Gap between S elements but both should be included.
TEST_F(TimeShiftBufferDepthTest, NormalGap) {
  const int kTimeShiftBufferDepth = 10;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kDuration = kDefaultTimeScale;
  const uint64_t kSize = 20000;
  const uint64_t kRepeat = 6;
  // CHECK here so that the when next S element is added with 1 segment, this S
  // element doesn't go away.
  CHECK_LT(kRepeat - 1u, static_cast<uint64_t>(kTimeShiftBufferDepth));
  CHECK_EQ(kDuration, kDefaultTimeScale);

  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration * (kRepeat + 1);

  const uint64_t gap_s_element_start_time = first_s_element_end_time + 1;
  AddSegments(gap_s_element_start_time, kDuration, kSize, /* no repeat */ 0);

  std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kInitialStartTime, kDuration, kRepeat);
  expected_s_element += base::StringPrintf(kSElementTemplateWithoutR,
                                           gap_s_element_start_time, kDuration);

  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(expected_s_element, kDefaultStartNumber)));
}

// Case where there is a huge gap so the first S element is removed.
TEST_F(TimeShiftBufferDepthTest, HugeGap) {
  const int kTimeShiftBufferDepth = 10;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kDuration = kDefaultTimeScale;
  const uint64_t kSize = 20000;
  const uint64_t kRepeat = 6;
  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  const uint64_t first_s_element_end_time =
      kInitialStartTime + kDuration * (kRepeat + 1);

  // Big enough gap so first S element should not be there.
  const uint64_t gap_s_element_start_time =
      first_s_element_end_time +
      (kTimeShiftBufferDepth + 1) * kDefaultTimeScale;
  const uint64_t kSecondSElementRepeat = 9;
  static_assert(
      kSecondSElementRepeat < static_cast<uint64_t>(kTimeShiftBufferDepth),
      "second_s_element_repeat_must_be_less_than_time_shift_buffer_depth");
  AddSegments(gap_s_element_start_time, kDuration, kSize,
              kSecondSElementRepeat);

  std::string expected_s_element =
      base::StringPrintf(kSElementTemplate, gap_s_element_start_time, kDuration,
                         kSecondSElementRepeat);
  const int kExpectedRemovedSegments = kRepeat + 1;
  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(
          expected_s_element, kDefaultStartNumber + kExpectedRemovedSegments)));
}

// Check if startNumber is working correctly.
TEST_F(TimeShiftBufferDepthTest, ManySegments) {
  const int kTimeShiftBufferDepth = 1;
  mutable_mpd_options()->mpd_params.time_shift_buffer_depth =
      kTimeShiftBufferDepth;

  const uint64_t kInitialStartTime = 0;
  const uint64_t kDuration = kDefaultTimeScale;
  const uint64_t kSize = 20000;
  const uint64_t kRepeat = 10000;
  const uint64_t kTotalNumSegments = kRepeat + 1;
  AddSegments(kInitialStartTime, kDuration, kSize, kRepeat);

  const int kExpectedSegmentsLeft = kTimeShiftBufferDepth + 1;
  const int kExpectedSegmentsRepeat = kExpectedSegmentsLeft - 1;
  const int kExpectedRemovedSegments =
      kTotalNumSegments - kExpectedSegmentsLeft;

  std::string expected_s_element = base::StringPrintf(
      kSElementTemplate, kExpectedRemovedSegments * kDuration, kDuration,
      static_cast<uint64_t>(kExpectedSegmentsRepeat));

  EXPECT_THAT(
      representation_->GetXml().get(),
      XmlNodeEqual(ExpectedXml(
          expected_s_element, kDefaultStartNumber + kExpectedRemovedSegments)));
}

}  // namespace shaka
