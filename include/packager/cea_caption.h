// Copyright 2024 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_CEA_CAPTION_H_
#define PACKAGER_PUBLIC_CEA_CAPTION_H_

#include <string>

namespace shaka {

/// CEA caption description.
struct CeaCaption {
  /// The display name of the caption.
  std::string name;
  /// The language of the caption.
  std::string language;
  /// The channel of the caption, e.g. "CC1", "SERVICE2".
  std::string channel;
  /// True if this is the default caption.
  bool is_default = false;
  /// True if this caption should be autoselected.
  bool autoselect = true;
};

}  // namespace shaka

#endif  // PACKAGER_PUBLIC_CEA_CAPTION_H_
