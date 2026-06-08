// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/base/preselection.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <packager/mpd/base/media_info.pb.h>
#include <packager/mpd/test/xml_compare.h>

namespace shaka {

namespace {

const char kTestLanguage[] = "en";
const char kTestGroupId[] = "1000";
const char kTestPreselectionTag[] = "1";
const uint32_t kTestAdaptationId = 1;
const uint32_t kTestSelectionPriority = 3;

const char kAc4DialogGainScheme[] =
    "tag:dolby.com,2018:dash:audio_dialog_gain:2025";
const char kEnhancedIntelligibilityScheme[] = "urn:mpeg:dash:role:2011";
const char kEnhancedIntelligibilityValue[] = "enhanced-audio-intelligibility";

}  // namespace

using ::testing::HasSubstr;

TEST(PreselectionTest, BasicPreselectionCreation) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);

  EXPECT_EQ(kTestGroupId, preselection.id());
  EXPECT_EQ(kTestAdaptationId, preselection.preselection_components());
  EXPECT_EQ(kTestLanguage, preselection.lang());
  EXPECT_EQ(kTestPreselectionTag, preselection.tag());
  EXPECT_EQ(kTestSelectionPriority, preselection.selection_priority());
}

TEST(PreselectionTest, AddLabel) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);

  preselection.AddLabel("en", "Dialog +4dB");
  preselection.AddLabel("de", "Dialog +4dB");

  EXPECT_EQ(2u, preselection.labels().size());
  EXPECT_EQ("en", preselection.labels()[0].lang);
  EXPECT_EQ("Dialog +4dB", preselection.labels()[0].value);
  EXPECT_EQ("de", preselection.labels()[1].lang);
  EXPECT_EQ("Dialog +4dB", preselection.labels()[1].value);
}

TEST(PreselectionTest, AddRole) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);

  preselection.AddRole("urn:mpeg:dash:role:2011", "main");
  preselection.AddRole("urn:mpeg:dash:role:2011", "alternate");

  EXPECT_EQ(2u, preselection.roles().size());
  EXPECT_EQ("urn:mpeg:dash:role:2011", preselection.roles()[0].scheme_id_uri);
  EXPECT_EQ("main", preselection.roles()[0].value);
  EXPECT_EQ("urn:mpeg:dash:role:2011", preselection.roles()[1].scheme_id_uri);
  EXPECT_EQ("alternate", preselection.roles()[1].value);
}

TEST(PreselectionTest, GetXmlBasic) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "0.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlWithLabels) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddLabel("en", "Dialog +4dB");
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "0.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "  <Label lang=\"en\" value=\"Dialog +4dB\"/>"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlWithRoles) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddRole("urn:mpeg:dash:role:2011", "main");
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "0.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"main\"/>"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlWithDialogGainZero) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "0.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  // No Accessibility when gain is 0.
  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlWithDialogGainPositive) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddAccessibility(kEnhancedIntelligibilityScheme,
                                kEnhancedIntelligibilityValue);
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "4.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"4.0\"/>"
      "  <Accessibility schemeIdUri=\"urn:mpeg:dash:role:2011\" "
      "value=\"enhanced-audio-intelligibility\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlComplete) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddLabel("en", "Dialog +4dB");
  preselection.AddRole("urn:mpeg:dash:role:2011", "main");
  preselection.AddAccessibility(kEnhancedIntelligibilityScheme,
                                kEnhancedIntelligibilityValue);
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "4.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"4.0\"/>"
      "  <Label lang=\"en\" value=\"Dialog +4dB\"/>"
      "  <Accessibility schemeIdUri=\"urn:mpeg:dash:role:2011\" "
      "value=\"enhanced-audio-intelligibility\"/>"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"main\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlWithMultipleLabelsAndRoles) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddLabel("en", "Dialog +4dB");
  preselection.AddLabel("de", "Dialog +4dB");
  preselection.AddRole("urn:mpeg:dash:role:2011", "main");
  preselection.AddRole("urn:mpeg:dash:role:2011", "alternate");
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "0.0");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\">"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "  <Label lang=\"en\" value=\"Dialog +4dB\"/>"
      "  <Label lang=\"de\" value=\"Dialog +4dB\"/>"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"main\"/>"
      "  <Role schemeIdUri=\"urn:mpeg:dash:role:2011\" value=\"alternate\"/>"
      "  <SupplementalProperty "
      "schemeIdUri=\"tag:dolby.com,2018:dash:audio_dialog_gain:2025\" "
      "value=\"0.0\"/>"
      "</Preselection>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, GetXmlNoSupplementalProperties) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  const char kExpectedXml[] =
      "<Preselection id=\"1000\" preselectionComponents=\"1\" lang=\"en\" "
      "tag=\"1\" selectionPriority=\"3\"/>";

  EXPECT_THAT(*xml_node, XmlNodeEqual(kExpectedXml));
}

