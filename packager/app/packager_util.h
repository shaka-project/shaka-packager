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

#include "packager/base/optional.h"
#include "packager/media/base/fourccs.h"
#include "packager/packager.h"

namespace shaka {

// TODO(kqyang): Should we consolidate XxxParams and XxxOptions?
struct ChunkingParams;
struct DecryptionParams;
struct EncryptionParams;
struct Mp4OutputParams;
struct MpdOptions;
struct MpdParams;

namespace media {

class MediaHandler;
class KeySource;
class Status;
struct ChunkingOptions;
struct EncryptionOptions;
struct MuxerOptions;

/// Create KeySource based on provided command line options for content
/// encryption. Also fetches keys.
/// @param protection_scheme specifies the protection scheme to be used for
///        encryption.
/// @return A std::unique_ptr containing a new KeySource, or nullptr if
///         encryption is not required.
std::unique_ptr<KeySource> CreateEncryptionKeySource(
    FourCC protection_scheme,
    const EncryptionParams& encryption_params);

/// Create KeySource based on provided command line options for content
/// decryption. Does not fetch keys.
/// @return A std::unique_ptr containing a new KeySource, or nullptr if
///         decryption is not required.
std::unique_ptr<KeySource> CreateDecryptionKeySource(
    const DecryptionParams& decryption_params);

/// @return ChunkingOptions from provided command line options.
ChunkingOptions GetChunkingOptions(const ChunkingParams& chunking_params);

/// @return EncryptionOptions from provided command line options.
EncryptionOptions GetEncryptionOptions(
    const EncryptionParams& encryption_params);

/// @return MuxerOptions from provided command line options.
MuxerOptions GetMuxerOptions(const std::string& temp_dir,
                             const Mp4OutputParams& mp4_params);

/// @return MpdOptions from provided command line options.
MpdOptions GetMpdOptions(bool on_demand_profile, const MpdParams& mpd_params);

/// Connect handlers in the vector.
/// @param handlers A vector of media handlers to be conncected. the handlers
///        are chained from front() to back().
/// @return OK on success.
Status ConnectHandlers(std::vector<std::shared_ptr<MediaHandler>>& handlers);

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_APP_PACKAGER_UTIL_H_
