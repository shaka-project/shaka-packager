// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Packager utility functions.

#ifndef APP_PACKAGER_UTIL_H_
#define APP_PACKAGER_UTIL_H_

#include <gflags/gflags.h>
#include <memory>
#include <string>
#include <vector>

DECLARE_bool(dump_stream_info);

namespace shaka {

struct MpdOptions;

namespace media {

class KeySource;
class MediaStream;
class Muxer;
struct MuxerOptions;

/// Print all the stream info for the provided strings to standard output.
void DumpStreamInfo(const std::vector<MediaStream*>& streams);

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

/// Set flags according to profile.
bool AssignFlagsFromProfile();

/// Fill MuxerOptions members using provided command line options.
bool GetMuxerOptions(MuxerOptions* muxer_options);

/// Fill MpdOptions members using provided command line options.
bool GetMpdOptions(MpdOptions* mpd_options);

/// Select and add a stream from a provided set to a muxer.
/// @param streams contains the set of MediaStreams from which to select.
/// @param stream_selector is a string containing one of the following values:
///        "audio" to select the first audio track, "video" to select the first
///        video track, or a decimal number indicating which track number to
///        select (start at "1").
/// @param language_override is a string which, if non-empty, overrides the
///        stream's language metadata.
/// @return true if successful, false otherwise.
bool AddStreamToMuxer(const std::vector<MediaStream*>& streams,
                      const std::string& stream_selector,
                      const std::string& language_override,
                      Muxer* muxer);

}  // namespace media
}  // namespace shaka

#endif  // APP_PACKAGER_UTIL_H_