TEST(PreselectionTest, DialogGainFormattedWithOneDecimal) {
  Preselection preselection(kTestGroupId, kTestAdaptationId, kTestLanguage,
                            kTestPreselectionTag, kTestSelectionPriority);
  preselection.AddSupplementalProperty(kAc4DialogGainScheme, "8.5");
  auto xml_node = preselection.GetXml();
  ASSERT_NE(nullptr, xml_node);

  std::string xml_str = xml_node->ToString("");
  EXPECT_THAT(xml_str, HasSubstr("value=\"8.5\""));
}

TEST(PreselectionTest, CreateFromAc4PreselectionBasic) {
  MediaInfo::Ac4Preselection ac4_preselection;
  ac4_preselection.set_group_id("1001");
  ac4_preselection.set_lang("en");
  ac4_preselection.set_preselection_tag("2");
  ac4_preselection.set_selection_priority(2);
  ac4_preselection.set_dialog_gain(2.0f);  // 2 / 2 = 1.0

  auto preselection = Preselection::CreateFromAc4Preselection(
      ac4_preselection, kTestAdaptationId);

  ASSERT_NE(nullptr, preselection);
  EXPECT_EQ("1001", preselection->id());
  EXPECT_EQ(kTestAdaptationId, preselection->preselection_components());
  EXPECT_EQ("en", preselection->lang());
  EXPECT_EQ("2", preselection->tag());
  EXPECT_EQ(2u, preselection->selection_priority());
  auto xml_node = preselection->GetXml();
  ASSERT_NE(nullptr, xml_node);
  std::string xml_str = xml_node->ToString("");
  EXPECT_THAT(xml_str, HasSubstr("value=\"1.0\""));
}

TEST(PreselectionTest, CreateFromAc4PreselectionWithLabels) {
  MediaInfo::Ac4Preselection ac4_preselection;
  ac4_preselection.set_group_id("1002");
  ac4_preselection.set_lang("en");
  ac4_preselection.set_preselection_tag("3");
  ac4_preselection.set_selection_priority(1);
  auto* label1 = ac4_preselection.add_labels();
  label1->set_lang("en");
  label1->set_value("Dialog +8dB");

  auto* label2 = ac4_preselection.add_labels();
  label2->set_lang("de");
  label2->set_value("Dialog +8dB");

  auto preselection = Preselection::CreateFromAc4Preselection(
      ac4_preselection, kTestAdaptationId);

  ASSERT_NE(nullptr, preselection);
  EXPECT_EQ(2u, preselection->labels().size());
}

TEST(PreselectionTest, CreateFromAc4PreselectionWithRoles) {
  MediaInfo::Ac4Preselection ac4_preselection;
  ac4_preselection.set_group_id("1003");
  ac4_preselection.set_lang("en");
  ac4_preselection.set_preselection_tag("4");
  ac4_preselection.set_selection_priority(4);

  auto* role1 = ac4_preselection.add_roles();
  role1->set_scheme("urn:mpeg:dash:role:2011");
  role1->set_value("main");

  auto* role2 = ac4_preselection.add_roles();
  role2->set_scheme("urn:mpeg:dash:role:2011");
  role2->set_value("alternate");

  auto preselection = Preselection::CreateFromAc4Preselection(
      ac4_preselection, kTestAdaptationId);

  ASSERT_NE(nullptr, preselection);
  EXPECT_EQ(2u, preselection->roles().size());
  EXPECT_EQ("urn:mpeg:dash:role:2011", preselection->roles()[0].scheme_id_uri);
  EXPECT_EQ("main", preselection->roles()[0].value);
}

TEST(PreselectionTest, CreateFromAc4PreselectionWithDialogGain) {
  MediaInfo::Ac4Preselection ac4_preselection;
  ac4_preselection.set_group_id("1004");
  ac4_preselection.set_lang("en");
  ac4_preselection.set_preselection_tag("5");
  ac4_preselection.set_selection_priority(1);
  ac4_preselection.set_dialog_gain(8.0f);  // 8.0 / 2 = 4.0

  auto preselection = Preselection::CreateFromAc4Preselection(
      ac4_preselection, kTestAdaptationId);

  ASSERT_NE(nullptr, preselection);

  auto xml_node = preselection->GetXml();
  ASSERT_NE(nullptr, xml_node);

  std::string xml_str = xml_node->ToString("");
  EXPECT_THAT(xml_str, HasSubstr("value=\"4.0\""));
}

}  // namespace shaka
