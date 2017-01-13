// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/mpd/base/dash_iop_mpd_notifier.h"
#include "packager/mpd/base/mock_mpd_builder.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"

namespace shaka {

using ::testing::_;
using ::testing::Eq;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

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

bool ElementEqual(const Element& lhs, const Element& rhs) {
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

bool ContentProtectionElementEqual(const ContentProtectionElement& lhs,
                                   const ContentProtectionElement& rhs) {
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

  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_path_));
    output_path_ = temp_file_path_.AsUTF8Unsafe();
  }

  void TearDown() override {
    base::DeleteFile(temp_file_path_, false /* non recursive, just 1 file */);
  }

  MpdBuilder::MpdType GetMpdBuilderType(const DashIopMpdNotifier& notifier) {
    return notifier.MpdBuilderForTesting()->type();
  }

  void SetMpdBuilder(DashIopMpdNotifier* notifier,
                     std::unique_ptr<MpdBuilder> mpd_builder) {
    notifier->SetMpdBuilderForTesting(std::move(mpd_builder));
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

  // Default mocks that can be used for the tests.
  // IOW, if a test only requires one instance of
  // Mock{AdaptationSet,Representation}, these can be used.
  std::unique_ptr<MockAdaptationSet> default_mock_adaptation_set_;
  std::unique_ptr<MockRepresentation> default_mock_representation_;

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

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRole(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  // This is for the Flush() below but adding expectation here because the next
  // std::move(lines) the pointer.
  EXPECT_CALL(*mock_mpd_builder, ToString(_)).WillOnce(Return(true));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &unused_container_id));
  EXPECT_TRUE(notifier.Flush());
}

// Verify that if the MediaInfo contains text information, then
// MpdBuilder::ForceSetSegmentAlignment() is called.
TEST_P(DashIopMpdNotifierTest, NotifyNewTextContainer) {
  const char kTextMediaInfo[] =
      "text_info {\n"
      "  format: 'ttml'\n"
      "  language: 'en'\n"
      "}\n"
      "container_type: CONTAINER_TEXT\n";
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(StrEq("en")))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRole(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, ForceSetSegmentAlignment(true));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  // This is for the Flush() below but adding expectation here because the next
  // std::move(lines) the pointer.
  EXPECT_CALL(*mock_mpd_builder, ToString(_)).WillOnce(Return(true));

  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kTextMediaInfo),
                                          &unused_container_id));
  EXPECT_TRUE(notifier.Flush());
}

// Verify VOD NotifyNewContainer() operation works with different
// MediaInfo::ProtectedContent.
// Two AdaptationSets should be created.
// AdaptationSets with different DRM won't be switchable.
TEST_P(DashIopMpdNotifierTest,
       NotifyNewContainersWithDifferentProtectedContent) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

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
  std::unique_ptr<MockAdaptationSet> sd_adaptation_set(
      new MockAdaptationSet(kSdAdaptationSetId));
  std::unique_ptr<MockAdaptationSet> hd_adaptation_set(
      new MockAdaptationSet(kHdAdaptationSetId));

  const uint32_t kSdRepresentation = 4u;
  const uint32_t kHdRepresentation = 5u;
  std::unique_ptr<MockRepresentation> sd_representation(
      new MockRepresentation(kSdRepresentation));
  std::unique_ptr<MockRepresentation> hd_representation(
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
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kSdProtectedContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kHdProtectedContent), &unused_container_id));

  EXPECT_THAT(sd_adaptation_set->adaptation_set_switching_ids(), ElementsAre());
  EXPECT_THAT(hd_adaptation_set->adaptation_set_switching_ids(), ElementsAre());
}

// Verify VOD NotifyNewContainer() operation works with same
// MediaInfo::ProtectedContent. Only one AdaptationSet should be
// created.
TEST_P(DashIopMpdNotifierTest, NotifyNewContainersWithSameProtectedContent) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

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
  std::unique_ptr<MockRepresentation> sd_representation(
      new MockRepresentation(kSdRepresentation));
  std::unique_ptr<MockRepresentation> hd_representation(
      new MockRepresentation(kHdRepresentation));

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
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kSdProtectedContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kHdProtectedContent), &unused_container_id));

  // No adaptation set switching if there is only one AdaptationSet.
  EXPECT_THAT(default_mock_adaptation_set_->adaptation_set_switching_ids(),
              ElementsAre());
}

// AddContentProtection() should not work and should always return false.
TEST_P(DashIopMpdNotifierTest, AddContentProtection) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));

  ContentProtectionElement empty_content_protection_element;
  EXPECT_FALSE(notifier.AddContentProtectionElement(
      container_id, empty_content_protection_element));
}

