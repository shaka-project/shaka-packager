// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>
#include <libxml/tree.h>

#include <list>

#include "packager/base/logging.h"
#include "packager/base/strings/string_util.h"
#include "packager/mpd/base/mpd_builder.h"
#include "packager/mpd/base/xml/xml_node.h"
#include "packager/mpd/test/xml_compare.h"

namespace edash_packager {
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
  ~RepresentationTest() override {}

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
TEST_F(RepresentationTest, MetaTestXmlElementsEqual) {
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

// Verify that if contents are different, XmlEqual returns false.
// This is to catch the case where just using xmlNodeGetContent() on elements
// that have subelements don't quite work well.
// xmlNodeGetContent(<A>) (for both <A>s) will return "content1content2".
// But if it is run on <B> for the first XML, it will return "content1", but
// for second XML will return "c".
TEST_F(RepresentationTest, MetaTestXmlEqualDifferentContent) {
  ASSERT_FALSE(XmlEqual(
      "<A><B>content1</B><B>content2</B></A>",
      "<A><B>c</B><B>ontent1content2</B></A>"));
}

// Verify that AddContentProtectionElements work.
// xmlReadMemory() (used in XmlEqual()) doesn't like XML fragments that have
// namespaces without context, e.g. <cenc:pssh> element.
// The MpdBuilderTests work because the MPD element has xmlns:cenc attribute.
// Tests that have <cenc:pssh> is in mpd_builder_unittest.
TEST_F(RepresentationTest, AddContentProtectionElements) {
  std::list<ContentProtectionElement> content_protections;
  ContentProtectionElement content_protection_widevine;
  content_protection_widevine.scheme_id_uri =
      "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
  content_protection_widevine.value = "SOME bogus Widevine DRM version";
  Element any_element;
  any_element.name = "AnyElement";
  any_element.content = "any content";
  content_protection_widevine.subelements.push_back(any_element);
  content_protections.push_back(content_protection_widevine);

  ContentProtectionElement content_protection_clearkey;
  content_protection_clearkey.scheme_id_uri =
      "urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b";
  content_protections.push_back(content_protection_clearkey);

  representation_.AddContentProtectionElements(content_protections);
  ScopedXmlPtr<xmlDoc>::type doc(MakeDoc(representation_.PassScopedPtr()));
  ASSERT_TRUE(XmlEqual(
      "<Representation>\n"
      " <ContentProtection\n"
      "   schemeIdUri=\"urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\"\n"
      "   value=\"SOME bogus Widevine DRM version\">\n"
      "     <AnyElement>any content</AnyElement>\n"
      " </ContentProtection>\n"
      " <ContentProtection\n"
      "   schemeIdUri=\"urn:uuid:1077efec-c0b2-4d02-ace3-3c1e52e2fb4b\">"
      " </ContentProtection>\n"
      "</Representation>",
      doc.get()));
}

// Some template names cannot be used for init segment name.
TEST_F(RepresentationTest, InvalidLiveInitSegmentName) {
  MediaInfo media_info;
  const uint32_t kDefaultStartNumber = 1;

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
}  // namespace edash_packager
