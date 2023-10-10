// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MPD_TEST_XML_COMPARE_H_
#define MPD_TEST_XML_COMPARE_H_

#include <optional>
#include <string>

#include <gmock/gmock.h>
#include <libxml/tree.h>

#include <packager/mpd/base/xml/scoped_xml_ptr.h>
#include <packager/mpd/base/xml/xml_node.h>

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
bool XmlEqual(const std::string& xml1, const xml::XmlNode& xml2);
bool XmlEqual(const std::string& xml1, const std::optional<xml::XmlNode>& xml2);

/// Get string representation of the xml node.
std::string XmlNodeToString(const xml::XmlNode& xml_node);
std::string XmlNodeToString(const std::optional<xml::XmlNode>& xml_node);

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
  if (!arg) {
    *result_listener << "returned error";
    return false;
  }

  std::string actual_value;
  if (!arg->GetAttribute(attribute, &actual_value)) {
    *result_listener << "no attribute '" << attribute << "'";
    return false;
  }
  *result_listener << actual_value;
  return expected_value == actual_value;
}

/// Check if the attribute is set in an xmlNodePtr.
/// Note that the ownership is not transferred.
MATCHER_P(AttributeSet, attribute, "") {
  if (!arg) {
    *result_listener << "returned error";
    return false;
  }
  std::string unused;
  return arg->GetAttribute(attribute, &unused);
}
}  // namespace shaka

#endif  // MPD_TEST_XML_COMPARE_H_
