// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
#define PACKAGER_MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "packager/media/event/muxer_listener.h"

namespace shaka {

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

/// @return True if @a media_info1 and @a media_info2 are compatible. MediaInfos
///         are considered to be compatible if codec and container are the same.
bool IsMediaInfoCompatible(const MediaInfo& media_info1,
                           const MediaInfo& media_info2);

/// @param[in,out] media_info points to the MediaInfo object to be filled.
/// @return true on success, false otherwise.
bool SetVodInformation(const MuxerListener::MediaRanges& media_ranges,
                       float duration_seconds,
                       MediaInfo* media_info);

/// @param protection_scheme specifies the protection scheme: 'cenc', 'cens',
///        'cbc1', 'cbcs'.
/// @param default_key_id is the key ID for this media in hex (i.e. non-human
///        readable, typically 16 bytes.)
/// @param key_system_info the key-system specific info for the media.
/// @param media_info is where the content protection information is stored and
///        cannot be null.
void SetContentProtectionFields(
    FourCC protection_scheme,
    const std::vector<uint8_t>& default_key_id,
    const std::vector<ProtectionSystemSpecificInfo>& key_system_info,
    MediaInfo* media_info);

/// Creates a UUID string from the given binary data.  The data must be 16 bytes
/// long.  Outputs: "00000000-0000-0000-0000-000000000000"
std::string CreateUUIDString(const std::vector<uint8_t>& data);

}  // namespace internal
}  // namespace media
}  // namespace shaka
#endif  // PACKAGER_MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
