#ifndef MPD_TEST_XML_COMPARE_H_
#define MPD_TEST_XML_COMPARE_H_

#include <libxml/tree.h>

#include <string>

namespace edash_packager {

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

}  // namespace edash_packager

#endif  // MPD_TEST_XML_COMPARE_H_
