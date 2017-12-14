// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/mpd/base/dash_iop_mpd_notifier.h"
#include "packager/mpd/base/mock_mpd_builder.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"

namespace shaka {

using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrEq;

namespace {

const uint32_t kDefaultAdaptationSetId = 0u;
const uint32_t kDefaultRepresentationId = 1u;
const bool kContentProtectionInAdaptationSet = true;

MATCHER_P(EqualsProto, message, "") {
  return ::google::protobuf::util::MessageDifferencer::Equals(arg, message);
}

}  // namespace

// TODO(rkuroiwa): This is almost exactly the same as SimpleMpdNotifierTest but
// replaced all SimpleMpd with DashIopMpd,
// use typed tests
// (https://code.google.com/p/googletest/wiki/AdvancedGuide#Typed_Tests);
// also because SimpleMpdNotifier and DashIopMpdNotifier have common behavior
// for most of the public functions.
class DashIopMpdNotifierTest : public ::testing::Test {
 protected:
  DashIopMpdNotifierTest()
      : default_mock_period_(new MockPeriod),
        default_mock_adaptation_set_(
            new MockAdaptationSet(kDefaultAdaptationSetId)),
        default_mock_representation_(
            new MockRepresentation(kDefaultRepresentationId)) {}

  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path_));
    empty_mpd_option_.mpd_params.mpd_output = temp_file_path_.AsUTF8Unsafe();

    // Three valid media info. The actual data does not matter.
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
    valid_media_info1_ = ConvertToMediaInfo(kValidMediaInfo);
    valid_media_info2_ = valid_media_info1_;
    valid_media_info2_.mutable_video_info()->set_width(960);
    valid_media_info3_ = valid_media_info1_;
    valid_media_info3_.mutable_video_info()->set_width(480);
  }

  void TearDown() override {
    base::DeleteFile(temp_file_path_, false /* non recursive, just 1 file */);
  }

  void SetMpdBuilder(DashIopMpdNotifier* notifier,
                     std::unique_ptr<MpdBuilder> mpd_builder) {
    notifier->SetMpdBuilderForTesting(std::move(mpd_builder));
  }

 protected:
  // Empty mpd options except with output path specified, so that
  // WriteMpdToFile() doesn't crash.
  MpdOptions empty_mpd_option_;

  // Default mocks that can be used for the tests.
  // IOW, if a test only requires one instance of
  // Mock{Period,AdaptationSet,Representation}, these can be used.
  std::unique_ptr<MockPeriod> default_mock_period_;
  std::unique_ptr<MockAdaptationSet> default_mock_adaptation_set_;
  std::unique_ptr<MockRepresentation> default_mock_representation_;

  // Three valid media info. The actual content does not matter.
  MediaInfo valid_media_info1_;
  MediaInfo valid_media_info2_;
  MediaInfo valid_media_info3_;

 private:
  base::FilePath temp_file_path_;
};

// Verify that basic VOD NotifyNewContainer() operation works.
// No encrypted contents.
TEST_F(DashIopMpdNotifierTest, NotifyNewContainer) {
  DashIopMpdNotifier notifier(empty_mpd_option_);

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder());

  EXPECT_CALL(*mock_mpd_builder, AddPeriod())
      .WillOnce(Return(default_mock_period_.get()));
  EXPECT_CALL(*default_mock_period_,
              GetOrCreateAdaptationSet(EqualsProto(valid_media_info1_),
                                       Eq(kContentProtectionInAdaptationSet)))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  // This is for the Flush() below but adding expectation here because the next
  // std::move(lines) the pointer.
  EXPECT_CALL(*mock_mpd_builder, ToString(_)).WillOnce(Return(true));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(
      notifier.NotifyNewContainer(valid_media_info1_, &unused_container_id));
  EXPECT_TRUE(notifier.Flush());
}

// AddContentProtection() should not work and should always return false.
TEST_F(DashIopMpdNotifierTest, AddContentProtection) {
  DashIopMpdNotifier notifier(empty_mpd_option_);

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder());

  EXPECT_CALL(*mock_mpd_builder, AddPeriod())
      .WillOnce(Return(default_mock_period_.get()));
  EXPECT_CALL(*default_mock_period_, GetOrCreateAdaptationSet(_, _))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(valid_media_info1_, &container_id));

  ContentProtectionElement empty_content_protection_element;
  EXPECT_FALSE(notifier.AddContentProtectionElement(
      container_id, empty_content_protection_element));
}

