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
#include "packager/media/base/protection_system_specific_info.h"
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

  std::unique_ptr<MediaPlaylist> Create(MediaPlaylist::MediaPlaylistType type,
                                        const std::string& file_name,
                                        const std::string& name,
                                        const std::string& group_id) override {
    return std::unique_ptr<MediaPlaylist>(
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

const char kCencProtectionScheme[] = "cenc";
const char kSampleAesProtectionScheme[] = "cbca";

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

  void InjectMediaPlaylistFactory(
      std::unique_ptr<MediaPlaylistFactory> factory) {
    notifier_.media_playlist_factory_ = std::move(factory);
  }

  void InjectMediaPlaylistFactory(std::unique_ptr<MediaPlaylistFactory> factory,
                                  SimpleHlsNotifier* notifier) {
    notifier->media_playlist_factory_ = std::move(factory);
  }

  void InjectMasterPlaylist(std::unique_ptr<MasterPlaylist> playlist) {
    notifier_.master_playlist_ = std::move(playlist);
  }

  void InjectMasterPlaylist(std::unique_ptr<MasterPlaylist> playlist,
                            SimpleHlsNotifier* notifier) {
    notifier->master_playlist_ = std::move(playlist);
  }

  size_t NumRegisteredMediaPlaylists() { return notifier_.stream_map_.size(); }

  uint32_t SetupStream(const std::string& protection_scheme,
                       MockMediaPlaylist* mock_media_playlist) {
    MediaInfo media_info;
    media_info.mutable_protected_content()->set_protection_scheme(
        protection_scheme);
    std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
        new MockMasterPlaylist());
    std::unique_ptr<MockMediaPlaylistFactory> factory(
        new MockMediaPlaylistFactory());

    EXPECT_CALL(
        *mock_master_playlist,
        AddMediaPlaylist(static_cast<MediaPlaylist*>(mock_media_playlist)));
    EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
    EXPECT_CALL(*factory, CreateMock(_, _, _, _))
        .WillOnce(Return(mock_media_playlist));

    InjectMasterPlaylist(std::move(mock_master_playlist));
    InjectMediaPlaylistFactory(std::move(factory));
    EXPECT_TRUE(notifier_.Init());
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
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

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

  InjectMasterPlaylist(std::move(mock_master_playlist));
  InjectMediaPlaylistFactory(std::move(factory));
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

  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

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

  InjectMasterPlaylist(std::move(mock_master_playlist), &test_notifier);
  InjectMediaPlaylistFactory(std::move(factory), &test_notifier);
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

  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

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

  InjectMasterPlaylist(std::move(mock_master_playlist), &test_notifier);
  InjectMediaPlaylistFactory(std::move(factory), &test_notifier);
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
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  EXPECT_CALL(*mock_master_playlist, AddMediaPlaylist(mock_media_playlist));

  EXPECT_CALL(*mock_media_playlist, SetMediaInfo(_)).WillOnce(Return(true));
  EXPECT_CALL(*factory, CreateMock(kVodPlaylist, StrEq("video_playlist.m3u8"),
                                   StrEq("name"), StrEq("groupid")))
      .WillOnce(Return(mock_media_playlist));

  InjectMasterPlaylist(std::move(mock_master_playlist));
  InjectMediaPlaylistFactory(std::move(factory));
  EXPECT_TRUE(notifier_.Init());
  MediaInfo media_info;
  uint32_t stream_id;
  EXPECT_TRUE(notifier_.NotifyNewStream(media_info, "video_playlist.m3u8",
                                        "name", "groupid", &stream_id));
  EXPECT_EQ(1u, NumRegisteredMediaPlaylists());
}

TEST_F(SimpleHlsNotifierTest, NotifyNewSegment) {
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

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

  InjectMasterPlaylist(std::move(mock_master_playlist));
  InjectMediaPlaylistFactory(std::move(factory));
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
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
  };
  std::vector<uint8_t> any_key_id(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId));
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();

  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(any_key_id);

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344112233441122334411223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(_,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq(""),
                        StrEq("0x45454545454545454545454545454545"),
                        StrEq("com.widevine"), _));
  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _,
                  StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x11223344112233441122334411223344"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id, any_key_id, widevine_system_id_, iv, pssh_box));
}

