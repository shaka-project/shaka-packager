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

/// @param container_type specifies container type. A default ContentProtection
///        element will be added if the container is MP4.
/// @param user_scheme_id_uri is the user specified schemeIdUri for
///        ContentProtection.
/// @return true if a ContentProtectionXml is added, false otherwise.
bool AddContentProtectionElements(MuxerListener::ContainerType container_type,
                                  const std::string& user_scheme_id_uri,
                                  MediaInfo* media_info);

}  // namespace internal
}  // namespace media
}  // namespace edash_packager
#endif  // MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
