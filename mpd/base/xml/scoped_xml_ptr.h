// scoped_ptr alias for libxml2 objects. Deleters for the objects are also
// defined in this file.
#ifndef MPD_BASE_XML_SCOPED_XML_PTR_H_
#define MPD_BASE_XML_SCOPED_XML_PTR_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/libxml/src/include/libxml/tree.h"

namespace dash_packager {
namespace xml {

struct XmlDeleter {
  // Called by scoped_ptr. http://goo.gl/YaLbcS
  inline void operator()(xmlNodePtr ptr) const { xmlFreeNode(ptr); }
  inline void operator()(xmlDocPtr ptr) const { xmlFreeDoc(ptr); }
  inline void operator()(xmlChar* ptr) const { xmlFree(ptr); }
};

// C++11 allows template alias but standards before it do not. This struct is
// for aliasing scoped_ptr with libxml2 object deleter.
template <typename XmlType>
struct ScopedXmlPtr {
  typedef scoped_ptr<XmlType, XmlDeleter> type;
};

}  // namespace xml
}  // namespace dash_packager
#endif  // MPD_BASE_XML_SCOPED_XML_PTR_H_
