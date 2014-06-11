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
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

DECLARE_bool(dump_stream_info);

namespace media {

class EncryptionKeySource;
class MediaInfo;
class MediaStream;
class Muxer;
struct MuxerOptions;

/// Print all the stream info for the provided strings to standard output.
void DumpStreamInfo(const std::vector<MediaStream*>& streams);

/// Create EncryptionKeySource based on provided command line options.
/// @return A scoped_ptr containig a new EncryptionKeySource, or NULL if
///         encryption is not required.
scoped_ptr<EncryptionKeySource> CreateEncryptionKeySource();

/// Fill MuxerOptions members using provided command line options.
bool GetMuxerOptions(MuxerOptions* muxer_options);

/// Select and add a stream from a provided set to a muxer.
/// @param streams contains the set of MediaStreams from which to select.
/// @param stream_selector is a string containing one of the following values:
///        "audio" to select the first audio track, "video" to select the first
///        video track, or a decimal number indicating which track number to
///        select (start at "1").
/// @return true if successful, false otherwise.
bool AddStreamToMuxer(const std::vector<MediaStream*>& streams,
                      const std::string& stream_selector,
                      Muxer* muxer);

}  // namespace media

#endif  // APP_PACKAGER_UTIL_H_