// Default Key IDs are different but if the content protection UUIDs match, then
// the AdaptationSet they belong to should be switchable.
// This is a long test.
// Basically this
// 1. Add an SD protected content. This should make an AdaptationSet.
// 2. Add an HD protected content. This should make another AdaptationSet that
//    is different from the SD version. SD AdaptationSet and HD AdaptationSet
//    should be switchable.
// 3. Add a 4k protected content. This should also make a new AdaptationSet.
//    It should be switchable with SD/HD AdaptationSet.
TEST_P(DashIopMpdNotifierTest, SetAdaptationSetSwitching) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

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
  std::unique_ptr<MockAdaptationSet> sd_adaptation_set(
      new MockAdaptationSet(kSdAdaptationSetId));
  std::unique_ptr<MockAdaptationSet> hd_adaptation_set(
      new MockAdaptationSet(kHdAdaptationSetId));

  const uint32_t kSdRepresentation = 4u;
  const uint32_t kHdRepresentation = 5u;
  std::unique_ptr<MockRepresentation> sd_representation(
      new MockRepresentation(kSdRepresentation));
  std::unique_ptr<MockRepresentation> hd_representation(
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

  // This is not very nice but we need it for settings expectations later.
  MockMpdBuilder* mock_mpd_builder_raw = mock_mpd_builder.get();
  uint32_t unused_container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kSdProtectedContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kHdProtectedContent), &unused_container_id));

  EXPECT_THAT(sd_adaptation_set->adaptation_set_switching_ids(),
              ElementsAre(kHdAdaptationSetId));
  EXPECT_THAT(hd_adaptation_set->adaptation_set_switching_ids(),
              ElementsAre(kSdAdaptationSetId));

  // Add another content that has the same protected content and make sure that
  // adaptation set switching is set correctly.
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
  std::unique_ptr<MockAdaptationSet> fourk_adaptation_set(
      new MockAdaptationSet(k4kAdaptationSetId));

  const uint32_t k4kRepresentationId = 4001u;
  std::unique_ptr<MockRepresentation> fourk_representation(
      new MockRepresentation(k4kRepresentationId));

  EXPECT_CALL(*mock_mpd_builder_raw, AddAdaptationSet(_))
      .WillOnce(Return(fourk_adaptation_set.get()));
  EXPECT_CALL(*fourk_adaptation_set, AddContentProtectionElement(_)).Times(2);
  EXPECT_CALL(*fourk_adaptation_set, AddRepresentation(_))
      .WillOnce(Return(fourk_representation.get()));

  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(k4kProtectedContent), &unused_container_id));

  EXPECT_THAT(sd_adaptation_set->adaptation_set_switching_ids(),
              UnorderedElementsAre(kHdAdaptationSetId, k4kAdaptationSetId));
  EXPECT_THAT(hd_adaptation_set->adaptation_set_switching_ids(),
              UnorderedElementsAre(kSdAdaptationSetId, k4kAdaptationSetId));
  EXPECT_THAT(fourk_adaptation_set->adaptation_set_switching_ids(),
              ElementsAre(kSdAdaptationSetId, kHdAdaptationSetId));
}

// Even if the UUIDs match, video and audio AdaptationSets should not be
// switchable.
TEST_P(DashIopMpdNotifierTest,
       DoNotSetAdaptationSetSwitchingIfContentTypesDifferent) {
  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

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
  std::unique_ptr<MockAdaptationSet> video_adaptation_set(
      new MockAdaptationSet(kVideoAdaptationSetId));
  std::unique_ptr<MockAdaptationSet> audio_adaptation_set(
      new MockAdaptationSet(kAudioAdaptationSetId));

  const uint32_t kVideoRepresentation = 8u;
  const uint32_t kAudioRepresentation = 9u;
  std::unique_ptr<MockRepresentation> video_representation(
      new MockRepresentation(kVideoRepresentation));
  std::unique_ptr<MockRepresentation> audio_representation(
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
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kVideoContent), &unused_container_id));
  EXPECT_TRUE(notifier.NotifyNewContainer(
      ConvertToMediaInfo(kAudioContent), &unused_container_id));

  EXPECT_THAT(video_adaptation_set->adaptation_set_switching_ids(),
              ElementsAre());
  EXPECT_THAT(audio_adaptation_set->adaptation_set_switching_ids(),
              ElementsAre());
}

TEST_P(DashIopMpdNotifierTest, UpdateEncryption) {
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

  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);

  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

  EXPECT_CALL(*mock_mpd_builder, AddAdaptationSet(_))
      .WillOnce(Return(default_mock_adaptation_set_.get()));
  EXPECT_CALL(*default_mock_adaptation_set_, AddRole(_)).Times(0);
  EXPECT_CALL(*default_mock_adaptation_set_, AddRepresentation(_))
      .WillOnce(Return(default_mock_representation_.get()));

  uint32_t container_id;
  SetMpdBuilder(&notifier, std::move(mock_mpd_builder));
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kProtectedContent),
                                          &container_id));

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
  DashIopMpdNotifier notifier(kOnDemandProfile, empty_mpd_option_,
                             empty_base_urls_, output_path_);
  uint32_t container_id;
  EXPECT_TRUE(notifier.NotifyNewContainer(ConvertToMediaInfo(kValidMediaInfo),
                                          &container_id));
  const uint32_t kAnySampleDuration = 1000;
  EXPECT_TRUE(notifier.NotifySampleDuration(container_id,  kAnySampleDuration));
  EXPECT_TRUE(notifier.Flush());
}

// Don't put different audio languages or codecs in the same AdaptationSet.
TEST_P(DashIopMpdNotifierTest, SplitAdaptationSetsByLanguageAndCodec) {
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

  DashIopMpdNotifier notifier(dash_profile(), empty_mpd_option_,
                              empty_base_urls_, output_path_);
  std::unique_ptr<MockMpdBuilder> mock_mpd_builder(
      new MockMpdBuilder(mpd_type()));

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
                        DashIopMpdNotifierTest,
                        ::testing::Values(MpdBuilder::kStatic,
                                          MpdBuilder::kDynamic));

}  // namespace shaka
