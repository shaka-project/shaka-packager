// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Packager utility functions.

#ifndef PACKAGER_APP_PACKAGER_UTIL_H_
#define PACKAGER_APP_PACKAGER_UTIL_H_

#include <gflags/gflags.h>

#include <memory>

DECLARE_bool(dump_stream_info);

namespace shaka {

struct MpdOptions;

namespace media {

class KeySource;
struct MuxerOptions;

/// Create KeySource based on provided command line options for content
/// encryption. Also fetches keys.
/// @return A std::unique_ptr containing a new KeySource, or nullptr if
///         encryption is not required.
std::unique_ptr<KeySource> CreateEncryptionKeySource();

/// Create KeySource based on provided command line options for content
/// decryption. Does not fetch keys.
/// @return A std::unique_ptr containing a new KeySource, or nullptr if
///         decryption is not required.
std::unique_ptr<KeySource> CreateDecryptionKeySource();

/// Fill MuxerOptions members using provided command line options.
bool GetMuxerOptions(MuxerOptions* muxer_options);

/// Fill MpdOptions members using provided command line options.
bool GetMpdOptions(bool on_demand_profile, MpdOptions* mpd_options);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_APP_PACKAGER_UTIL_H_
