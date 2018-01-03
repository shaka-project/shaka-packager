#ifndef MPD_TEST_XML_COMPARE_H_
#define MPD_TEST_XML_COMPARE_H_

#include <gmock/gmock.h>
#include <libxml/tree.h>

#include <string>

#include "packager/mpd/base/xml/scoped_xml_ptr.h"

namespace shaka {

/// Checks whether the XMLs are equivalent. An XML schema can specify strict
/// ordering of children and MPD does. These functions are currently tuned to
/// compare subsets of MPD. Therefore this function requires strict ordering of
/// the child elements of an element. Attributes can be in any order since XML
/// cannot enforce ordering of attributes.
/// @param xml1 is compared against @a xml2.
/// @param xml2 is compared against @a xml1.
/// @return true if @a xml1 and @a xml2 are equivalent, false otherwise.
bool XmlEqual(const std::string& xml1, const std::string& xml2);
bool XmlEqual(const std::string& xml1, xmlDocPtr xml2);
bool XmlEqual(xmlDocPtr xml1, xmlDocPtr xml2);
bool XmlEqual(const std::string& xml1, xmlNodePtr xml2);

/// Get string representation of the xml node.
/// Note that the ownership is not transferred.
std::string XmlNodeToString(xmlNodePtr xml_node);

/// Match an xmlNodePtr with an xml in string representation.
MATCHER_P(XmlNodeEqual,
          xml,
          std::string("xml node equal (ignore extra white spaces)\n") + xml) {
  *result_listener << "\n" << XmlNodeToString(arg);
  return XmlEqual(xml, arg);
}

/// Match the attribute of an xmlNodePtr with expected value.
/// Note that the ownership is not transferred.
MATCHER_P2(AttributeEqual, attribute, expected_value, "") {
  xml::scoped_xml_ptr<xmlChar> attribute_xml_str(
      xmlGetProp(arg, BAD_CAST attribute));
  if (!attribute_xml_str) {
    *result_listener << "no attribute '" << attribute << "'";
    return false;
  }
  std::string actual_value =
      reinterpret_cast<const char*>(attribute_xml_str.get());
  *result_listener << actual_value;
  return expected_value == actual_value;
}

/// Check if the attribute is set in an xmlNodePtr.
/// Note that the ownership is not transferred.
MATCHER_P(AttributeSet, attribute, "") {
  xml::scoped_xml_ptr<xmlChar> attribute_xml_str(
      xmlGetProp(arg, BAD_CAST attribute));
  return attribute_xml_str != nullptr;
}
}  // namespace shaka

#endif  // MPD_TEST_XML_COMPARE_H_
