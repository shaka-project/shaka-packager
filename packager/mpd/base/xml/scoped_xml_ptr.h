// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// unique_ptr alias for libxml2 objects. Deleters for the objects are also
// defined in this file.

#ifndef MPD_BASE_XML_SCOPED_XML_PTR_H_
#define MPD_BASE_XML_SCOPED_XML_PTR_H_

#include <libxml/tree.h>
#include <libxml/xmlschemas.h>

#include <memory>

namespace shaka {
namespace xml {

/// Deleter functor for deleting libxml2 pointers. This is used with
/// ScopedXmlPtr.
struct XmlDeleter {
  // Called by std::unique_ptr.
  inline void operator()(xmlSchemaParserCtxtPtr ptr) const {
    xmlSchemaFreeParserCtxt(ptr);
  }
  inline void operator()(xmlSchemaValidCtxtPtr ptr) const {
    xmlSchemaFreeValidCtxt(ptr);
  }
  inline void operator()(xmlSchemaPtr ptr) const { xmlSchemaFree(ptr); }
  inline void operator()(xmlNodePtr ptr) const { xmlFreeNode(ptr); }
  inline void operator()(xmlDocPtr ptr) const { xmlFreeDoc(ptr); }
  inline void operator()(xmlChar* ptr) const { xmlFree(ptr); }
};

template <typename XmlType>
using scoped_xml_ptr = std::unique_ptr<XmlType, XmlDeleter>;

}  // namespace xml
}  // namespace shaka

#endif  // MPD_BASE_XML_SCOPED_XML_PTR_H_
