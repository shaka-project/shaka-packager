// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_CODECS_WEBVTT_UTIL_H_
#define PACKAGER_MEDIA_CODECS_WEBVTT_UTIL_H_

#include <cstdint>
#include <vector>

namespace shaka {
namespace media {

// Utility function to create side data item for decoder buffer.
template <typename T>
void MakeSideData(T id_begin,
                  T id_end,
                  T settings_begin,
                  T settings_end,
                  std::vector<uint8_t>* side_data) {
  // The DecoderBuffer only supports a single side data item. In the case of
  // a WebVTT cue, we can have potentially two side data items. In order to
  // avoid disrupting DecoderBuffer any more than we need to, we copy both
  // side data items onto a single one, and terminate each with a NUL marker.
  side_data->clear();
  side_data->insert(side_data->end(), id_begin, id_end);
  side_data->push_back(0);
  side_data->insert(side_data->end(), settings_begin, settings_end);
  side_data->push_back(0);
}

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CODECS_WEBVTT_UTIL_H_
