// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// ContentProtectionElement is shared a structure that can be passed to
// MPD generator classes to add ContentProtection element in the resulting MPD
// file.
// http://goo.gl/UrsSlF

#ifndef MPD_BASE_CONTENT_PROTECTION_ELEMENT_H_
#define MPD_BASE_CONTENT_PROTECTION_ELEMENT_H_

#include <map>
#include <string>

namespace dash_packager {

/// Structure to represent <ContentProtection> element in DASH MPD spec (ISO
/// 23009-1:2012 MPD and Segment Formats).
struct ContentProtectionElement {
  ContentProtectionElement();
  ~ContentProtectionElement();

  std::string value;  // Will be set for 'value' attribute.
  std::string scheme_id_uri;  // Will be set for 'schemeIdUri' attribute.

  // Other attributes for this element.
  std::map<std::string, std::string> additional_attributes;

  // The elements that will be in this element.
  std::string subelements;
};

}  // namespace dash_packager

#endif  // MPD_BASE_CONTENT_PROTECTION_ELEMENT_H_
