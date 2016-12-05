// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/hls/base/hls_notifier.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/event/hls_notify_muxer_listener.h"
#include "packager/media/event/muxer_listener_test_helper.h"

namespace shaka {
namespace media {

using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

namespace {

class MockHlsNotifier : public hls::HlsNotifier {
 public:
  MockHlsNotifier()
      : HlsNotifier(hls::HlsNotifier::HlsProfile::kOnDemandProfile) {}

  MOCK_METHOD0(Init, bool());
  MOCK_METHOD5(NotifyNewStream,
               bool(const MediaInfo& media_info,
                    const std::string& playlist_name,
                    const std::string& name,
                    const std::string& group_id,
                    uint32_t* stream_id));
  MOCK_METHOD5(NotifyNewSegment,
               bool(uint32_t stream_id,
                    const std::string& segment_name,
                    uint64_t start_time,
                    uint64_t duration,
                    uint64_t size));
  MOCK_METHOD5(
      NotifyEncryptionUpdate,
      bool(uint32_t stream_id,
           const std::vector<uint8_t>& key_id,
           const std::vector<uint8_t>& system_id,
           const std::vector<uint8_t>& iv,
           const std::vector<uint8_t>& protection_system_specific_data));
  MOCK_METHOD0(Flush, bool());
};

// Doesn't really matter what the values are as long as it is a system ID (16
// bytes).
const uint8_t kAnySystemId[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
};

const uint8_t kAnyData[] = {
  0xFF, 0x78, 0xAA, 0x6B,
};

// This value doesn't really affect the test, it's not used by the
// implementation.
const bool kInitialEncryptionInfo = true;

const char kDefaultPlaylistName[] = "default_playlist.m3u8";
const char kDefaultName[] = "DEFAULTNAME";
const char kDefaultGroupId[] = "DEFAULTGROUPID";

}  // namespace

class HlsNotifyMuxerListenerTest : public ::testing::Test {
 protected:
  HlsNotifyMuxerListenerTest()
      : listener_(kDefaultPlaylistName,
                  kDefaultName,
                  kDefaultGroupId,
                  &mock_notifier_) {}

  MockHlsNotifier mock_notifier_;
  HlsNotifyMuxerListener listener_;
};

// Verify that NotifyEncryptionUpdate() is not called before OnMediaStart() is
// called.
TEST_F(HlsNotifyMuxerListenerTest, OnEncryptionInfoReadyBeforeMediaStart) {
  ProtectionSystemSpecificInfo info;
  std::vector<uint8_t> system_id(kAnySystemId,
                                 kAnySystemId + arraysize(kAnySystemId));
  info.set_system_id(system_id.data(), system_id.size());
  std::vector<uint8_t> pssh_data(kAnyData, kAnyData + arraysize(kAnyData));
  info.set_pssh_data(pssh_data);

  std::vector<uint8_t> key_id(16, 0x05);
  std::vector<ProtectionSystemSpecificInfo> key_system_infos;
  key_system_infos.push_back(info);

  std::vector<uint8_t> iv(16, 0x54);

  EXPECT_CALL(mock_notifier_, NotifyEncryptionUpdate(_, _, _, _, _)).Times(0);
  listener_.OnEncryptionInfoReady(kInitialEncryptionInfo, FOURCC_cbcs, key_id,
                                  iv, key_system_infos);
}

TEST_F(HlsNotifyMuxerListenerTest, OnMediaStart) {
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);

  EXPECT_CALL(mock_notifier_,
              NotifyNewStream(_, StrEq(kDefaultPlaylistName),
                              StrEq("DEFAULTNAME"), StrEq("DEFAULTGROUPID"), _))
      .WillOnce(Return(true));

