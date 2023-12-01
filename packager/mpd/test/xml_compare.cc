// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/mpd/test/xml_compare.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <packager/macros/logging.h>

namespace shaka {

namespace {
xml::scoped_xml_ptr<xmlDoc> GetDocFromString(const std::string& xml_str) {
  return xml::scoped_xml_ptr<xmlDoc>(
      xmlReadMemory(xml_str.data(), xml_str.size(), NULL, NULL, 0));
}

// Make a map from attributes of the node.
std::map<std::string, std::string> GetMapOfAttributes(xmlNodePtr node) {
  DVLOG(2) << "Getting attributes for node "
           << reinterpret_cast<const char*>(node->name);
  std::map<std::string, std::string> attribute_map;
  for (xmlAttr* attribute = node->properties;
       attribute && attribute->name && attribute->children;
       attribute = attribute->next) {
    const char* name = reinterpret_cast<const char*>(attribute->name);
    xml::scoped_xml_ptr<xmlChar> value(
        xmlNodeListGetString(node->doc, attribute->children, 1));

    attribute_map[name] = reinterpret_cast<const char*>(value.get());
    DVLOG(3) << "In node " << reinterpret_cast<const char*>(node->name)
             << " found attribute " << name << " with value "
             << reinterpret_cast<const char*>(value.get());
  }

  return attribute_map;
}

bool MapCompareFunc(std::pair<std::string, std::string> a,
                    std::pair<std::string, std::string> b) {
  if (a.first != b.first) {
    DLOG(INFO) << "Attribute mismatch " << a.first << " and " << b.first;
    return false;
  }

  if (a.second != b.second) {
    DLOG(INFO) << "Value mismatch for " << a.first << std::endl << "Values are "
               << a.second << " and " << b.second;
    return false;
  }
  return true;
}

bool MapsEqual(const std::map<std::string, std::string>& map1,
               const std::map<std::string, std::string>& map2) {
  return map1.size() == map2.size() &&
         std::equal(map1.begin(), map1.end(), map2.begin(), MapCompareFunc);
}

// Return true if the nodes have the same attributes.
bool CompareAttributes(xmlNodePtr node1, xmlNodePtr node2) {
  return MapsEqual(GetMapOfAttributes(node1), GetMapOfAttributes(node2));
}

// Return true if the name of the nodes match.
bool CompareNames(xmlNodePtr node1, xmlNodePtr node2) {
  DVLOG(2) << "Comparing " << reinterpret_cast<const char*>(node1->name)
           << " and " << reinterpret_cast<const char*>(node2->name);
  return xmlStrcmp(node1->name, node2->name) == 0;
}

bool CompareContents(xmlNodePtr node1, xmlNodePtr node2) {
  xml::scoped_xml_ptr<xmlChar> node1_content_ptr(xmlNodeGetContent(node1));
  xml::scoped_xml_ptr<xmlChar> node2_content_ptr(xmlNodeGetContent(node2));
  std::string node1_content =
      reinterpret_cast<const char*>(node1_content_ptr.get());
  std::string node2_content =
      reinterpret_cast<const char*>(node2_content_ptr.get());

  node1_content.erase(
      std::remove(node1_content.begin(), node1_content.end(), '\n'),
      node1_content.end());
  node2_content.erase(
      std::remove(node2_content.begin(), node2_content.end(), '\n'),
      node2_content.end());

  node1_content = absl::StripAsciiWhitespace(node1_content);
  node2_content = absl::StripAsciiWhitespace(node2_content);

  DVLOG(2) << "Comparing contents of "
           << reinterpret_cast<const char*>(node1->name) << "\n"
           << "First node's content:\n" << node1_content << "\n"
           << "Second node's content:\n" << node2_content;
  const bool same_content = node1_content == node2_content;
  LOG_IF(ERROR, !same_content)
      << "Contents of " << reinterpret_cast<const char*>(node1->name)
      << " do not match.\n"
      << "First node's content:\n" << node1_content << "\n"
      << "Second node's content:\n" << node2_content;
  return same_content;
}

// Recursively check the elements.
// Note that the terminating condition of the recursion is when the children do
// not match (inside the loop).
bool CompareNodes(xmlNodePtr node1, xmlNodePtr node2) {
  DCHECK(node1 && node2);
  if (!CompareNames(node1, node2)) {
    LOG(ERROR) << "Names of the nodes do not match: "
               << reinterpret_cast<const char*>(node1->name) << " "
               << reinterpret_cast<const char*>(node2->name);
    return false;
  }

  if (!CompareAttributes(node1, node2)) {
    LOG(ERROR) << "Attributes of element "
               << reinterpret_cast<const char*>(node1->name)
               << " do not match.";
    return false;
  }

  xmlNodePtr node1_child = xmlFirstElementChild(node1);
  xmlNodePtr node2_child = xmlFirstElementChild(node2);
  if (!node1_child && !node2_child) {
    // Note that xmlFirstElementChild() returns NULL if there are only
    // text type children.
    return CompareContents(node1, node2);
  }

  do {
    if (!node1_child || !node2_child)
      return node1_child == node2_child;

    DCHECK(node1_child && node2_child);
    if (!CompareNodes(node1_child, node2_child))
      return false;

    node1_child = xmlNextElementSibling(node1_child);
    node2_child = xmlNextElementSibling(node2_child);
  } while (node1_child || node2_child);

  // Both nodes have equivalent descendents.
  return true;
}
}  // namespace

bool XmlEqual(const std::string& xml1, const std::string& xml2) {
  xml::scoped_xml_ptr<xmlDoc> xml1_doc(GetDocFromString(xml1));
  xml::scoped_xml_ptr<xmlDoc> xml2_doc(GetDocFromString(xml2));
  if (!xml1_doc || !xml2_doc) {
    LOG(ERROR) << "xml1/xml2 is not valid XML.";
    return false;
  }

  xmlNodePtr xml1_root_element = xmlDocGetRootElement(xml1_doc.get());
  xmlNodePtr xml2_root_element = xmlDocGetRootElement(xml2_doc.get());
  if (!xml1_root_element || !xml2_root_element)
    return false;
  return CompareNodes(xml1_root_element, xml2_root_element);
}

bool XmlEqual(const std::string& xml1,
              const std::optional<xml::XmlNode>& xml2) {
  return xml2 && XmlEqual(xml1, *xml2);
}

bool XmlEqual(const std::string& xml1, const xml::XmlNode& xml2) {
  xml::scoped_xml_ptr<xmlDoc> xml1_doc(GetDocFromString(xml1));
  if (!xml1_doc) {
    LOG(ERROR) << "xml1 is not valid XML.";
    return false;
  }
  xmlNodePtr xml1_root_element = xmlDocGetRootElement(xml1_doc.get());
  if (!xml1_root_element)
    return false;
  return CompareNodes(xml1_root_element, xml2.GetRawPtr());
}

std::string XmlNodeToString(const std::optional<xml::XmlNode>& xml_node) {
  return xml_node ? XmlNodeToString(*xml_node) : "$ERROR$";
}

std::string XmlNodeToString(const xml::XmlNode& xml_node) {
  std::string output = xml_node.ToString(/* comment= */ "");

  // Remove the first line from the formatted string:
  //   <?xml version="" encoding="UTF-8"?>
  const size_t first_newline_char_pos = output.find('\n');
  DCHECK_NE(first_newline_char_pos, std::string::npos);
  return output.substr(first_newline_char_pos + 1);
}

}  // namespace shaka
