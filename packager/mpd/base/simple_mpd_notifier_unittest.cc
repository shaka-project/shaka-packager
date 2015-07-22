// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/mpd/base/mock_mpd_builder.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/base/simple_mpd_notifier.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"

namespace edash_packager {

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

namespace {
const char kValidMediaInfo[] =
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
const uint32_t kDefaultAdaptationSetId = 0u;
}  // namespace

class SimpleMpdNotifierTest
    : public ::testing::TestWithParam<MpdBuilder::MpdType> {
 protected:
  SimpleMpdNotifierTest()
      : default_mock_adaptation_set_(
            new MockAdaptationSet(kDefaultAdaptationSetId)) {}

  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path_));
    output_path_ = temp_file_path_.value();
  }

  void TearDown() override {
    base::DeleteFile(temp_file_path_, false /* non recursive, just 1 file */);
  }

  scoped_ptr<MockMpdBuilder> StaticMpdBuilderMock() {
    return make_scoped_ptr(new MockMpdBuilder(MpdBuilder::kStatic));
  }

  scoped_ptr<MockMpdBuilder> DynamicMpdBuilderMock() {
    return make_scoped_ptr(new MockMpdBuilder(MpdBuilder::kDynamic));
  }

  MpdBuilder::MpdType GetMpdBuilderType(const SimpleMpdNotifier& notifier) {
    return notifier.MpdBuilderForTesting()->type();
  }

  void SetMpdBuilder(SimpleMpdNotifier* notifier,
                     scoped_ptr<MpdBuilder> mpd_builder) {
    notifier->SetMpdBuilderForTesting(mpd_builder.Pass());
  }

  // Use output_path_ for specifying the MPD output path so that
  // WriteMpdToFile() doesn't crash.
  std::string output_path_;
  const MpdOptions empty_mpd_option_;
  const std::vector<std::string> empty_base_urls_;

  // Default AdaptationSet mock.
  scoped_ptr<MockAdaptationSet> default_mock_adaptation_set_;

 private:
  base::FilePath temp_file_path_;
};

// Verify that it creates the correct MpdBuilder type using DashProfile passed
// to the constructor.
TEST_F(SimpleMpdNotifierTest, CreateCorrectMpdBuilderType) {
  SimpleMpdNotifier on_demand_notifier(kOnDemandProfile, empty_mpd_option_,
                                       empty_base_urls_, output_path_);
  EXPECT_TRUE(on_demand_notifier.Init());
  EXPECT_EQ(MpdBuilder::kStatic,
            GetMpdBuilderType(on_demand_notifier));
  SimpleMpdNotifier live_notifier(kLiveProfile, empty_mpd_option_,
                                  empty_base_urls_, output_path_);
  EXPECT_TRUE(live_notifier.Init());
  EXPECT_EQ(MpdBuilder::kDynamic, GetMpdBuilderType(live_notifier));
}

// Verify NotifyNewContainer() works as expected for VOD.
TEST_P(SimpleMpdNotifierTest, NotifyNewContainer) {
  SimpleMpdNotifier notifier(kOnDemandProfile, empty_mpd_option_,
                             empty_base_urls_, output_path_);

  const uint32_t kRepresentationId = 1u;
  const MpdBuilder::MpdType mpd_type = GetParam();
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type));
  scoped_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  // This is for the Flush() below but adding expectation here because the next
  // lines Pass() the pointer.
  EXPECT_CALL(*mock_mpd_builder, ToString(_)).WillOnce(Return(true));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.Pass());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &unused_container_id));
  EXPECT_TRUE(notifier.Flush());
}

// Verify NotifySampleDuration() works as expected for Live.
TEST_F(SimpleMpdNotifierTest, LiveNotifySampleDuration) {
  SimpleMpdNotifier notifier(kLiveProfile, empty_mpd_option_, empty_base_urls_,
                             output_path_);

  const uint32_t kRepresentationId = 8u;
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(DynamicMpdBuilderMock());
  scoped_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.Pass());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  EXPECT_EQ(kRepresentationId, container_id);

  const uint32_t kSampleDuration = 100;
  EXPECT_CALL(*mock_representation, SetSampleDuration(kSampleDuration));
  EXPECT_TRUE(
      notifier.NotifySampleDuration(kRepresentationId, kSampleDuration));
}

