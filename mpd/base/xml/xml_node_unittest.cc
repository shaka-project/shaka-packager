// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "mpd/base/xml/xml_node.h"
#include "mpd/test/xml_compare.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/src/include/libxml/tree.h"

namespace dash_packager {
namespace xml {

namespace {

// Template so that it works for ContentProtectionXml and
// ContentProtectionXml::Element.
template <typename XmlElement>
void AddAttribute(const std::string& name,
                  const std::string& value,
                  XmlElement* content_protection_xml) {
  MediaInfo::ContentProtectionXml::AttributeNameValuePair* attribute =
      content_protection_xml->add_attributes();
  attribute->set_name(name);
  attribute->set_value(value);
}

ScopedXmlPtr<xmlDoc>::type MakeDoc(ScopedXmlPtr<xmlNode>::type node) {
  xml::ScopedXmlPtr<xmlDoc>::type doc(xmlNewDoc(BAD_CAST ""));
  xmlDocSetRootElement(doc.get(), node.release());

  return doc.Pass();
}
}  // namespace

// Make sure XmlEqual() is functioning correctly.
TEST(MetaTest, XmlEqual) {
  static const char kXml1[] =
      "<A>\n"
      "  <B\n"
      "    c=\"1\""
      "    e=\"foobar\""
      "    somelongnameattribute=\"somevalue\">\n"
      "      <Bchild childvalue=\"3\"\n"
      "              f=\"4\"/>\n"
      "  </B>\n"
      "  <C />\n"
      "</A>";


  // This is same as kXml1 but the attributes are reordered. Note that the
  // children are not reordered.
  static const char kXml1AttributeReorder[] =
      "<A>\n"
      "  <B\n"
      "    c=\"1\""
      "    somelongnameattribute=\"somevalue\"\n"
      "    e=\"foobar\">"
      "      <Bchild childvalue=\"3\"\n"
      "              f=\"4\"/>\n"
      "  </B>\n"
      "  <C />\n"
      "</A>";

  // <C> is before <B>.
  static const char kXml1ChildrenReordered[] =
      "<A>\n"
      "  <C />\n"
      "  <B\n"
      "    d=\"2\""
      "    c=\"1\""
      "    somelongnameattribute=\"somevalue\"\n"
      "    e=\"foobar\">"
      "      <Bchild childvalue=\"3\"\n"
      "              f=\"4\"/>\n"
      "  </B>\n"
      "</A>";

  // <C> is before <B>.
  static const char kXml1RemovedAttributes[] =
      "<A>\n"
      "  <B\n"
      "    d=\"2\"\n>"
      "      <Bchild f=\"4\"/>\n"
      "  </B>\n"
      "  <C />\n"
      "</A>";

  static const char kXml2[] =
      "<A>\n"
      "  <C />\n"
      "</A>";

  // In XML <C />, <C></C>, and <C/> mean the same thing.
  static const char kXml2DifferentSyntax[] =
      "<A>\n"
      "  <C></C>\n"
      "</A>";

  static const char kXml2MoreDifferentSyntax[] =
      "<A>\n"
      "  <C/>\n"
      "</A>";

  // Identity.
  ASSERT_TRUE(XmlEqual(kXml1, kXml1));

  // Equivalent.
  ASSERT_TRUE(XmlEqual(kXml1, kXml1AttributeReorder));
  ASSERT_TRUE(XmlEqual(kXml2, kXml2DifferentSyntax));
  ASSERT_TRUE(XmlEqual(kXml2, kXml2MoreDifferentSyntax));

  // Different.
  ASSERT_FALSE(XmlEqual(kXml1, kXml2));
  ASSERT_FALSE(XmlEqual(kXml1, kXml1ChildrenReordered));
  ASSERT_FALSE(XmlEqual(kXml1, kXml1RemovedAttributes));
  ASSERT_FALSE(XmlEqual(kXml1AttributeReorder, kXml1ChildrenReordered));
}

TEST(Representation, AddContentProtectionXml) {
  static const char kExpectedRepresentaionString[] =
      "<Representation>\n"
      " <ContentProtection\n"
      "   a=\"1\"\n"
      "   b=\"2\"\n"
      "   schemeIdUri=\"http://www.foo.com/drm\"\n"
      "   value=\"somevalue\">\n"
      "     <TestSubElement c=\"3\" d=\"4\"/>\n"
      " </ContentProtection>\n"
      "</Representation>";

  MediaInfo media_info;
  MediaInfo::ContentProtectionXml* content_protection_xml =
      media_info.add_content_protections();
  content_protection_xml->set_scheme_id_uri("http://www.foo.com/drm");
  content_protection_xml->set_value("somevalue");
  AddAttribute("a", "1", content_protection_xml);
  AddAttribute("b", "2", content_protection_xml);

  MediaInfo::ContentProtectionXml::Element* subelement =
      content_protection_xml->add_subelements();
  subelement->set_name("TestSubElement");
  AddAttribute("c", "3", subelement);
  AddAttribute("d", "4", subelement);

  RepresentationXmlNode representation;
  ASSERT_TRUE(
      representation.AddContentProtectionElementsFromMediaInfo(media_info));

  ScopedXmlPtr<xmlDoc>::type doc(MakeDoc(representation.PassScopedPtr()));
  ASSERT_TRUE(
      XmlEqual(kExpectedRepresentaionString, doc.get()));
}

}  // namespace xml
}  // namespace dash_packager
