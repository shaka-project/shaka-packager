// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
#define MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "packager/media/event/muxer_listener.h"

namespace edash_packager {

class MediaInfo;

namespace media {

class StreamInfo;
struct MuxerOptions;

namespace internal {

/// @param[out] media_info points to the MediaInfo object to be filled.
/// @return true on success, false otherwise.
bool GenerateMediaInfo(const MuxerOptions& muxer_options,
                       const StreamInfo& stream_info,
                       uint32_t reference_time_scale_,
                       MuxerListener::ContainerType container_type,
                       MediaInfo* media_info);

/// @param[in,out] media_info points to the MediaInfo object to be filled.
/// @return true on success, false otherwise.
bool SetVodInformation(bool has_init_range,
                       uint64_t init_range_start,
                       uint64_t init_range_end,
                       bool has_index_range,
                       uint64_t index_range_start,
                       uint64_t index_range_end,
                       float duration_seconds,
                       uint64_t file_size,
                       MediaInfo* media_info);

/// @param content_protection_uuid is the UUID of the content protection
///        in human readable form.
/// @param content_protection_name_version is the DRM name and verion.
/// @param default_key_id is the key ID for this media in hex (i.e. non-human
///        readable, typically 16 bytes.)
/// @param pssh is the pssh for the media in hex (i.e. non-human readable, raw
///        'pssh' box.)
/// @param media_info is where the content protection information is stored and
///        cannot be null.
void SetContentProtectionFields(
    const std::string& content_protection_uuid,
    const std::string& content_protection_name_version,
    const std::string& default_key_id,
    const std::string& pssh,
    MediaInfo* media_info);

}  // namespace internal
}  // namespace media
}  // namespace edash_packager
#endif  // MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
