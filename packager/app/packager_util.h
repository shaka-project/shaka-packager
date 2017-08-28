// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Packager utility functions.

#ifndef PACKAGER_APP_PACKAGER_UTIL_H_
#define PACKAGER_APP_PACKAGER_UTIL_H_

#include <memory>
#include <vector>

#include "packager/media/base/fourccs.h"

namespace shaka {

class Status;
struct DecryptionParams;
struct EncryptionParams;
struct MpdOptions;
struct MpdParams;

namespace media {

class MediaHandler;
class KeySource;

/// Create KeySource based on provided params for content encryption. Also
/// fetches keys.
/// @param protection_scheme specifies the protection scheme to be used for
///        encryption.
/// @return A std::unique_ptr containing a new KeySource, or nullptr if
///         encryption is not required.
std::unique_ptr<KeySource> CreateEncryptionKeySource(
    FourCC protection_scheme,
    const EncryptionParams& encryption_params);

/// Create KeySource based on provided params for content decryption. Does not
/// fetch keys.
/// @return A std::unique_ptr containing a new KeySource, or nullptr if
///         decryption is not required.
std::unique_ptr<KeySource> CreateDecryptionKeySource(
    const DecryptionParams& decryption_params);

/// @return MpdOptions from provided inputs.
MpdOptions GetMpdOptions(bool on_demand_profile, const MpdParams& mpd_params);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_APP_PACKAGER_UTIL_H_
