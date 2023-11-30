// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/mpd_utils.h>

#include <memory>

#include <absl/strings/escaping.h>
#include <absl/types/span.h>
#include <gtest/gtest.h>

#include <packager/mpd/base/adaptation_set.h>
#include <packager/mpd/base/mpd_options.h>
#include <packager/mpd/test/mpd_builder_test_helper.h>
#include <packager/mpd/test/xml_compare.h>

namespace shaka {
namespace {
const char kNoLanguage[] = "";
}  // namespace

class TestableAdaptationSet : public AdaptationSet {
 public:
  TestableAdaptationSet(const MpdOptions& mpd_options,
                        uint32_t* representation_counter)
      : AdaptationSet(kNoLanguage, mpd_options, representation_counter) {}
};

class MpdUtilsTest : public ::testing::Test {
 public:
  MpdUtilsTest() : adaptation_set_(mpd_options_, &representation_counter_) {}

 protected:
  MpdOptions mpd_options_;
  uint32_t representation_counter_ = 0;
  TestableAdaptationSet adaptation_set_;
};

TEST_F(MpdUtilsTest, ContentProtectionGeneral) {
  const char kMediaInfoWithContentProtection[] =
      "video_info {"
      "  codec: 'avc1'"
      "  width: 1920"
      "  height: 1080"
      "  time_scale: 3000"
      "  frame_duration: 100"
      "}"
      "protected_content {"
      "  default_key_id: '0123456789\x3A\x3B\x3C\x3D\x3E\x3F'"
      "  content_protection_entry {"
      "    uuid: 'my_uuid'"
      "    pssh: 'my_pssh'"
      "  }"
      "}"
      "container_type: 1";
  const MediaInfo media_info =
      ConvertToMediaInfo(kMediaInfoWithContentProtection);

  AddContentProtectionElements(media_info, &adaptation_set_);
  ASSERT_TRUE(adaptation_set_.AddRepresentation(media_info));

  const char kExpectedOutput[] =
      "<AdaptationSet contentType='video' width='1920'"
      "    height='1080' frameRate='3000/100'>"
      "  <ContentProtection value='cenc'"
      "      schemeIdUri='urn:mpeg:dash:mp4protection:2011'"
      "      cenc:default_KID='30313233-3435-3637-3839-3a3b3c3d3e3f'/>"
      "  <ContentProtection schemeIdUri='urn:uuid:my_uuid'>"
      "    <cenc:pssh>bXlfcHNzaA==</cenc:pssh>"
      "  </ContentProtection>"
      "  <Representation id='0' bandwidth='0' codecs='avc1'"
      "   mimeType='video/mp4'/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set_.GetXml(), XmlNodeEqual(kExpectedOutput));
}

TEST_F(MpdUtilsTest, ContentProtectionMarlin) {
  const char kMediaInfoWithContentProtection[] =
      "video_info {"
      "  codec: 'avc1'"
      "  width: 1920"
      "  height: 1080"
      "  time_scale: 3000"
      "  frame_duration: 100"
      "}"
      "protected_content {"
      "  default_key_id: '0123456789\x3A\x3B\x3C\x3D\x3E\x3F'"
      "  content_protection_entry {"
      "    uuid: '5e629af5-38da-4063-8977-97ffbd9902d4'"
      "  }"
      "}"
      "container_type: 1";
  const MediaInfo media_info =
      ConvertToMediaInfo(kMediaInfoWithContentProtection);

  AddContentProtectionElements(media_info, &adaptation_set_);
  ASSERT_TRUE(adaptation_set_.AddRepresentation(media_info));

  const char kExpectedOutput[] =
      "<AdaptationSet contentType='video' width='1920'"
      "    height='1080' frameRate='3000/100'>"
      "  <ContentProtection value='cenc'"
      "      schemeIdUri='urn:mpeg:dash:mp4protection:2011'"
      "      cenc:default_KID='30313233-3435-3637-3839-3a3b3c3d3e3f'/>"
      "  <ContentProtection"
      "      schemeIdUri='urn:uuid:5E629AF5-38DA-4063-8977-97FFBD9902D4'>"
      "    <mas:MarlinContentIds>"
      "      <mas:MarlinContentId>"
      "        urn:marlin:kid:303132333435363738393a3b3c3d3e3f"
      "      </mas:MarlinContentId>"
      "    </mas:MarlinContentIds>"
      "  </ContentProtection>"
      "  <Representation id='0' bandwidth='0' codecs='avc1'"
      "   mimeType='video/mp4'/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set_.GetXml(), XmlNodeEqual(kExpectedOutput));
}

TEST_F(MpdUtilsTest, ContentProtectionPlayReadyCencMspr) {
    const std::string pssh_str("0000003870737368010000009A04F079"
                               "98404286AB92E65BE0885F9500000001"
                               "11223344556677889900AABBCCDDEEFF"
                               "0000000430313233");

    std::string pssh = absl::HexStringToBytes(pssh_str);

    const char kMediaInfoWithContentProtection[] =
        "video_info {"
        "  codec: 'avc1'"
        "  width: 1920"
        "  height: 1080"
        "  time_scale: 3000"
        "  frame_duration: 100"
        "}"
        "protected_content {"
        "  protection_scheme: 'cenc'"
        "  default_key_id: '0123456789\x3A\x3B\x3C\x3D\x3E\x3F'"
        "  include_mspr_pro: 1"
        "}"
        "container_type: 1";

    MediaInfo media_info =
        ConvertToMediaInfo(kMediaInfoWithContentProtection);

    MediaInfo::ProtectedContent * protected_content =
        media_info.mutable_protected_content();
    MediaInfo::ProtectedContent::ContentProtectionEntry* entry =
        protected_content->add_content_protection_entry();
    entry->set_uuid("9a04f079-9840-4286-ab92-e65be0885f95");
    entry->set_pssh(pssh.data(), pssh.size());

    AddContentProtectionElements(media_info, &adaptation_set_);
    ASSERT_TRUE(adaptation_set_.AddRepresentation(media_info));

    const char kExpectedOutput[] =
        "<AdaptationSet contentType='video' width='1920'"
        "    height='1080' frameRate='3000/100'>"
        "  <ContentProtection value='cenc'"
        "      schemeIdUri='urn:mpeg:dash:mp4protection:2011'"
        "      cenc:default_KID='30313233-3435-3637-3839-3a3b3c3d3e3f'/>"
        "  <ContentProtection value='MSPR 2.0'"
        "      schemeIdUri='urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95'>"
        "    <cenc:pssh>"
        "AAAAOHBzc2gBAAAAmgTweZhAQoarkuZb4IhflQAAAAERIjNEVWZ3iJkAqrvM3e7/"
        "AAAABDAxMjM="
        "    </cenc:pssh>"
        "    <mspr:pro>MDEyMw==</mspr:pro>"
        "  </ContentProtection>"
        "  <Representation id='0' bandwidth='0' codecs='avc1' "
        "mimeType='video/mp4'/>"
        "</AdaptationSet>";

    EXPECT_THAT(adaptation_set_.GetXml(), XmlNodeEqual(kExpectedOutput));
}

TEST_F(MpdUtilsTest, ContentProtectionPlayReadyCenc) {
    const std::string pssh_str("0000003870737368010000009A04F079"
        "98404286AB92E65BE0885F9500000001"
        "11223344556677889900AABBCCDDEEFF"
        "0000000430313233");

    std::string pssh_hex_str = absl::HexStringToBytes(pssh_str);
    absl::string_view pssh_str_view(pssh_hex_str);
    absl::Span<const uint8_t> span(
        reinterpret_cast<const uint8_t*>(pssh_str_view.data()),
        pssh_str_view.size());
    std::vector<uint8_t> pssh = std::vector<uint8_t>(span.begin(), span.end());

    const char kMediaInfoWithContentProtection[] =
        "video_info {"
        "  codec: 'avc1'"
        "  width: 1920"
        "  height: 1080"
        "  time_scale: 3000"
        "  frame_duration: 100"
        "}"
        "protected_content {"
        "  protection_scheme: 'cenc'"
        "  default_key_id: '0123456789\x3A\x3B\x3C\x3D\x3E\x3F'"
        "  include_mspr_pro: 0"
        "}"
        "container_type: 1";

    MediaInfo media_info =
        ConvertToMediaInfo(kMediaInfoWithContentProtection);

    MediaInfo::ProtectedContent * protected_content =
        media_info.mutable_protected_content();
    MediaInfo::ProtectedContent::ContentProtectionEntry* entry =
        protected_content->add_content_protection_entry();
    entry->set_uuid("9a04f079-9840-4286-ab92-e65be0885f95");
    entry->set_pssh(pssh.data(), pssh.size());

    AddContentProtectionElements(media_info, &adaptation_set_);
    ASSERT_TRUE(adaptation_set_.AddRepresentation(media_info));

    const char kExpectedOutput[] =
        "<AdaptationSet contentType='video' width='1920'"
        "    height='1080' frameRate='3000/100'>"
        "  <ContentProtection value='cenc'"
        "      schemeIdUri='urn:mpeg:dash:mp4protection:2011'"
        "      cenc:default_KID='30313233-3435-3637-3839-3a3b3c3d3e3f'/>"
        "  <ContentProtection"
        "      schemeIdUri='urn:uuid:9a04f079-9840-4286-ab92-e65be0885f95'>"
        "    <cenc:pssh>"
        "AAAAOHBzc2gBAAAAmgTweZhAQoarkuZb4IhflQAAAAERIjNEVWZ3iJkAqrvM3e7/"
        "AAAABDAxMjM="
        "    </cenc:pssh>"
        "  </ContentProtection>"
        "  <Representation id='0' bandwidth='0' codecs='avc1' "
        "mimeType='video/mp4'/>"
        "</AdaptationSet>";

    EXPECT_THAT(adaptation_set_.GetXml(), XmlNodeEqual(kExpectedOutput));
}

}  // namespace shaka
