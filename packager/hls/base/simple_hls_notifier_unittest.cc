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
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/widevine_key_source.h"
#include "packager/media/base/widevine_pssh_data.pb.h"

namespace shaka {
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

const uint64_t kAnyStartTime = 10;
const uint64_t kAnyDuration = 1000;
const uint64_t kAnySize = 2000;

MATCHER_P(SegmentTemplateEq, expected_template, "") {
  *result_listener << " which is " << arg.segment_template();
  return arg.segment_template() == expected_template;
}

}  // namespace

class SimpleHlsNotifierTest : public ::testing::Test {
 protected:
  SimpleHlsNotifierTest()
      : notifier_(HlsNotifier::HlsProfile::kOnDemandProfile,
                  kTestPrefix,
                  kAnyOutputDir,
                  kMasterPlaylistName),
        widevine_system_id_(
            media::kWidevineSystemId,
            media::kWidevineSystemId + arraysize(media::kWidevineSystemId)),
        common_system_id_(
            media::kCommonSystemId,
            media::kCommonSystemId + arraysize(media::kCommonSystemId)) {}

  void InjectMediaPlaylistFactory(scoped_ptr<MediaPlaylistFactory> factory) {
    notifier_.media_playlist_factory_ = factory.Pass();
  }

  void InjectMediaPlaylistFactory(scoped_ptr<MediaPlaylistFactory> factory,
                                  SimpleHlsNotifier* notifier) {
    notifier->media_playlist_factory_ = factory.Pass();
  }

  void InjectMasterPlaylist(scoped_ptr<MasterPlaylist> playlist) {
    notifier_.master_playlist_ = playlist.Pass();
  }

  void InjectMasterPlaylist(scoped_ptr<MasterPlaylist> playlist,
                            SimpleHlsNotifier* notifier) {
    notifier->master_playlist_ = playlist.Pass();
  }

  const std::map<uint32_t, MediaPlaylist*>& GetMediaPlaylistMap() {
    return notifier_.media_playlist_map_;
  }

  uint32_t SetupStream(MockMediaPlaylist* mock_media_playlist) {
    scoped_ptr<MockMasterPlaylist> mock_master_playlist(
        new MockMasterPlaylist());
    scoped_ptr<MockMediaPlaylistFactory> factory(
        new MockMediaPlaylistFactory());

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
    return stream_id;
  }

  SimpleHlsNotifier notifier_;
  const std::vector<uint8_t> widevine_system_id_;
  const std::vector<uint8_t> common_system_id_;
};

TEST_F(SimpleHlsNotifierTest, Init) {
  EXPECT_TRUE(notifier_.Init());
}

// Verify that relative paths can be handled.
// For this test, since the prefix "anything/" matches, the prefix should be
// stripped.
TEST_F(SimpleHlsNotifierTest, RebaseSegmentTemplateRelative) {
  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(
      *mock_media_playlist,
      SetMediaInfo(SegmentTemplateEq("path/to/media$Number$.ts")))
      .WillOnce(Return(true));

  // Verify that the common prefix is stripped for AddSegment().
  EXPECT_CALL(*mock_media_playlist,
              AddSegment("http://testprefix.com/path/to/media1.ts", _, _));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, StrEq("video_playlist.m3u8"),
                                   StrEq("name"), StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass());
  InjectMediaPlaylistFactory(factory.Pass());
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  media_info.set_segment_template("anything/path/to/media$Number$.ts");
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "video_playlist.m3u8",
                                        "name", "groupid", &stream_id));

  EXPECT_TRUE(
      notifier_.NotifyNewSegment(stream_id, "anything/path/to/media1.ts",
                                 kAnyStartTime, kAnyDuration, kAnySize));
}

// Verify that when segment template's prefix and output dir match, then the
// prefix is stripped from segment template.
TEST_F(SimpleHlsNotifierTest,
       RebaseAbsoluteSegmentTemplatePrefixAndOutputDirMatch) {
  const char kAbsoluteOutputDir[] = "/tmp/something/";
  // Require a separate instance to set kAbsoluteOutputDir.
  SimpleHlsNotifier test_notifier(HlsNotifier::HlsProfile::kOnDemandProfile,
                                  kTestPrefix, kAbsoluteOutputDir,
                                  kMasterPlaylistName);

  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(*mock_media_playlist,
              SetMediaInfo(SegmentTemplateEq("media$Number$.ts")))
      .WillOnce(Return(true));

  // Verify that the output_dir is stripped and then kTestPrefix is prepended.
  EXPECT_CALL(*mock_media_playlist,
              AddSegment("http://testprefix.com/media1.ts", _, _));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, StrEq("video_playlist.m3u8"),
                                   StrEq("name"), StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass(), &test_notifier);
  InjectMediaPlaylistFactory(factory.Pass(), &test_notifier);
  EXPECT_TRUE(test_notifier.Init());
  MediaInfo media_info;
  media_info.set_segment_template("/tmp/something/media$Number$.ts");
  uint32_t stream_id;
  EXPECT_TRUE(test_notifier.NotifyNewStream(media_info, "video_playlist.m3u8",
                                            "name", "groupid", &stream_id));

  EXPECT_TRUE(
      test_notifier.NotifyNewSegment(stream_id, "/tmp/something/media1.ts",
                                     kAnyStartTime, kAnyDuration, kAnySize));
}

