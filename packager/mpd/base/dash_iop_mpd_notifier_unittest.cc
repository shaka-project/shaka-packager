// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/file_util.h"
#include "packager/base/files/file_path.h"
#include "packager/mpd/base/dash_iop_mpd_notifier.h"
#include "packager/mpd/base/mock_mpd_builder.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"

namespace edash_packager {

using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;

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
const uint32_t kDefaultRepresentationId = 1u;
const int kDefaultGroupId = -1;

bool ElementEqual(const edash_packager::Element& lhs,
                  const edash_packager::Element& rhs) {
  const bool all_equal_except_sublement_check =
      lhs.name == rhs.name && lhs.attributes.size() == rhs.attributes.size() &&
      std::equal(lhs.attributes.begin(), lhs.attributes.end(),
                 rhs.attributes.begin()) &&
      lhs.content == rhs.content &&
      lhs.subelements.size() == rhs.subelements.size();

  if (!all_equal_except_sublement_check) {
    return false;
  }

  for (size_t i = 0; i < lhs.subelements.size(); ++i) {
    if (!ElementEqual(lhs.subelements[i], rhs.subelements[i]))
      return false;
  }
  return true;
}

bool ContentProtectionElementEqual(
    const edash_packager::ContentProtectionElement& lhs,
    const edash_packager::ContentProtectionElement& rhs) {
  const bool all_equal_except_sublement_check =
      lhs.value == rhs.value && lhs.scheme_id_uri == rhs.scheme_id_uri &&
      lhs.additional_attributes.size() == rhs.additional_attributes.size() &&
      std::equal(lhs.additional_attributes.begin(),
                 lhs.additional_attributes.end(),
                 rhs.additional_attributes.begin()) &&
      lhs.subelements.size() == rhs.subelements.size();

  if (!all_equal_except_sublement_check)
    return false;

  for (size_t i = 0; i < lhs.subelements.size(); ++i) {
    if (!ElementEqual(lhs.subelements[i], rhs.subelements[i]))
      return false;
  }
  return true;
}

MATCHER_P(ContentProtectionElementEq, expected, "") {
  return ContentProtectionElementEqual(arg, expected);
}

}  // namespace

// TODO(rkuroiwa): This is almost exactly the same as SimpleMpdNotifierTest but
// replaced all SimpleMpd with DashIopMpd,
// use typed tests
// (https://code.google.com/p/googletest/wiki/AdvancedGuide#Typed_Tests);
// also because SimpleMpdNotifier and DashIopMpdNotifier have common behavior
// for most of the public functions.
class DashIopMpdNotifierTest
    : public ::testing::TestWithParam<MpdBuilder::MpdType> {
 protected:
  DashIopMpdNotifierTest()
      : default_mock_adaptation_set_(
            new MockAdaptationSet(kDefaultAdaptationSetId)),
        default_mock_representation_(
            new MockRepresentation(kDefaultRepresentationId)) {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path_));
    output_path_ = temp_file_path_.value();
    ON_CALL(*default_mock_adaptation_set_, Group())
        .WillByDefault(Return(kDefaultGroupId));
  }

  virtual void TearDown() OVERRIDE {
    base::DeleteFile(temp_file_path_, false /* non recursive, just 1 file */);
  }

  MpdBuilder::MpdType GetMpdBuilderType(const DashIopMpdNotifier& notifier) {
    return notifier.MpdBuilderForTesting()->type();
  }

  void SetMpdBuilder(DashIopMpdNotifier* notifier,
                     scoped_ptr<MpdBuilder> mpd_builder) {
    notifier->SetMpdBuilderForTesting(mpd_builder.Pass());
  }

  MpdBuilder::MpdType mpd_type() {
    return GetParam();
  }

  DashProfile dash_profile() {
    return mpd_type() == MpdBuilder::kStatic ? kOnDemandProfile : kLiveProfile;
  }

  // Use output_path_ for specifying the MPD output path so that
  // WriteMpdToFile() doesn't crash.
  std::string output_path_;
  const MpdOptions empty_mpd_option_;
  const std::vector<std::string> empty_base_urls_;

  // Default AdaptationSet mock.
  scoped_ptr<MockAdaptationSet> default_mock_adaptation_set_;
  scoped_ptr<MockRepresentation> default_mock_representation_;

 private:
  base::FilePath temp_file_path_;
};