  MuxerOptions muxer_options;
  listener_.OnMediaStart(muxer_options, *video_stream_info, 90000,
                         MuxerListener::kContainerMpeg2ts);
}

// OnEncryptionStart() should call MuxerListener::NotifyEncryptionUpdate() after
// OnEncryptionInfoReady() and OnMediaStart().
TEST_F(HlsNotifyMuxerListenerTest, OnEncryptionStart) {
  ProtectionSystemSpecificInfo info;
  std::vector<uint8_t> system_id(kAnySystemId,
                                 kAnySystemId + arraysize(kAnySystemId));
  info.set_system_id(system_id.data(), system_id.size());
  std::vector<uint8_t> pssh_data(kAnyData, kAnyData + arraysize(kAnyData));
  info.set_pssh_data(pssh_data);

  std::vector<uint8_t> key_id(16, 0x05);
  std::vector<ProtectionSystemSpecificInfo> key_system_infos;
  key_system_infos.push_back(info);

  std::vector<uint8_t> iv(16, 0x54);

  EXPECT_CALL(mock_notifier_, NotifyEncryptionUpdate(_, _, _, _, _)).Times(0);
  listener_.OnEncryptionInfoReady(kInitialEncryptionInfo, FOURCC_cbcs, key_id,
                                  iv, key_system_infos);
  ::testing::Mock::VerifyAndClearExpectations(&mock_notifier_);

  ON_CALL(mock_notifier_, NotifyNewStream(_, _, _, _, _))
      .WillByDefault(Return(true));
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);
  MuxerOptions muxer_options;

  EXPECT_CALL(mock_notifier_, NotifyEncryptionUpdate(_, _, _, _, _)).Times(0);
  listener_.OnMediaStart(muxer_options, *video_stream_info, 90000,
                         MuxerListener::kContainerMpeg2ts);
  ::testing::Mock::VerifyAndClearExpectations(&mock_notifier_);

  EXPECT_CALL(mock_notifier_,
              NotifyEncryptionUpdate(_, key_id, system_id, iv, pssh_data))
      .WillOnce(Return(true));
  listener_.OnEncryptionStart();
}

// If OnEncryptionStart() is called before media start,
// HlsNotiifer::NotifyEncryptionUpdate() should be called by the end of
// OnMediaStart().
TEST_F(HlsNotifyMuxerListenerTest, OnEncryptionStartBeforeMediaStart) {
  ProtectionSystemSpecificInfo info;
  std::vector<uint8_t> system_id(kAnySystemId,
                                 kAnySystemId + arraysize(kAnySystemId));
  info.set_system_id(system_id.data(), system_id.size());
  std::vector<uint8_t> pssh_data(kAnyData, kAnyData + arraysize(kAnyData));
  info.set_pssh_data(pssh_data);

  std::vector<uint8_t> key_id(16, 0x05);
  std::vector<ProtectionSystemSpecificInfo> key_system_infos;
  key_system_infos.push_back(info);

  std::vector<uint8_t> iv(16, 0x54);

  EXPECT_CALL(mock_notifier_, NotifyEncryptionUpdate(_, _, _, _, _)).Times(0);
  listener_.OnEncryptionInfoReady(kInitialEncryptionInfo, FOURCC_cbcs, key_id,
                                  iv, key_system_infos);
  ::testing::Mock::VerifyAndClearExpectations(&mock_notifier_);

  ON_CALL(mock_notifier_, NotifyNewStream(_, _, _, _, _))
      .WillByDefault(Return(true));
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);
  MuxerOptions muxer_options;

  // It doesn't really matter when this is called, could be called right away in
  // OnEncryptionStart() if that is possible. Just matters that it is called by
  // the time OnMediaStart() returns.
  EXPECT_CALL(mock_notifier_,
              NotifyEncryptionUpdate(_, key_id, system_id, iv, pssh_data))
      .WillOnce(Return(true));
  listener_.OnEncryptionStart();
  listener_.OnMediaStart(muxer_options, *video_stream_info, 90000,
                         MuxerListener::kContainerMpeg2ts);
}

