// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_HLS_PUBLIC_HLS_PLAYLIST_TYPE_H_
#define PACKAGER_HLS_PUBLIC_HLS_PLAYLIST_TYPE_H_

namespace shaka {

/// Defines the EXT-X-PLAYLIST-TYPE in the HLS specification. For
/// HlsPlaylistType of kLive, EXT-X-PLAYLIST-TYPE tag is omitted.
enum class HlsPlaylistType {
  kVod,
  kEvent,
  kLive,
};

}  // namespace shaka

#endif  // PACKAGER_HLS_PUBLIC_HLS_PLAYLIST_TYPE_H_
