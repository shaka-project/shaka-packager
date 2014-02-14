// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/event/vod_mpd_notify_muxer_listener.h"

#include <cmath>

#include "base/logging.h"
#include "media/base/audio_stream_info.h"
#include "media/base/video_stream_info.h"
#include "media/event/vod_muxer_listener_internal.h"
#include "mpd/base/media_info.pb.h"
#include "mpd/base/mpd_notifier.h"

namespace media {
namespace event {

VodMpdNotifyMuxerListener::VodMpdNotifyMuxerListener(
    dash_packager::MpdNotifier* mpd_notifier)
    : mpd_notifier_(mpd_notifier),
      reference_time_scale_(0),
      container_type_(kContainerUnknown) {
  DCHECK(mpd_notifier);
}

VodMpdNotifyMuxerListener::~VodMpdNotifyMuxerListener() {}

void VodMpdNotifyMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const std::vector<StreamInfo*>& stream_infos,
    uint32 time_scale,
    ContainerType container_type) {
  muxer_options_ = muxer_options;
  reference_time_scale_ = time_scale;
  container_type_ = container_type;
}

void VodMpdNotifyMuxerListener::OnMediaEnd(
    const std::vector<StreamInfo*>& stream_infos,
    bool has_init_range,
    uint64 init_range_start,
    uint64 init_range_end,
    bool has_index_range,
    uint64 index_range_start,
    uint64 index_range_end,
    float duration_seconds,
    uint64 file_size) {
  dash_packager::MediaInfo media_info;
  if (!internal::GenerateMediaInfo(muxer_options_,
                                   stream_infos,
                                   has_init_range,
                                   init_range_start,
                                   init_range_end,
                                   has_index_range,
                                   index_range_start,
                                   index_range_end,
                                   duration_seconds,
                                   file_size,
                                   reference_time_scale_,
                                   container_type_,
                                   &media_info)) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  uint32 id;  // Result unused.
  mpd_notifier_->NotifyNewContainer(media_info, &id);
}

void VodMpdNotifyMuxerListener::OnNewSegment(uint64 start_time,
                                             uint64 duration,
                                             uint64 segment_file_size) {}

}  // namespace event
}  // namespace media
