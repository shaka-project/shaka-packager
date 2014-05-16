// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
#define MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_

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

// On success fill |media_info| with input data and return true, otherwise
// return false.
bool GenerateMediaInfo(const MuxerOptions& muxer_options,
                       const std::vector<StreamInfo*>& stream_infos,
                       bool has_init_range,
                       uint64 init_range_start,
                       uint64 init_range_end,
                       bool has_index_range,
                       uint64 index_range_start,
                       uint64 index_range_end,
                       float duration_seconds,
                       uint64 file_size,
                       uint32 reference_time_scale_,
                       MuxerListener::ContainerType container_type,
                       dash_packager::MediaInfo* media_info);

}  // namespace internal
}  // namespace event
}  // namespace media
#endif  // MEDIA_EVENT_MUXER_LISTENER_INTERNAL_H_