// Verify that it creates the correct MpdBuilder type using DashProfile passed
// to the constructor.
TEST_F(DashIopMpdNotifierTest, CreateCorrectMpdBuilderType) {
  DashIopMpdNotifier on_demand_notifier(kOnDemandProfile, empty_mpd_option_,
                                        empty_base_urls_, output_path_);
  EXPECT_TRUE(on_demand_notifier.Init());
  EXPECT_EQ(MpdBuilder::kStatic, GetMpdBuilderType(on_demand_notifier));
  DashIopMpdNotifier live_notifier(kLiveProfile, empty_mpd_option_,
                                   empty_base_urls_, output_path_);
  EXPECT_TRUE(live_notifier.Init());
  EXPECT_EQ(MpdBuilder::kDynamic, GetMpdBuilderType(live_notifier));
}

// Verify that basic VOD NotifyNewContainer() operation works.
// No encrypted contents.
TEST_P(DashIopMpdNotifierTest, NotifyNewContainer) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);

  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRole(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  // This is for the Flush() below but adding expectation here because the next
  // lines Pass() the pointer.
  EXPECT_CALL(*mock_mpd_builder, ToString(_)).WillOnce(Return(true));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.PassAs<MpdBuilder>());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &unused_container_id));
  EXPECT_TRUE(notifier.Flush());
}

// Verify VOD NotifyNewContainer() operation works with different
// MediaInfo::ProtectedContent.
// Two AdaptationSets should be created.
// Different DRM so they won't be grouped.
TEST_P(DashIopMpdNotifierTest,
       NotifyNewContainersWithDifferentProtectedContent) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type()));

  // Note they both have different (bogus) pssh, like real use case.
  // default Key ID = _default_key_id_
  const char kSdProtectedContent[] =
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

  // default Key ID = .default.key.id.
  const char kHdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'anotheruuid'\n"
      "    name_version: 'SomeOtherProtection version 3'\n"
      "    pssh: 'pssh2'\n"
      "  }\n"
      "  default_key_id: '.default.key.id.'\n"
      "}\n"
      "container_type: 1\n";

  // Check that the right ContentProtectionElements for SD is created.
  // HD is the same case, so not checking.
  ContentProtectionElement mp4_protection;
  mp4_protection.scheme_id_uri = "urn:mpeg:dash:mp4protection:2011";
  mp4_protection.value = "cenc";
  // This should match the "_default_key_id_" above, but taking it as hex data
  // and converted to UUID format.
  mp4_protection.additional_attributes["cenc:default_KID"] =
      "5f646566-6175-6c74-5f6b-65795f69645f";
  ContentProtectionElement sd_my_drm;
  sd_my_drm.scheme_id_uri = "urn:uuid:myuuid";
  sd_my_drm.value = "MyContentProtection version 1";
  Element cenc_pssh;
  cenc_pssh.name = "cenc:pssh";
  cenc_pssh.content = "cHNzaDE=";  // Base64 encoding of 'pssh1'.
  sd_my_drm.subelements.push_back(cenc_pssh);

  // Not using default mocks in this test so that we can keep track of
  // mocks by named mocks.
  const uint32_t kSdAdaptationSetId = 2u;
  const uint32_t kHdAdaptationSetId = 3u;
  scoped_ptr<MockAdaptationSet> sd_adaptation_set(
      new MockAdaptationSet(kSdAdaptationSetId));
  scoped_ptr<MockAdaptationSet> hd_adaptation_set(
      new MockAdaptationSet(kHdAdaptationSetId));

  ON_CALL(*sd_adaptation_set, Group()).WillByDefault(Return(kDefaultGroupId));
  ON_CALL(*hd_adaptation_set, Group()).WillByDefault(Return(kDefaultGroupId));
  EXPECT_CALL(*sd_adaptation_set, SetGroup(_)).Times(0);
  EXPECT_CALL(*hd_adaptation_set, SetGroup(_)).Times(0);

  const uint32_t kSdRepresentation = 4u;
  const uint32_t kHdRepresentation = 5u;
  scoped_ptr<MockRepresentation> sd_representation(
      new MockRepresentation(kSdRepresentation));
  scoped_ptr<MockRepresentation> hd_representation(
      new MockRepresentation(kHdRepresentation));

  InSequence in_sequence;
  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(sd_adaptation_set.get()));
  EXPECT_CALL(
      *sd_adaptation_set,
      AddContentProtectionElement(ContentProtectionElementEq(mp4_protection)));
  EXPECT_CALL(*sd_adaptation_set, AddContentProtectionElement(
                                      ContentProtectionElementEq(sd_my_drm)));
  EXPECT_CALL(*sd_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(sd_representation.get()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(hd_adaptation_set.get()));
  // Called twice for the same reason as above.
  EXPECT_CALL(*hd_adaptation_set, AddContentProtectionElement(_)).Times(2);

  // Add main Role here for both.
  EXPECT_CALL(*sd_adaptation_set, AddRole(AdaptationSet::kRoleMain));
  EXPECT_CALL(*hd_adaptation_set, AddRole(AdaptationSet::kRoleMain));

  EXPECT_CALL(*hd_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(hd_representation.get()));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.PassAs<MpdBuilder>());
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kSdProtectedContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kHdProtectedContent), &unused_container_id));
}

