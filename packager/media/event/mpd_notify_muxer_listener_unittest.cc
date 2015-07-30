// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/mpd_notify_muxer_listener.h"

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

#include "packager/base/stl_util.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener_test_helper.h"
#include "packager/mpd/base/content_protection_element.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_notifier.h"

using ::testing::_;

namespace edash_packager {

namespace {

// TODO(rkuroiwa): This is copied from mpd_builder_test_helper.cc. Make a
// common target that only has mpd_builder_test_helper and its dependencies
// so the two test targets can share this.
MediaInfo ConvertToMediaInfo(const std::string& media_info_string) {
  MediaInfo media_info;
  CHECK(::google::protobuf::TextFormat::ParseFromString(media_info_string,
                                                        &media_info));
  return media_info;
}

class MockMpdNotifier : public MpdNotifier {
 public:
  MockMpdNotifier(DashProfile profile) : MpdNotifier(profile) {}
  virtual ~MockMpdNotifier() OVERRIDE {}

  MOCK_METHOD0(Init, bool());
  MOCK_METHOD2(NotifyNewContainer,
               bool(const MediaInfo& media_info, uint32_t* container_id));
  MOCK_METHOD2(NotifySampleDuration,
               bool(uint32_t container_id, uint32_t sample_duration));
  MOCK_METHOD4(NotifyNewSegment,
               bool(uint32_t container_id,
                    uint64_t start_time,
                    uint64_t duration,
                    uint64_t size));
  MOCK_METHOD2(
      AddContentProtectionElement,
      bool(uint32_t container_id,
           const ContentProtectionElement& content_protection_element));
};

}  // namespace

namespace media {

class MpdNotifyMuxerListenerTest : public ::testing::Test {
 public:

  // Set up objects for VOD profile.
  void SetupForVod() {
    notifier_.reset(new MockMpdNotifier(kOnDemandProfile));
    listener_.reset(new MpdNotifyMuxerListener(notifier_.get()));
  }

  void FireOnMediaEndWithParams(const OnMediaEndParameters& params) {
    // On success, this writes the result to |temp_file_path_|.
    listener_->OnMediaEnd(params.has_init_range,
                          params.init_range_start,
                          params.init_range_end,
                          params.has_index_range,
                          params.index_range_start,
                          params.index_range_end,
                          params.duration_seconds,
                          params.file_size);
  }

  scoped_ptr<MpdNotifyMuxerListener> listener_;
  scoped_ptr<MockMpdNotifier> notifier_;
};

MATCHER_P(ExpectMediaInfoEq, expected_text_format, "") {
  const MediaInfo expected = ConvertToMediaInfo(expected_text_format);
  return MediaInfoEqual(expected, arg);
}

TEST_F(MpdNotifyMuxerListenerTest, VodClearContent) {
  SetupForVod();
  MuxerOptions muxer_options;
  SetDefaultMuxerOptionsValues(&muxer_options);
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);

  EXPECT_CALL(*notifier_, NotifyNewContainer(_, _)).Times(0);
  listener_->OnMediaStart(muxer_options, *video_stream_info,
                          kDefaultReferenceTimeScale,
                          MuxerListener::kContainerMp4);
  ::testing::Mock::VerifyAndClearExpectations(notifier_.get());

  EXPECT_CALL(*notifier_, NotifyNewContainer(
                              ExpectMediaInfoEq(kExpectedDefaultMediaInfo), _));
  FireOnMediaEndWithParams(GetDefaultOnMediaEndParams());
}

// default_key_id and pssh are converted to string because when std::equal
// compares a negative char and uint8_t > 127, it considers them not equal.
MATCHER_P4(ProtectedContentEq, uuid, name, default_key_id, pssh, "") {
  const MediaInfo& actual_media_info = arg;
  EXPECT_TRUE(actual_media_info.has_protected_content());
  EXPECT_EQ(
      1,
      actual_media_info.protected_content().content_protection_entry().size());
  const std::string& actual_default_kid =
      actual_media_info.protected_content().default_key_id();
  const std::string expected_default_kid_string(default_key_id.begin(),
                                                default_key_id.end());
  if (actual_default_kid != expected_default_kid_string) {
    return false;
  }

  const MediaInfo_ProtectedContent_ContentProtectionEntry& entry =
      actual_media_info.protected_content().content_protection_entry(0);

  const std::string expcted_pssh_string(pssh.begin(), pssh.end());
  return entry.uuid() == uuid && entry.name_version() == name &&
         entry.pssh().size() == pssh.size() &&
         entry.pssh() == expcted_pssh_string;
}

TEST_F(MpdNotifyMuxerListenerTest, VodEncryptedContent) {
  SetupForVod();
  MuxerOptions muxer_options;
  SetDefaultMuxerOptionsValues(&muxer_options);
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);