TEST_F(DashIopMpdNotifierTest, UpdateEncryption) {
  DashIopMpdNotifier notifier(empty_mpd_option_);

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder());

  EXPECT_CALL(*mock_mpd_builder, AddPeriod())
      .WillOnce(Return(default_mock_period_.get()));
  EXPECT_CALL(*default_mock_period_, GetOrCreateAdaptationSet(_, _))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(valid_media_info1_, &container_id));

  ::testing::Mock::VerifyAndClearExpectations(
      default_mock_adaptation_set_.get());

  const uint8_t kBogusNewPssh[] = {// "psshsomethingelse" as uint8 array.
                                   0x70, 0x73, 0x73, 0x68, 0x73, 0x6f,
                                   0x6d, 0x65, 0x74, 0x68, 0x69, 0x6e,
                                   0x67, 0x65, 0x6c, 0x73, 0x65};
  const std::vector<uint8_t> kBogusNewPsshVector(
      kBogusNewPssh, kBogusNewPssh + arraysize(kBogusNewPssh));
  const char kBogusNewPsshInBase64[] = "cHNzaHNvbWV0aGluZ2Vsc2U=";

  EXPECT_CALL(*default_mock_adaptation_set_,
              UpdateContentProtectionPssh(StrEq("myuuid"),
                                          StrEq(kBogusNewPsshInBase64)));
  EXPECT_TRUE(notifier.NotifyEncryptionUpdate(
      container_id, "myuuid", std::vector<uint8_t>(), kBogusNewPsshVector));
}

// This test is mainly for tsan. Using both the notifier and the MpdBuilder.
// Although locks in MpdBuilder have been removed,
// https://github.com/google/shaka-packager/issues/45
// This issue identified a bug where using SimpleMpdNotifier with multiple
// threads causes a deadlock. This tests with DashIopMpdNotifier.
TEST_F(DashIopMpdNotifierTest, NotifyNewContainerAndSampleDurationNoMock) {
  DashIopMpdNotifier notifier(empty_mpd_option_);
  uint32_t container_id;
  EXPECT_TRUE(notifier.NotifyNewContainer(valid_media_info1_, &container_id));
  const uint32_t kAnySampleDuration = 1000;
  EXPECT_TRUE(notifier.NotifySampleDuration(container_id,  kAnySampleDuration));
  EXPECT_TRUE(notifier.Flush());
}

// Test multiple media info with some belongs to the same AdaptationSets.
TEST_F(DashIopMpdNotifierTest, MultipleMediaInfo) {
  DashIopMpdNotifier notifier(empty_mpd_option_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder());

  std::unique_ptr<MockAdaptationSet> adaptation_set1(new MockAdaptationSet(1));
  std::unique_ptr<MockAdaptationSet> adaptation_set2(new MockAdaptationSet(2));

  std::unique_ptr<MockRepresentation> representation1(
      new MockRepresentation(1));
  std::unique_ptr<MockRepresentation> representation2(
      new MockRepresentation(2));
  std::unique_ptr<MockRepresentation> representation3(
      new MockRepresentation(3));

  EXPECT_CALL(*mock_mpd_builder, AddPeriod())
      .WillOnce(Return(default_mock_period_.get()));

  EXPECT_CALL(*default_mock_period_,
              GetOrCreateAdaptationSet(EqualsProto(valid_media_info1_), _))
      .WillOnce(Return(adaptation_set1.get()));
  EXPECT_CALL(*adaptation_set1,
              AddRepresentation(EqualsProto(valid_media_info1_)))
      .WillOnce(Return(representation1.get()));
  // Return the same adaptation set for |valid_media_info2_| and
  // |valid_media_info3_|. This results in AddRepresentation to be called twice
  // on |adaptation_set2|.
  EXPECT_CALL(*default_mock_period_,
              GetOrCreateAdaptationSet(AnyOf(EqualsProto(valid_media_info2_),
                                             EqualsProto(valid_media_info3_)),
                                       _))
      .WillOnce(Return(adaptation_set2.get()))
      .WillOnce(Return(adaptation_set2.get()));
  EXPECT_CALL(*adaptation_set2,
              AddRepresentation(AnyOf(EqualsProto(valid_media_info2_),
                                      EqualsProto(valid_media_info3_))))
      .WillOnce(Return(representation2.get()))
      .WillOnce(Return(representation3.get()));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(
      notifier.NotifyNewContainer(valid_media_info1_, &unused_container_id));
  EXPECT_TRUE(
      notifier.NotifyNewContainer(valid_media_info2_, &unused_container_id));
  EXPECT_TRUE(
      notifier.NotifyNewContainer(valid_media_info3_, &unused_container_id));
}

}  // namespace shaka