// Verify VOD NotifyNewContainer() operation works with same
// MediaInfo::ProtectedContent. Only one AdaptationSet should be
// created.
TEST_P(DashIopMpdNotifierTest, NotifyNewContainersWithSameProtectedContent) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type()));

  // These have the same default key ID and PSSH.
  const char kSdProtectedContent[] =
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
      "    pssh: 'psshbox'\n"
      "  }\n"
      "  default_key_id: '.DEFAULT.KEY.ID.'\n"
      "}\n"
      "container_type: 1\n";

  const char kHdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'psshbox'\n"
      "  }\n"
      "  default_key_id: '.DEFAULT.KEY.ID.'\n"
      "}\n"
      "container_type: 1\n";

  ContentProtectionElement mp4_protection;
  mp4_protection.scheme_id_uri = "urn:mpeg:dash:mp4protection:2011";
  mp4_protection.value = "cenc";
  // This should match the ".DEFAULT.KEY.ID." above, but taking it as hex data
  // and converted to UUID format.
  mp4_protection.additional_attributes["cenc:default_KID"] =
      "2e444546-4155-4c54-2e4b-45592e49442e";
  ContentProtectionElement my_drm;
  my_drm.scheme_id_uri = "urn:uuid:myuuid";
  my_drm.value = "MyContentProtection version 1";
  Element cenc_pssh;
  cenc_pssh.name = "cenc:pssh";
  cenc_pssh.content = "cHNzaGJveA==";  // Base64 encoding of 'psshbox'.
  my_drm.subelements.push_back(cenc_pssh);

  const uint32_t kSdRepresentation = 6u;
  const uint32_t kHdRepresentation = 7u;
  scoped_ptr<MockRepresentation> sd_representation(
      new MockRepresentation(kSdRepresentation));
  scoped_ptr<MockRepresentation> hd_representation(
      new MockRepresentation(kHdRepresentation));

  // No reason to set @group if there is only one AdaptationSet.
  EXPECT_CALL(*default_mock_adaptation_set_, SetGroup(_)).Times(0);

  InSequence in_sequence;
  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(
      *default_mock_adaptation_set_,
      AddContentProtectionElement(ContentProtectionElementEq(mp4_protection)));
  EXPECT_CALL(*default_mock_adaptation_set_,
              AddContentProtectionElement(ContentProtectionElementEq(my_drm)));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRole(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(sd_representation.get()));

  // For second representation, no new AddAdaptationSet().
  // And make sure that AddContentProtection() is not called.
  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddContentProtectionElement(_))
      .Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddRole(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(hd_representation.get()));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.PassAs<MpdBuilder>());
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kSdProtectedContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kHdProtectedContent), &unused_container_id));
}

