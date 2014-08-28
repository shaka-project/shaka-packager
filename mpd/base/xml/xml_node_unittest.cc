// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>
#include <libxml/tree.h>

#include <list>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "mpd/base/mpd_builder.h"
#include "mpd/base/xml/xml_node.h"
#include "mpd/test/xml_compare.h"

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

std::string GetDocAsFlatString(xmlDocPtr doc) {
  static const int kFlatFormat = 0;
  int doc_str_size = 0;
  xmlChar* doc_str = NULL;
  xmlDocDumpFormatMemoryEnc(doc, &doc_str, &doc_str_size, "UTF-8", kFlatFormat);
  DCHECK(doc_str);

  std::string output(doc_str, doc_str + doc_str_size);
  xmlFree(doc_str);
  return output;
}

ScopedXmlPtr<xmlDoc>::type MakeDoc(ScopedXmlPtr<xmlNode>::type node) {
  xml::ScopedXmlPtr<xmlDoc>::type doc(xmlNewDoc(BAD_CAST ""));
  xmlDocSetRootElement(doc.get(), node.release());
  return doc.Pass();
}

}  // namespace

class RepresentationTest : public ::testing::Test {
 public:
  RepresentationTest() {}
  virtual ~RepresentationTest() {}

  // Ownership transfers, IOW this function will release the resource for
  // |node|. Returns |node| in string format.
  // You should not call this function multiple times.
  std::string GetStringFormat() {
    xml::ScopedXmlPtr<xmlDoc>::type doc(xmlNewDoc(BAD_CAST ""));

    // Because you cannot easily get the string format of a xmlNodePtr, it gets
    // attached to a temporary xml doc.
    xmlDocSetRootElement(doc.get(), representation_.Release());
    std::string doc_str = GetDocAsFlatString(doc.get());

    // GetDocAsFlatString() adds
    // <?xml version="" encoding="UTF-8"?>
    // to the first line. So this removes the first line.
    const size_t first_newline_char_pos = doc_str.find('\n');
    DCHECK_NE(first_newline_char_pos, std::string::npos);
    return doc_str.substr(first_newline_char_pos + 1);
  }

 protected:
  RepresentationXmlNode representation_;
  std::list<SegmentInfo> segment_infos_;
};

// Make sure XmlEqual() is functioning correctly.
// TODO(rkuroiwa): Move this to a separate file. This requires it to be TEST_F
// due to gtest /test
TEST_F(RepresentationTest, MetaTest_XmlEqual) {
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

TEST_F(RepresentationTest, AddContentProtectionXml) {
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

  ASSERT_TRUE(
      representation_.AddContentProtectionElementsFromMediaInfo(media_info));
  ScopedXmlPtr<xmlDoc>::type doc(MakeDoc(representation_.PassScopedPtr()));
  ASSERT_TRUE(
      XmlEqual(kExpectedRepresentaionString, doc.get()));
}

// Some template names cannot be used for init segment name.
TEST_F(RepresentationTest, InvalidLiveInitSegmentName) {
  MediaInfo media_info;
  const uint32 kDefaultStartNumber = 1;

  // $Number$ cannot be used for segment name.
  media_info.set_init_segment_name("$Number$.mp4");
  ASSERT_FALSE(representation_.AddLiveOnlyInfo(
      media_info, segment_infos_, kDefaultStartNumber));

  // $Time$ as well.
  media_info.set_init_segment_name("$Time$.mp4");
  ASSERT_FALSE(representation_.AddLiveOnlyInfo(
      media_info, segment_infos_, kDefaultStartNumber));

  // This should be valid.
  media_info.set_init_segment_name("some_non_template_name.mp4");
  ASSERT_TRUE(representation_.AddLiveOnlyInfo(
      media_info, segment_infos_, kDefaultStartNumber));
}

}  // namespace xml
}  // namespace dash_packager
