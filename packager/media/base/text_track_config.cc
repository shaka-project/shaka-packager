// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/base/text_track_config.h>

namespace shaka {
namespace media {

TextTrackConfig::TextTrackConfig()
    : kind_(kTextNone) {
}

TextTrackConfig::TextTrackConfig(TextKind kind,
                                 const std::string& label,
                                 const std::string& language,
                                 const std::string& id)
    : kind_(kind),
      label_(label),
      language_(language),
      id_(id) {
}

bool TextTrackConfig::Matches(const TextTrackConfig& config) const {
  return config.kind() == kind_ &&
         config.label() == label_ &&
         config.language() == language_ &&
         config.id() == id_;
}

}  // namespace media
}  // namespace shaka