// AddContentProtection() should not work and should always return false.
TEST_P(DashIopMpdNotifierTest, AddContentProtection) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);

  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.PassAs<MpdBuilder>());
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));

  ContentProtectionElement empty_content_protection_element;
  EXPECT_FALSE(notifier.AddContentProtectionElement(
      container_id, empty_content_protection_element));
}

// Default Key IDs are different but if the content protection UUIDs match, then
// they can be in the same group.
// This is a long test.
// Basically this
// 1. Add an SD protected content. This should make an AdaptationSet.
// 2. Add an HD protected content. This should make another AdaptationSet that
//    is different from the SD version. Both SD and HD should have the same
//    group ID assigned.
// 3. Add a 4k protected content. This should also make a new AdaptationSet.
//    The group ID should also match the SD and HD (but this takes a slightly
//    different path).
TEST_P(DashIopMpdNotifierTest, SetGroup) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type()));

  // These have the same default key ID and PSSH.
  const char kSdProtectedContent[] =
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
      "    pssh: 'pssh_sd'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "container_type: 1\n";

  const char kHdProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 1280\n"
      "  height: 720\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_hd'\n"
      "  }\n"
      "  default_key_id: '.DEFAULT.KEY.ID.'\n"
      "}\n"
      "container_type: 1\n";

  const uint32_t kSdAdaptationSetId = 6u;
  const uint32_t kHdAdaptationSetId = 7u;
  scoped_ptr<MockAdaptationSet> sd_adaptation_set(
      new MockAdaptationSet(kSdAdaptationSetId));
  scoped_ptr<MockAdaptationSet> hd_adaptation_set(
      new MockAdaptationSet(kHdAdaptationSetId));

  ON_CALL(*sd_adaptation_set, Group()).WillByDefault(Return(kDefaultGroupId));
  ON_CALL(*hd_adaptation_set, Group()).WillByDefault(Return(kDefaultGroupId));

  const uint32_t kSdRepresentation = 4u;
  const uint32_t kHdRepresentation = 5u;
  scoped_ptr<MockRepresentation> sd_representation(
      new MockRepresentation(kSdRepresentation));
  scoped_ptr<MockRepresentation> hd_representation(
      new MockRepresentation(kHdRepresentation));

  InSequence in_sequence;
  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(sd_adaptation_set.get()));
  EXPECT_CALL(*sd_adaptation_set, AddContentProtectionElement(_)).Times(2);
  EXPECT_CALL(*sd_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(sd_representation.get()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(hd_adaptation_set.get()));
  EXPECT_CALL(*hd_adaptation_set, AddContentProtectionElement(_)).Times(2);
  EXPECT_CALL(*hd_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(hd_representation.get()));

  // Both AdaptationSets' groups should be set to the same value.
  const int kExpectedGroupId = 1;
  EXPECT_CALL(*sd_adaptation_set, SetGroup(kExpectedGroupId));
  EXPECT_CALL(*hd_adaptation_set, SetGroup(kExpectedGroupId));

  // This is not very nice but we need it for settings expectations later.
  MockMpdBuilder* mock_mpd_builder_raw = mock_mpd_builder.get();
  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.PassAs<MpdBuilder>());
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kSdProtectedContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kHdProtectedContent), &unused_container_id));

  // Now that the group IDs are set Group() returns kExpectedGroupId.
  ON_CALL(*sd_adaptation_set, Group()).WillByDefault(Return(kExpectedGroupId));
  ON_CALL(*hd_adaptation_set, Group()).WillByDefault(Return(kExpectedGroupId));

  // Add another content that has the same protected content and make sure that
  // it gets added to the existing group.
  const char k4kProtectedContent[] =
      "video_info {\n"
      "  codec: 'avc1'\n"
      "  width: 4096\n"
      "  height: 2160\n"
      "  time_scale: 10\n"
      "  frame_duration: 10\n"
      "  pixel_width: 1\n"
      "  pixel_height: 1\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_4k'\n"
      "  }\n"
      "  default_key_id: 'some16bytestring'\n"
      "}\n"
      "container_type: 1\n";

  const uint32_t k4kAdaptationSetId = 4000u;
  scoped_ptr<MockAdaptationSet> fourk_adaptation_set(
      new MockAdaptationSet(k4kAdaptationSetId));
  ON_CALL(*fourk_adaptation_set, Group())
      .WillByDefault(Return(kDefaultGroupId));

  const uint32_t k4kRepresentationId = 4001u;
  scoped_ptr<MockRepresentation> fourk_representation(
      new MockRepresentation(k4kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder_raw, AddAdaptationSet(_))
      .WillOnce(Return(fourk_adaptation_set.get()));
  EXPECT_CALL(*fourk_adaptation_set, AddContentProtectionElement(_)).Times(2);
  EXPECT_CALL(*fourk_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(fourk_representation.get()));

  // Same group ID should be set.
  EXPECT_CALL(*fourk_adaptation_set, SetGroup(kExpectedGroupId));

  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(k4kProtectedContent), &unused_container_id));
}

