// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/base64.h"
#include "packager/hls/base/mock_media_playlist.h"
#include "packager/hls/base/simple_hls_notifier.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace edash_packager {
namespace hls {

using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

namespace {
const char kMasterPlaylistName[] = "master.m3u8";
const MediaPlaylist::MediaPlaylistType kVodPlaylist =
    MediaPlaylist::MediaPlaylistType::kVod;

class MockMasterPlaylist : public MasterPlaylist {
 public:
  MockMasterPlaylist() : MasterPlaylist(kMasterPlaylistName) {}

  MOCK_METHOD1(AddMediaPlaylist, void(MediaPlaylist* media_playlist));
  MOCK_METHOD2(WriteAllPlaylists,
               bool(const std::string& prefix, const std::string& output_dir));
  MOCK_METHOD2(WriteMasterPlaylist,
               bool(const std::string& prefix, const std::string& output_dir));
};

class MockMediaPlaylistFactory : public MediaPlaylistFactory {
 public:
  MOCK_METHOD4(CreateMock,
               MediaPlaylist*(MediaPlaylist::MediaPlaylistType type,
                              const std::string& file_name,
                              const std::string& name,
                              const std::string& group_id));

  scoped_ptr<MediaPlaylist> Create(MediaPlaylist::MediaPlaylistType type,
                                   const std::string& file_name,
                                   const std::string& name,
                                   const std::string& group_id) override {
    return scoped_ptr<MediaPlaylist>(
        CreateMock(type, file_name, name, group_id));
  }
};

const char kTestPrefix[] = "http://testprefix.com/";
const char kAnyOutputDir[] = "anything/";

}  // namespace

class SimpleHlsNotifierTest : public ::testing::Test {
 protected:
  SimpleHlsNotifierTest()
      : notifier_(HlsNotifier::HlsProfile::kOnDemandProfile,
                  kTestPrefix,
                  kAnyOutputDir,
                  kMasterPlaylistName) {}

  void InjectMediaPlaylistFactory(scoped_ptr<MediaPlaylistFactory> factory) {
    notifier_.media_playlist_factory_ = factory.Pass();
  }

  void InjectMasterPlaylist(scoped_ptr<MasterPlaylist> playlist) {
    notifier_.master_playlist_ = playlist.Pass();
  }

  const std::map<uint32_t, MediaPlaylist*>& GetMediaPlaylistMap() {
    return notifier_.media_playlist_map_;
  }

  SimpleHlsNotifier notifier_;
};

TEST_F(SimpleHlsNotifierTest, Init) {
  EXPECT_TRUE(notifier_.Init());
}

TEST_F(SimpleHlsNotifierTest, NotifyNewStream) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, StrEq("video_playlist.m3u8"),
                                   StrEq("name"), StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass());
  InjectMediaPlaylistFactory(factory.Pass());
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "video_playlist.m3u8",
                                        "name", "groupid", &stream_id));
  EXPECT_EQ(1u, GetMediaPlaylistMap().size());
}

TEST_F(SimpleHlsNotifierTest, NotifyNewSegment) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");

  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(_, _, _, _))
      .WillOnce(Return(mock_media_playlist));

  const uint64_t kStartTime = 1328;
  const uint64_t kDuration = 398407;
  const uint64_t kSize = 6595840;
  const std::string segment_name = "segmentname";
  EXPECT_CALL(*mock_media_playlist,
              AddSegment(StrEq(kTestPrefix + segment_name), kDuration, kSize));

  InjectMasterPlaylist(mock_master_playlist.Pass());
  InjectMediaPlaylistFactory(factory.Pass());
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "playlist.m3u8", "name",
                                        "groupid", &stream_id));

  EXPECT_TRUE(notifier_.NotifyNewSegment(stream_id, segment_name, kStartTime,
                                         kDuration, kSize));
}

