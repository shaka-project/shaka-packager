// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_BASE_TEXT_TRACK_H_
#define PACKAGER_MEDIA_BASE_TEXT_TRACK_H_

#include <memory>
#include <string>

#include "packager/base/callback.h"
#include "packager/base/time/time.h"

namespace shaka {
namespace media {

/// Specifies the varieties of text tracks.
enum TextKind {
  kTextSubtitles,
  kTextCaptions,
  kTextDescriptions,
  kTextMetadata,
  kTextNone
};

class TextTrack {
 public:
  ~TextTrack() override {}
  virtual void addWebVTTCue(const base::TimeDelta& start,
                            const base::TimeDelta& end,
                            const std::string& id,
                            const std::string& content,
                            const std::string& settings) = 0;
};

typedef base::Callback<std::unique_ptr<TextTrack>(TextKind kind,
                                                  const std::string& label,
                                                  const std::string& language)>
    AddTextTrackCB;

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_TEXT_TRACK_H_