// NotifyEncryptionUpdate() should not be called if NotifyNewStream() fails in
// OnMediaStart().
TEST_F(HlsNotifyMuxerListenerTest, NoEncryptionUpdateIfNotifyNewStreamFails) {
  ProtectionSystemSpecificInfo info;
  std::vector<uint8_t> system_id(kAnySystemId,
                                 kAnySystemId + arraysize(kAnySystemId));
  info.set_system_id(system_id.data(), system_id.size());
  std::vector<uint8_t> pssh_data(kAnyData, kAnyData + arraysize(kAnyData));
  info.set_pssh_data(pssh_data);

  std::vector<uint8_t> key_id(16, 0x05);
  std::vector<ProtectionSystemSpecificInfo> key_system_infos;
  key_system_infos.push_back(info);

  std::vector<uint8_t> iv(16, 0x54);

  EXPECT_CALL(mock_notifier_, NotifyEncryptionUpdate(_, _, _, _, _)).Times(0);
  listener_.OnEncryptionInfoReady(kInitialEncryptionInfo, FOURCC_cbcs, key_id,
                                  iv, key_system_infos);
  ::testing::Mock::VerifyAndClearExpectations(&mock_notifier_);

  EXPECT_CALL(mock_notifier_, NotifyNewStream(_, _, _, _, _))
      .WillOnce(Return(false));
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);
  MuxerOptions muxer_options;

  EXPECT_CALL(mock_notifier_, NotifyEncryptionUpdate(_, _, _, _, _)).Times(0);
  listener_.OnMediaStart(muxer_options, *video_stream_info, 90000,
                         MuxerListener::kContainerMpeg2ts);
}

// Verify that after OnMediaStart(), OnEncryptionInfoReady() calls
// NotifyEncryptionUpdate().
TEST_F(HlsNotifyMuxerListenerTest, OnEncryptionInfoReady) {
  ON_CALL(mock_notifier_, NotifyNewStream(_, _, _, _, _))
      .WillByDefault(Return(true));
  VideoStreamInfoParameters video_params = GetDefaultVideoStreamInfoParams();
  scoped_refptr<StreamInfo> video_stream_info =
      CreateVideoStreamInfo(video_params);
  MuxerOptions muxer_options;
  listener_.OnMediaStart(muxer_options, *video_stream_info, 90000,
                         MuxerListener::kContainerMpeg2ts);

  ProtectionSystemSpecificInfo info;
  std::vector<uint8_t> system_id(kAnySystemId,
                                 kAnySystemId + arraysize(kAnySystemId));
  info.set_system_id(system_id.data(), system_id.size());
  std::vector<uint8_t> pssh_data(kAnyData, kAnyData + arraysize(kAnyData));
  info.set_pssh_data(pssh_data);

  std::vector<uint8_t> key_id(16, 0x05);
  std::vector<ProtectionSystemSpecificInfo> key_system_infos;
  key_system_infos.push_back(info);

  std::vector<uint8_t> iv(16, 0x54);

  EXPECT_CALL(mock_notifier_,
              NotifyEncryptionUpdate(_, key_id, system_id, iv, pssh_data))
      .WillOnce(Return(true));
  listener_.OnEncryptionInfoReady(kInitialEncryptionInfo, FOURCC_cbcs, key_id,
                                  iv, key_system_infos);
}

// Make sure it doesn't crash.
TEST_F(HlsNotifyMuxerListenerTest, OnSampleDurationReady) {
  listener_.OnSampleDurationReady(2340);
}

// Make sure it doesn't crash.
TEST_F(HlsNotifyMuxerListenerTest, OnMediaEnd) {
  // None of these values matter, they are not used.
  listener_.OnMediaEnd(false, 0, 0, false, 0, 0, 0, 0);
}

TEST_F(HlsNotifyMuxerListenerTest, OnNewSegment) {
  const uint64_t kStartTime = 19283;
  const uint64_t kDuration = 98028;
  const uint64_t kFileSize = 756739;
  EXPECT_CALL(mock_notifier_,
              NotifyNewSegment(_, StrEq("new_segment_name10.ts"), kStartTime,
                               kDuration, kFileSize));
  listener_.OnNewSegment("new_segment_name10.ts", kStartTime, kDuration,
                         kFileSize);
}

}  // namespace media
}  // namespace shaka