TEST_F(SimpleHlsNotifierTest, NotifyNewSegmentWithoutStreamsRegistered) {
  EXPECT_TRUE(notifier_.Init());
  EXPECT_FALSE(notifier_.NotifyNewSegment(1u, "anything", 0u, 0u, 0u));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdate) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");

  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(_, _, _, _))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass());
  InjectMediaPlaylistFactory(factory.Pass());
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "playlist.m3u8", "name",
                                        "groupid", &stream_id));

  const uint8_t kSystemIdWidevine[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                       0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                       0xd5, 0x1d, 0x21, 0xed};
  std::vector<uint8_t> system_id(
      kSystemIdWidevine, kSystemIdWidevine + arraysize(kSystemIdWidevine));
  std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
  };
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str;
  ASSERT_TRUE(widevine_pssh_data.SerializeToString(&widevine_pssh_data_str));
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"provider\":\"someprovider\","
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344\",]}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq("0x45454545454545454545454545454545"),
                        StrEq("com.widevine"), _));
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      system_id, iv, pssh_data));
}

// Verify that when there are multiple key IDs in PSSH, the key ID that is
// passed to NotifyEncryptionUpdate() is the first key ID in the json format.
TEST_F(SimpleHlsNotifierTest, MultipleKeyIdsInPssh) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");

  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(_, _, _, _))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass());
  InjectMediaPlaylistFactory(factory.Pass());
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "playlist.m3u8", "name",
                                        "groupid", &stream_id));

  const uint8_t kSystemIdWidevine[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                       0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                       0xd5, 0x1d, 0x21, 0xed};
  std::vector<uint8_t> system_id(
      kSystemIdWidevine, kSystemIdWidevine + arraysize(kSystemIdWidevine));
  std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kFirstKeyId[] = {
    0x11, 0x11, 0x11, 0x11,
  };
  const uint8_t kSecondKeyId[] = {
    0x22, 0x22, 0x22, 0x22,
  };
  widevine_pssh_data.add_key_id()->assign(kFirstKeyId,
                                          kFirstKeyId + arraysize(kFirstKeyId));
  widevine_pssh_data.add_key_id()->assign(
      kSecondKeyId, kSecondKeyId + arraysize(kSecondKeyId));
  std::string widevine_pssh_data_str;
  ASSERT_TRUE(widevine_pssh_data.SerializeToString(&widevine_pssh_data_str));
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"provider\":\"someprovider\","
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"22222222\",\"11111111\",]}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq("0x45454545454545454545454545454545"),
                        StrEq("com.widevine"), _));
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      // Use the second key id here so that it will be thre first one in the
      // key_ids array in the JSON.
      std::vector<uint8_t>(kSecondKeyId,
                           kSecondKeyId + arraysize(kSecondKeyId)),
      system_id, iv, pssh_data));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateEmptyIv) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");

  EXPECT_CALL(
      *mock_master_playlist,
      AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(_, _, _, _))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass());
  InjectMediaPlaylistFactory(factory.Pass());
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "playlist.m3u8", "name",
                                        "groupid", &stream_id));

  const uint8_t kSystemIdWidevine[] = {0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6,
                                       0x4a, 0xce, 0xa3, 0xc8, 0x27, 0xdc,
                                       0xd5, 0x1d, 0x21, 0xed};
  std::vector<uint8_t> system_id(
      kSystemIdWidevine, kSystemIdWidevine + arraysize(kSystemIdWidevine));

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
  };
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str;
  ASSERT_TRUE(widevine_pssh_data.SerializeToString(&widevine_pssh_data_str));
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"provider\":\"someprovider\","
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344\",]}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq(""), StrEq("com.widevine"), _));

  std::vector<uint8_t> empty_iv;
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      system_id, empty_iv, pssh_data));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWithoutStreamsRegistered) {
  std::vector<uint8_t> system_id;
  std::vector<uint8_t> iv;
  std::vector<uint8_t> pssh_data;
  std::vector<uint8_t> key_id;
  EXPECT_TRUE(notifier_.Init());
  EXPECT_FALSE(notifier_.NotifyEncryptionUpdate(1238u, key_id, system_id, iv,
                                                pssh_data));
}

TEST_F(SimpleHlsNotifierTest, Flush) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  EXPECT_CALL(*mock_master_playlist,
              WriteAllPlaylists(StrEq(kTestPrefix), StrEq(kAnyOutputDir)))
      .WillOnce(Return(true));
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  InjectMasterPlaylist(mock_master_playlist.Pass());
  EXPECT_TRUE(notifier_.Init());
  EXPECT_TRUE(notifier_.Flush());
}

}  // namespace hls
}  // namespace edash_packager
