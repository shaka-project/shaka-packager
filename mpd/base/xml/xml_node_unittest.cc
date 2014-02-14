// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "mpd/base/xml/xml_node.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libxml/src/include/libxml/tree.h"

namespace dash_packager {
namespace xml {

namespace {

// TODO(rkuroiwa): Add XmlStringCompare() that does not care about the
// prettiness of the string representation of the XML. We currently use
// CollapseWhitespaceASCII() with carefully handcrafted expectations so that we
// can compare the result.

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

// Ownership transfers, IOW this function will release the resource for |node|.
// Returns |node| in string format.
std::string GetStringFormat(ScopedXmlPtr<xmlNode>::type node) {
  xml::ScopedXmlPtr<xmlDoc>::type doc(xmlNewDoc(BAD_CAST ""));

  // Because you cannot easily get the string format of a xmlNodePtr, it gets
  // attached to a temporary xml doc.
  xmlDocSetRootElement(doc.get(), node.release());
  std::string doc_str = GetDocAsFlatString(doc.get());

  // GetDocAsFlatString() adds
  // <?xml version="" encoding="UTF-8"?>
  // to the first line. So this removes the first line.
  const size_t first_newline_char_pos = doc_str.find('\n');
  DCHECK_NE(first_newline_char_pos, std::string::npos);
  return doc_str.substr(first_newline_char_pos + 1);
}

}  // namespace

TEST(Representation, AddContentProtectionXml) {
  static const char kExpectedRepresentaionString[] =
      "<Representation>\n\
        <ContentProtection \
          a=\"1\" \
          b=\"2\" \
          schemeIdUri=\"http://www.foo.com/drm\" \
          value=\"somevalue\">\n\
            <TestSubElement c=\"3\" d=\"4\"/>\n\
        </ContentProtection>\n\
      </Representation>";

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

  std::string representation_xml_string =
      GetStringFormat(representation.PassScopedPtr());
  // Compare with expected output (both flattened).
  ASSERT_EQ(CollapseWhitespaceASCII(kExpectedRepresentaionString, true),
            CollapseWhitespaceASCII(representation_xml_string, true));
}

}  // namespace xml
}  // namespace dash_packager
