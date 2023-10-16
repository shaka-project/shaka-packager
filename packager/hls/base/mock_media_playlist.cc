// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/hls/base/mock_media_playlist.h>

#include <packager/hls_params.h>

namespace shaka {
namespace hls {

MockMediaPlaylist::MockMediaPlaylist(const std::string& file_name,
                                     const std::string& name,
                                     const std::string& group_id)
    : MediaPlaylist(HlsParams(), file_name, name, group_id) {}
MockMediaPlaylist::~MockMediaPlaylist() {}

}  // namespace hls
}  // namespace shaka
