// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/representation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

  std::unique_ptr<RepresentationStateChangeListener> NoListener() {
    return std::unique_ptr<RepresentationStateChangeListener>();
  }

 private:
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

}  // namespace shaka