// Even if the UUIDs match, video and audio AdaptationSets should not be grouped
// together.
TEST_P(DashIopMpdNotifierTest, DoNotSetGroupIfContentTypesDifferent) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  scoped_ptr<MockMpdBuilder> mock_mpd_builder(new MockMpdBuilder(mpd_type()));

  // These have the same default key ID and PSSH.
  const char kVideoContent[] =
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
      "    pssh: 'pssh_video'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "container_type: 1\n";

  const char kAudioContent[] =
      "audio_info {\n"
      "  codec: 'mp4a.40.2'\n"
      "  sampling_frequency: 44100\n"
      "  time_scale: 1200\n"
      "  num_channels: 2\n"
      "}\n"
      "protected_content {\n"
      "  content_protection_entry {\n"
      "    uuid: 'myuuid'\n"
      "    name_version: 'MyContentProtection version 1'\n"
      "    pssh: 'pssh_audio'\n"
      "  }\n"
      "  default_key_id: '_default_key_id_'\n"
      "}\n"
      "reference_time_scale: 50\n"
      "container_type: 1\n"
      "media_duration_seconds: 10.5\n";

  const uint32_t kVideoAdaptationSetId = 6u;
  const uint32_t kAudioAdaptationSetId = 7u;
  scoped_ptr<MockAdaptationSet> video_adaptation_set(
      new MockAdaptationSet(kVideoAdaptationSetId));
  scoped_ptr<MockAdaptationSet> audio_adaptation_set(
      new MockAdaptationSet(kAudioAdaptationSetId));

  ON_CALL(*video_adaptation_set, Group())
      .WillByDefault(Return(kDefaultGroupId));
  ON_CALL(*audio_adaptation_set, Group())
      .WillByDefault(Return(kDefaultGroupId));

  // Both AdaptationSets' groups should NOT be set.
  EXPECT_CALL(*video_adaptation_set, SetGroup(_)).Times(0);
  EXPECT_CALL(*audio_adaptation_set, SetGroup(_)).Times(0);

  const uint32_t kVideoRepresentation = 8u;
  const uint32_t kAudioRepresentation = 9u;
  scoped_ptr<MockRepresentation> video_representation(
      new MockRepresentation(kVideoRepresentation));
  scoped_ptr<MockRepresentation> audio_representation(
      new MockRepresentation(kAudioRepresentation));

  InSequence in_sequence;
  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(video_adaptation_set.get()));
  EXPECT_CALL(*video_adaptation_set, AddContentProtectionElement(_)).Times(2);
  EXPECT_CALL(*video_adaptation_set, AddRole(_)).Times(0);
  EXPECT_CALL(*video_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(video_representation.get()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(audio_adaptation_set.get()));
  EXPECT_CALL(*audio_adaptation_set, AddContentProtectionElement(_)).Times(2);
  EXPECT_CALL(*audio_adaptation_set, AddRole(_)).Times(0);
  EXPECT_CALL(*audio_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(audio_representation.get()));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, mock_mpd_builder.PassAs<MpdBuilder>());
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kVideoContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kAudioContent), &unused_container_id));
}

INSTANTIATE_TEST_CASE_P(StaticAndDynamic,
                        DashIopMpdNotifierTest,
                        ::testing::Values(MpdBuilder::kStatic,
                                          MpdBuilder::kDynamic));

}  // namespace edash_packager