  // Can be anystring, we just want to check that it is preserved in the
  // protobuf.
  const char kTestUUID[] = "somebogusuuid";
  const char kDrmName[] = "drmname";
  const char kDefaultKeyId[] = "defaultkeyid";
  const char kPssh[] = "pssh";
  const std::vector<uint8_t> default_key_id(
      kDefaultKeyId, kDefaultKeyId + arraysize(kDefaultKeyId) - 1);
  const std::vector<uint8_t> pssh(kPssh, kPssh + arraysize(kPssh) - 1);

  const std::string kExpectedMediaInfo =
      std::string(kExpectedDefaultMediaInfo) +
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'somebogusuuid'\n"
      "    name_version: 'drmname'\n"
      "    pssh: 'pssh'\n"
      "  }\n"
      "  default_key_id: 'defaultkeyid'\n"
      "}\n";

  EXPECT_CALL(*notifier_, NotifyNewContainer(_, _)).Times(0);
  listener_->OnEncryptionInfoReady(kTestUUID, kDrmName, default_key_id, pssh);

  listener_->OnMediaStart(muxer_options, *video_stream_info,
                          kDefaultReferenceTimeScale,
                          MuxerListener::kContainerMp4);
  ::testing::Mock::VerifyAndClearExpectations(notifier_.get());

  EXPECT_CALL(*notifier_,
              NotifyNewContainer(ExpectMediaInfoEq(kExpectedMediaInfo), _));
  FireOnMediaEndWithParams(GetDefaultOnMediaEndParams());
}

// Verify that calling OnSampleDurationReady() sets the frame duration in the
// media info, and the media info gets passed to NotifyNewContainer() with
// frame_duration == sample_duration.
TEST_F(MpdNotifyMuxerListenerTest, VodOnSampleDurationReady) {
  SetupForVod();
  MuxerOptions muxer_options;
  SetDefaultMuxerOptionsValues(&muxer_options);
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);
  const uint32_t kSampleDuration = 1234u;
  const char kExpectedMediaInfo[] =
      "bandwidth: 7620\n"
      "video_info {\n"
      "  frame_duration: 1234\n"  // Should match the constant above.
      "  codec: 'avc1.010101'\n"
      "  width: 720\n"
      "  height: 480\n"
      "  time_scale: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "init_range {\n"
      "  begin: 0\n"
      "  end: 120\n"
      "}\n"
      "index_range {\n"
      "  begin: 121\n"
      "  end: 221\n"
      "}\n"
      "reference_time_scale: 1111\n"
      "container_type: 1\n"
      "media_file_name: 'test_output_file_name.mp4'\n"
      "media_duration_seconds: 10.5\n";
  const uint32_t kReferenceTimeScale = 1111u;  // Should match the protobuf.

  EXPECT_CALL(*notifier_, NotifyNewContainer(_, _)).Times(0);
  listener_->OnMediaStart(muxer_options, *video_stream_info,
                          kReferenceTimeScale, MuxerListener::kContainerMp4);
  listener_->OnSampleDurationReady(kSampleDuration);
  ::testing::Mock::VerifyAndClearExpectations(notifier_.get());

  EXPECT_CALL(*notifier_,
              NotifyNewContainer(ExpectMediaInfoEq(kExpectedMediaInfo), _));
  FireOnMediaEndWithParams(GetDefaultOnMediaEndParams());
}

// TODO(rkuroiwa): Add tests for live.

}  // namespace media
}  // namespace edash_packager