// Verify that NotifySampleDuration works for OnDemand profile.
// TODO(rkuroiwa): SimpleMpdNotifier returns a container ID but does not
// register it to its map for VOD. Must fix and enable this test.
// This test can be also parmeterized just like NotifyNewContainer() test, once
// it is fixed.
TEST_F(SimpleMpdNotifierTest, OnDemandNotifySampleDuration) {
  SimpleMpdNotifier notifier(kOnDemandProfile, empty_mpd_option_,
                             empty_base_urls_, output_path_);

  const uint32_t kRepresentationId = 14u;
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(StaticMpdBuilderMock());
  scoped_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.Pass());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  EXPECT_EQ(kRepresentationId, container_id);

  const uint32_t kSampleDuration = 100;
  EXPECT_CALL(*mock_representation, SetSampleDuration(kSampleDuration));
  EXPECT_TRUE(
      notifier.NotifySampleDuration(kRepresentationId, kSampleDuration));
}

// Verify that NotifyNewSegment() for live works.
TEST_F(SimpleMpdNotifierTest, LiveNotifyNewSegment) {
  SimpleMpdNotifier notifier(kLiveProfile, empty_mpd_option_, empty_base_urls_,
                             output_path_);

  const uint32_t kRepresentationId = 447834u;
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(DynamicMpdBuilderMock());
  scoped_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.Pass());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  EXPECT_EQ(kRepresentationId, container_id);

  const uint64_t kStartTime = 0u;
  const uint32_t kSegmentDuration = 100u;
  const uint64_t kSegmentSize = 123456u;
  EXPECT_CALL(*mock_representation,
              AddNewSegment(kStartTime, kSegmentDuration, kSegmentSize));

  EXPECT_TRUE(notifier.NotifyNewSegment(kRepresentationId, kStartTime,
                                        kSegmentDuration, kSegmentSize));
}

// Verify AddContentProtectionElement() works. Profile doesn't matter.
TEST_F(SimpleMpdNotifierTest, AddContentProtectionElement) {
  SimpleMpdNotifier notifier(kOnDemandProfile, empty_mpd_option_,
                             empty_base_urls_, output_path_);

  const uint32_t kRepresentationId = 0u;
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(StaticMpdBuilderMock());
  scoped_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.Pass());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  EXPECT_EQ(kRepresentationId, container_id);

  ContentProtectionElement element;
  EXPECT_CALL(*mock_representation, AddContentProtectionElement(_));
  EXPECT_TRUE(notifier.AddContentProtectionElement(kRepresentationId, element));
}

TEST_P(SimpleMpdNotifierTest, UpdateEncryption) {
  const char kProtectedContent[] =
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
  SimpleMpdNotifier notifier(kLiveProfile, empty_mpd_option_, empty_base_urls_,
                             output_path_);
  const uint32_t kRepresentationId = 447834u;
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(DynamicMpdBuilderMock());
  scoped_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.Pass());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kProtectedContent),
                                          &container_id));

  ::testing::Mock::VerifyAndClearExpectations(
      default_mock_adaptation_set_.get());

  // "psshsomethingelse" as uint8 array.
  const uint8_t kBogusNewPssh[] = {0x70, 0x73, 0x73, 0x68, 0x73, 0x6f,
                                   0x6d, 0x65, 0x74, 0x68, 0x69, 0x6e,
                                   0x67, 0x65, 0x6c, 0x73, 0x65};
  const std::vector<uint8_t> kBogusNewPsshVector(
      kBogusNewPssh, kBogusNewPssh + arraysize(kBogusNewPssh));
  const char kBogusNewPsshInBase64[] = "cHNzaHNvbWV0aGluZ2Vsc2U=";

  EXPECT_CALL(*mock_representation,
              UpdateContentProtectionPssh(StrEq("myuuid"),
                                          StrEq(kBogusNewPsshInBase64)));
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      container_id, "myuuid", std::vector<uint8_t>(), kBogusNewPsshVector));
}

INSTANTIATE_TEST_CASE_P(StaticAndDynamic,
                        SimpleMpdNotifierTest,
                        ::testing::Values(MpdBuilder::kStatic,
                                          MpdBuilder::kDynamic));

}  // namespace edash_packager
