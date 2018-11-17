// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/mpd/base/mpd_utils.h"

#include <gtest/gtest.h>
#include <memory>

#include "packager/mpd/base/adaptation_set.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/mpd/test/mpd_builder_test_helper.h"
#include "packager/mpd/test/xml_compare.h"

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
  EXPECT_THAT(adaptation_set_.GetXml().get(), XmlNodeEqual(kExpectedOutput));
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
      "      schemeIdUri='urn:uuid:5e629af5-38da-4063-8977-97ffbd9902d4'>"
      "    <mas:MarlinContentIds>"
      "      <mas:MarlinContentId>"
      "        urn:marlin:kid:303132333435363738393a3b3c3d3e3f"
      "      </mas:MarlinContentId>"
      "    </mas:MarlinContentIds>"
      "  </ContentProtection>"
      "  <Representation id='0' bandwidth='0' codecs='avc1'"
      "   mimeType='video/mp4'/>"
      "</AdaptationSet>";
  EXPECT_THAT(adaptation_set_.GetXml().get(), XmlNodeEqual(kExpectedOutput));
}

}  // namespace shaka