// Verify that key_ids in pssh is optional.
TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateWidevineNoKeyidsInPssh) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist);

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
      "\"key_ids\":[\"11223344112233441122334411223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(_,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq(""),
                        StrEq("0x45454545454545454545454545454545"),
                        StrEq("com.widevine"), _));
  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          _,
          StrEq("data:text/plain;base64," + expected_pssh_base64),
          StrEq("0x11223344112233441122334411223344"),
          StrEq("0x45454545454545454545454545454545"),
          StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
  };
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      widevine_system_id_, iv, pssh_box));
}

TEST_F(SimpleHlsNotifierTest, NotifyEncryptionUpdateFixedKey) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist);

  const std::vector<uint8_t> key_id(16, 0x23);
  const std::vector<uint8_t> iv(16, 0x45);
  const std::vector<uint8_t> dummy_pssh_data(10, 'p');

  std::string expected_key_uri_base64;
  base::Base64Encode(std::string(key_id.begin(), key_id.end()),
                     &expected_key_uri_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          _,
          StrEq("data:text/plain;base64," + expected_key_uri_base64),
          StrEq(""),
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
  uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist);

  std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  const uint8_t kFirstKeyId[] = {
    0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11,
    0x11, 0x11, 0x11, 0x11,
  };
  const uint8_t kSecondKeyId[] = {
    0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22,
    0x22, 0x22, 0x22, 0x22,
  };
  std::vector<uint8_t> first_keyid(kFirstKeyId,
                                   kFirstKeyId + arraysize(kFirstKeyId));
  std::vector<uint8_t> second_keyid(kSecondKeyId,
                                    kSecondKeyId + arraysize(kSecondKeyId));

  widevine_pssh_data.add_key_id()->assign(kFirstKeyId,
                                          kFirstKeyId + arraysize(kFirstKeyId));
  widevine_pssh_data.add_key_id()->assign(
      kSecondKeyId, kSecondKeyId + arraysize(kSecondKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(first_keyid);
  pssh_info.add_key_id(second_keyid);

  const char kExpectedJson[] =
      "{"
      "\"key_ids\":[\"22222222222222222222222222222222\","
      "\"11111111111111111111111111111111\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(_,
                        StrEq("data:text/plain;base64," + expected_json_base64),
                        StrEq(""),
                        StrEq("0x45454545454545454545454545454545"),
                        StrEq("com.widevine"), _));

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _,
                  StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x22222222222222222222222222222222"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));

  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      // Use the second key id here so that it will be thre first one in the
      // key_ids array in the JSON.
      second_keyid, widevine_system_id_, iv, pssh_box));
}

// Verify that the encryption scheme set in MediaInfo is passed to
// MediaPlaylist::AddEncryptionInfo().
TEST_F(SimpleHlsNotifierTest, EncryptionScheme) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id =
      SetupStream(kCencProtectionScheme, mock_media_playlist);

  const std::vector<uint8_t> key_id(16, 0x23);
  const std::vector<uint8_t> iv(16, 0x45);
  const std::vector<uint8_t> dummy_pssh_data(10, 'p');

  std::string expected_key_uri_base64;
  base::Base64Encode(std::string(key_id.begin(), key_id.end()),
                     &expected_key_uri_base64);

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          MediaPlaylist::EncryptionMethod::kSampleAesCenc,
          StrEq("data:text/plain;base64," + expected_key_uri_base64),
          StrEq(""),
          StrEq("0x45454545454545454545454545454545"), StrEq("identity"), _));
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id, key_id, common_system_id_, iv, dummy_pssh_data));
}

// If using 'cenc' with Widevine, don't output the json form.
TEST_F(SimpleHlsNotifierTest, WidevineCencEncryptionScheme) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id =
      SetupStream(kCencProtectionScheme, mock_media_playlist);

  const std::vector<uint8_t> iv(16, 0x45);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
  };
  std::vector<uint8_t> any_key_id(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId));
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();

  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(any_key_id);

  std::string expected_pssh_base64;
  const std::vector<uint8_t> pssh_box = pssh_info.CreateBox();
  base::Base64Encode(std::string(pssh_box.begin(), pssh_box.end()),
                     &expected_pssh_base64);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _,
                  StrEq("data:text/plain;base64," + expected_pssh_base64),
                  StrEq("0x11223344112233441122334411223344"),
                  StrEq("0x45454545454545454545454545454545"),
                  StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), _));
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id, any_key_id, widevine_system_id_, iv, pssh_box));
}