// If the paths don't match at all and they are both absolute and completely
// different, then keep it as is.
TEST_F(SimpleHlsNotifierTest,
       RebaseAbsoluteSegmentTemplateCompletelyDifferentDirectory) {
  const char kAbsoluteOutputDir[] = "/tmp/something/";
  SimpleHlsNotifier test_notifier(HlsNotifier::HlsProfile::kOnDemandProfile,
                                  kTestPrefix, kAbsoluteOutputDir,
                                  kMasterPlaylistName);

  scoped_ptr<MockMasterPlaylist> mock_master_playlist(new MockMasterPlaylist());
  scoped_ptr<MockMediaPlaylistFactory> factory(new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(
      *mock_media_playlist,
      SetMediaInfo(SegmentTemplateEq("/var/somewhereelse/media$Number$.ts")))
      .WillOnce(Return(true));
  EXPECT_CALL(
      *mock_media_playlist,
      AddSegment("http://testprefix.com//var/somewhereelse/media1.ts", _, _));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, StrEq("video_playlist.m3u8"),
                                   StrEq("name"), StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(mock_master_playlist.Pass(), &test_notifier);
  InjectMediaPlaylistFactory(factory.Pass(), &test_notifier);
  EXPECT_TRUE(test_notifier.Init());
  MediaInfo media_info;
  media_info.set_segment_template("/var/somewhereelse/media$Number$.ts");
  uint32_t stream_id;
  EXPECT_TRUE(test_notifier.NotifyNewStream(media_info, "video_playlist.m3u8",
                                            "name", "groupid", &stream_id));
  EXPECT_TRUE(
      test_notifier.NotifyNewSegment(stream_id, "/var/somewhereelse/media1.ts",
                                     kAnyStartTime, kAnyDuration, kAnySize));
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

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWidevine) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id = SetupStream(mock_media_playlist);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
  };
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344\"],"
      "\"provider\":\"someprovider\"}";
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
      widevine_system_id_, iv, pssh_data));
}

// Verify that key_ids in pssh is optional.
TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWidevineNoKeyidsInPssh) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id = SetupStream(mock_media_playlist);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(MediaPlaylist::EncryptionMethod::kSampleAes,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq("0x45454545454545454545454545454545"),
                        StrEq("com.widevine"), _));
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
  };
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      widevine_system_id_, iv, pssh_data));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateFixedKey) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id = SetupStream(mock_media_playlist);

  const std::vector<uint8_t> key_id(16, 0x23);
  const std::vector<uint8_t> iv(16, 0x45);
  const std::vector<uint8_t> dummy_pssh_data(10, 'p');

  std::string expected_key_uri_base64;
  base::Base64Encode(std::string(key_id.begin(), key_id.end()),
                     &expected_key_uri_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          MediaPlaylist::EncryptionMethod::kSampleAes,
          StrEq("data:text/plain;base64," + expected_key_uri_base64),
          StrEq("0x45454545454545454545454545454545"), StrEq("identity"), _));
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id, key_id, common_system_id_, iv, dummy_pssh_data));
}

// Verify that when there are multiple key IDs in PSSH, the key ID that is
// passed to NotifyEncryptionUpdate() is the first key ID in the json format.
// Also verify that content_id is optional.
TEST_F(SimpleHlsNotifierTest, WidevineMultipleKeyIdsNoContentIdInPssh) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  uint32_t stream_id = SetupStream(mock_media_playlist);

  std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
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
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"key_ids\":[\"22222222\",\"11111111\"],"
      "\"provider\":\"someprovider\"}";
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
      widevine_system_id_, iv, pssh_data));
}

TEST_F(SimpleHlsNotifierTest, WidevineNotifyEncryptionUpdateEmptyIv) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id = SetupStream(mock_media_playlist);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
  };
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344\"],"
      "\"provider\":\"someprovider\"}";
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
      widevine_system_id_, empty_iv, pssh_data));
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
}  // namespace shaka
