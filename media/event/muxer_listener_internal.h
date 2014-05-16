// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
#define MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/event/muxer_listener.h"

namespace dash_packager {
class MediaInfo;
}  // namespace dash_packager

namespace media {

class StreamInfo;
struct MuxerOptions;

namespace event {
namespace internal {

/// @param[out] media_info points to the MediaInfo object to be filled.
/// @return true on success, false otherwise.
bool GenerateMediaInfo(const MuxerOptions& muxer_options,
                       const std::vector<StreamInfo*>& stream_infos,
                       uint32 reference_time_scale_,
                       MuxerListener::ContainerType container_type,
                       dash_packager::MediaInfo* media_info);

/// @param[in,out] media_info points to the MediaInfo object to be filled.
/// @return true on success, false otherwise.
bool SetVodInformation(bool has_init_range,
                       uint64 init_range_start,
                       uint64 init_range_end,
                       bool has_index_range,
                       uint64 index_range_start,
                       uint64 index_range_end,
                       float duration_seconds,
                       uint64 file_size,
                       dash_packager::MediaInfo* media_info);

/// @param container_type specifies container type. A default ContentProtection
///        element will be added if the container is MP4.
/// @param user_scheme_id_uri is the user specified schemeIdUri for
///        ContentProtection.
/// @return true if a ContentProtectionXml is added, false otherwise.
bool AddContentProtectionElements(MuxerListener::ContainerType container_type,
                                  const std::string& user_scheme_id_uri,
                                  dash_packager::MediaInfo* media_info);

}  // namespace internal
}  // namespace event
}  // namespace media
#endif  // MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
