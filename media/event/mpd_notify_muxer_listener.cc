// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "media/event/mpd_notify_muxer_listener.h"

#include <cmath>

#include "base/logging.h"
#include "media/base/audio_stream_info.h"
#include "media/base/video_stream_info.h"
#include "media/event/muxer_listener_internal.h"
#include "mpd/base/media_info.pb.h"
#include "mpd/base/mpd_notifier.h"

using dash_packager::MediaInfo;

namespace media {
namespace event {

MpdNotifyMuxerListener::MpdNotifyMuxerListener(
    dash_packager::MpdNotifier* mpd_notifier)
    : mpd_notifier_(mpd_notifier),
      notification_id_(0) {
  DCHECK(mpd_notifier);
  DCHECK(mpd_notifier->dash_profile() == dash_packager::kOnDemandProfile ||
         mpd_notifier->dash_profile() == dash_packager::kLiveProfile);
}

MpdNotifyMuxerListener::~MpdNotifyMuxerListener() {}

void MpdNotifyMuxerListener::SetContentProtectionSchemeIdUri(
    const std::string& scheme_id_uri) {
  scheme_id_uri_ = scheme_id_uri;
}

void MpdNotifyMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const std::vector<StreamInfo*>& stream_infos,
    uint32 time_scale,
    ContainerType container_type,
    bool is_encrypted) {
  scoped_ptr<MediaInfo> media_info(new MediaInfo());
  if (!internal::GenerateMediaInfo(muxer_options,
                                   stream_infos,
                                   time_scale,
                                   container_type,
                                   media_info.get())) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  if (is_encrypted) {
    if (!internal::AddContentProtectionElements(
            container_type, scheme_id_uri_, media_info.get())) {
      LOG(ERROR) << "Failed to add content protection elements.";
      return;
    }
  }

  if (mpd_notifier_->dash_profile() == dash_packager::kLiveProfile) {
    // TODO(kqyang): Check return result.
    mpd_notifier_->NotifyNewContainer(*media_info, &notification_id_);
  } else {
    media_info_ = media_info.Pass();
  }
}

void MpdNotifyMuxerListener::OnMediaEnd(bool has_init_range,
                                        uint64 init_range_start,
                                        uint64 init_range_end,
                                        bool has_index_range,
                                        uint64 index_range_start,
                                        uint64 index_range_end,
                                        float duration_seconds,
                                        uint64 file_size) {
  if (mpd_notifier_->dash_profile() == dash_packager::kLiveProfile)
    return;

  DCHECK(media_info_);
  if (!internal::SetVodInformation(has_init_range,
                                   init_range_start,
                                   init_range_end,
                                   has_index_range,
                                   index_range_start,
                                   index_range_end,
                                   duration_seconds,
                                   file_size,
                                   media_info_.get())) {
    LOG(ERROR) << "Failed to generate VOD information from input.";
    return;
  }

  uint32 id;  // Result unused.
  // TODO(kqyang): Check return result.
  mpd_notifier_->NotifyNewContainer(*media_info_, &id);
}

void MpdNotifyMuxerListener::OnNewSegment(uint64 start_time,
                                          uint64 duration,
                                          uint64 segment_file_size) {
  if (mpd_notifier_->dash_profile() != dash_packager::kLiveProfile)
    return;
  // TODO(kqyang): Check return result.
  mpd_notifier_->NotifyNewSegment(notification_id_, start_time, duration);
}

}  // namespace event
}  // namespace media
