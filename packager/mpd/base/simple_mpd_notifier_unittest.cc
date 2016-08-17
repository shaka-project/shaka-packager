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

namespace shaka {

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
    output_path_ = temp_file_path_.AsUTF8Unsafe();
  }

  void TearDown() override {
    base::DeleteFile(temp_file_path_, false /* non recursive, just 1 file */);
  }

  std::unique_ptr<MockMpdBuilder> StaticMpdBuilderMock() {
    return std::unique_ptr<MockMpdBuilder>(
        new MockMpdBuilder(MpdBuilder::kStatic));
  }

  std::unique_ptr<MockMpdBuilder> DynamicMpdBuilderMock() {
    return std::unique_ptr<MockMpdBuilder>(
        new MockMpdBuilder(MpdBuilder::kDynamic));
  }

  MpdBuilder::MpdType GetMpdBuilderType(const SimpleMpdNotifier& notifier) {
    return notifier.MpdBuilderForTesting()->type();
  }

  void SetMpdBuilder(SimpleMpdNotifier* notifier,
                     std::unique_ptr<MpdBuilder> mpd_builder) {
    notifier->SetMpdBuilderForTesting(std::move(mpd_builder));
  }

  // Use output_path_ for specifying the MPD output path so that
  // WriteMpdToFile() doesn't crash.
  std::string output_path_;
  const MpdOptions empty_mpd_option_;
  const std::vector<std::string> empty_base_urls_;

  // Default AdaptationSet mock.
  std::unique_ptr<MockAdaptationSet> default_mock_adaptation_set_;

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
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type));
  std::unique_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  // This is for the Flush() below but adding expectation here because the next
  // std::move(lines) the pointer.
  EXPECT_CALL(*mock_mpd_builder, ToString(_)).WillOnce(Return(true));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &unused_container_id));
  EXPECT_TRUE(notifier.Flush());
}

// Verify NotifySampleDuration() works as expected for Live.
TEST_F(SimpleMpdNotifierTest, LiveNotifySampleDuration) {
  SimpleMpdNotifier notifier(kLiveProfile, empty_mpd_option_, empty_base_urls_,
                             output_path_);

  const uint32_t kRepresentationId = 8u;
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(DynamicMpdBuilderMock());
  std::unique_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
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
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(StaticMpdBuilderMock());
  std::unique_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  EXPECT_EQ(kRepresentationId, container_id);

  const uint32_t kSampleDuration = 100;
  EXPECT_CALL(*mock_representation, SetSampleDuration(kSampleDuration));
  EXPECT_TRUE(
      notifier.NotifySampleDuration(kRepresentationId, kSampleDuration));
}

// This test is mainly for tsan. Using both the notifier and the MpdBuilder.
// Although locks in MpdBuilder have been removed,
// https://github.com/google/shaka-packager/issues/45
// This issue identified a bug where using SimpleMpdNotifier with multiple
// threads causes a deadlock.
TEST_F(SimpleMpdNotifierTest, NotifyNewContainerAndSampleDurationNoMock) {
  SimpleMpdNotifier notifier(kOnDemandProfile, empty_mpd_option_,
                             empty_base_urls_, output_path_);
  uint32_t container_id;
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  const uint32_t kAnySampleDuration = 1000;
  EXPECT_TRUE(notifier.NotifySampleDuration(container_id,  kAnySampleDuration));
  EXPECT_TRUE(notifier.Flush());
}

// Verify that NotifyNewSegment() for live works.
TEST_F(SimpleMpdNotifierTest, LiveNotifyNewSegment) {
  SimpleMpdNotifier notifier(kLiveProfile, empty_mpd_option_, empty_base_urls_,
                             output_path_);

  const uint32_t kRepresentationId = 447834u;
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(DynamicMpdBuilderMock());
  std::unique_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
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
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(StaticMpdBuilderMock());
  std::unique_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
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
      "    pssh: 'psshsomethingelse'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "container_type: 1\n";
  SimpleMpdNotifier notifier(kLiveProfile, empty_mpd_option_, empty_base_urls_,
                             output_path_);
  const uint32_t kRepresentationId = 447834u;
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(DynamicMpdBuilderMock());
  std::unique_ptr<MockRepresentation> mock_representation(
      new MockRepresentation(kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(mock_representation.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
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

// Don't put different audio languages or codecs in the same AdaptationSet.
TEST_P(SimpleMpdNotifierTest, SplitAdaptationSetsByLanguageAndCodec) {
  // MP4, English
  const char kAudioContent1[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'eng'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";

  // MP4, German
  const char kAudioContent2[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_MP4\n"
      "media_duration_seconds: 10.5\n";

  // WebM, German
  const char kAudioContent3[] =
      "audio_info {\n"
      "  codec: 'vorbis'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_WEBM\n"
      "media_duration_seconds: 10.5\n";

  // WebM, German again
  const char kAudioContent4[] =
      "audio_info {\n"
      "  codec: 'vorbis'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "  language: 'ger'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: CONTAINER_WEBM\n"
      "media_duration_seconds: 10.5\n";

  SimpleMpdNotifier notifier(kOnDemandProfile, empty_mpd_option_,
                             empty_base_urls_, output_path_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(StaticMpdBuilderMock());

  std::unique_ptr<MockAdaptationSet> adaptation_set1(new MockAdaptationSet(1));
  std::unique_ptr<MockAdaptationSet> adaptation_set2(new MockAdaptationSet(2));
  std::unique_ptr<MockAdaptationSet> adaptation_set3(new MockAdaptationSet(3));

  std::unique_ptr<MockRepresentation> representation1(
      new MockRepresentation(1));
  std::unique_ptr<MockRepresentation> representation2(
      new MockRepresentation(2));
  std::unique_ptr<MockRepresentation> representation3(
      new MockRepresentation(3));
  std::unique_ptr<MockRepresentation> representation4(
      new MockRepresentation(4));

  // We expect three AdaptationSets.
  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(adaptation_set1.get()))
      .WillOnce(Return(adaptation_set2.get()))
      .WillOnce(Return(adaptation_set3.get()));
  // The first AdaptationSet should have Eng MP4, one Representation.
  EXPECT_CALL(*adaptation_set1, AddRepresentation(_))
      .WillOnce(Return(representation1.get()));
  // The second AdaptationSet should have Ger MP4, one Representation.
  EXPECT_CALL(*adaptation_set2, AddRepresentation(_))
      .WillOnce(Return(representation2.get()));
  // The third AdaptationSet should have Ger WebM, two Representations.
  EXPECT_CALL(*adaptation_set3, AddRepresentation(_))
      .WillOnce(Return(representation3.get()))
      .WillOnce(Return(representation4.get()));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kAudioContent1), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kAudioContent2), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kAudioContent3), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kAudioContent4), &unused_container_id));
}

INSTANTIATE_TEST_CASE_P(StaticAndDynamic,
                        SimpleMpdNotifierTest,
                        ::testing::Values(MpdBuilder::kStatic,
                                          MpdBuilder::kDynamic));

}  // namespace shaka