TEST_F(SimpleHlsNotifierTest, WidevineNotifyEncryptionUpdateEmptyIv) {
  // Pointer released by SimpleHlsNotifier.
  MockMediaPlaylist* mock_media_playlist =
      new MockMediaPlaylist(kVodPlaylist, "", "", "");
  const uint32_t stream_id =
      SetupStream(kSampleAesProtectionScheme, mock_media_playlist);

  media::WidevinePsshData widevine_pssh_data;
  widevine_pssh_data.set_provider("someprovider");
  widevine_pssh_data.set_content_id("contentid");
  const uint8_t kAnyKeyId[] = {
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
    0x11, 0x22, 0x33, 0x44,
  };
  std::vector<uint8_t> any_key_id(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId));
  widevine_pssh_data.add_key_id()->assign(kAnyKeyId,
                                          kAnyKeyId + arraysize(kAnyKeyId));
  std::string widevine_pssh_data_str = widevine_pssh_data.SerializeAsString();
  EXPECT_TRUE(!widevine_pssh_data_str.empty());
  std::vector<uint8_t> pssh_data(widevine_pssh_data_str.begin(),
                                 widevine_pssh_data_str.end());

  const char kExpectedJson[] =
      "{"
      "\"content_id\":\"Y29udGVudGlk\","
      "\"key_ids\":[\"11223344112233441122334411223344\"],"
      "\"provider\":\"someprovider\"}";
  std::string expected_json_base64;
  base::Base64Encode(kExpectedJson, &expected_json_base64);

  media::ProtectionSystemSpecificInfo pssh_info;
  pssh_info.set_pssh_data(pssh_data);
  pssh_info.set_system_id(widevine_system_id_.data(),
                          widevine_system_id_.size());
  pssh_info.add_key_id(any_key_id);

  EXPECT_CALL(*mock_media_playlist,
              AddEncryptionInfo(
                  _, StrEq("data:text/plain;base64," + expected_json_base64),
                  StrEq(""), StrEq(""), StrEq("com.widevine"), StrEq("1")));

  EXPECT_CALL(
      *mock_media_playlist,
      AddEncryptionInfo(
          _,
          StrEq("data:text/plain;base64,"
                "AAAAS3Bzc2gAAAAA7e+"
                "LqXnWSs6jyCfc1R0h7QAAACsSEBEiM0QRIjNEESIzRBEiM0QaDHNvb"
                "WVwcm92aWRlciIJY29udGVudGlk"),
          StrEq("0x11223344112233441122334411223344"), StrEq(""),
          StrEq("urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"), StrEq("1")));
  std::vector<uint8_t> pssh_as_vec = pssh_info.CreateBox();
  std::string pssh_in_string(pssh_as_vec.begin(), pssh_as_vec.end());
  std::string base_64_encoded_pssh;
  base::Base64Encode(pssh_in_string, &base_64_encoded_pssh);
  LOG(INFO) << base_64_encoded_pssh;

  std::vector<uint8_t> empty_iv;
  EXPECT_TRUE(notifier_.NotifyEncryptionUpdate(
      stream_id,
      std::vector<uint8_t>(kAnyKeyId, kAnyKeyId + arraysize(kAnyKeyId)),
      widevine_system_id_, empty_iv, pssh_info.CreateBox()));
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
  std::unique_ptr<MockMasterPlaylist> mock_master_playlist(
      new MockMasterPlaylist());
  EXPECT_CALL(*mock_master_playlist,
              WriteAllPlaylists(StrEq(kTestPrefix), StrEq(kAnyOutputDir)))
      .WillOnce(Return(true));
  std::unique_ptr<MockMediaPlaylistFactory> factory(
      new MockMediaPlaylistFactory());

  InjectMasterPlaylist(std::move(mock_master_playlist));
  EXPECT_TRUE(notifier_.Init());
  EXPECT_TRUE(notifier_.Flush());
}

}  // namespace hls
}  // namespace shaka
