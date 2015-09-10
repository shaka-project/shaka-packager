// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/mpd_notify_muxer_listener.h"

#include <cmath>

#include "packager/base/logging.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener_internal.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_notifier.h"

namespace edash_packager {
namespace media {

MpdNotifyMuxerListener::MpdNotifyMuxerListener(MpdNotifier* mpd_notifier)
    : mpd_notifier_(mpd_notifier), notification_id_(0), is_encrypted_(false) {
  DCHECK(mpd_notifier);
  DCHECK(mpd_notifier->dash_profile() == kOnDemandProfile ||
         mpd_notifier->dash_profile() == kLiveProfile);
}

MpdNotifyMuxerListener::~MpdNotifyMuxerListener() {}

void MpdNotifyMuxerListener::SetContentProtectionSchemeIdUri(
    const std::string& scheme_id_uri) {
  scheme_id_uri_ = scheme_id_uri;
}

void MpdNotifyMuxerListener::OnEncryptionInfoReady(
    const std::string& content_protection_uuid,
    const std::string& content_protection_name_version,
    const std::vector<uint8_t>& default_key_id,
    const std::vector<uint8_t>& pssh) {
  content_protection_uuid_ = content_protection_uuid;
  content_protection_name_version_ = content_protection_name_version;
  default_key_id_.assign(default_key_id.begin(), default_key_id.end());
  pssh_.assign(pssh.begin(), pssh.end());
  is_encrypted_ = true;
}

void MpdNotifyMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const StreamInfo& stream_info,
    uint32_t time_scale,
    ContainerType container_type) {
  scoped_ptr<MediaInfo> media_info(new MediaInfo());
  if (!internal::GenerateMediaInfo(muxer_options,
                                   stream_info,
                                   time_scale,
                                   container_type,
                                   media_info.get())) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  if (is_encrypted_) {
    internal::SetContentProtectionFields(
        content_protection_uuid_, content_protection_name_version_,
        default_key_id_, pssh_, media_info.get());
  }

  if (mpd_notifier_->dash_profile() == kLiveProfile) {
    // TODO(kqyang): Check return result.
    mpd_notifier_->NotifyNewContainer(*media_info, &notification_id_);
  } else {
    media_info_ = media_info.Pass();
  }
}

// Record the sample duration in the media info for VOD so that OnMediaEnd, all
// the information is in the media info.
void MpdNotifyMuxerListener::OnSampleDurationReady(
    uint32_t sample_duration) {
  if (mpd_notifier_->dash_profile() == kLiveProfile) {
    mpd_notifier_->NotifySampleDuration(notification_id_, sample_duration);
    return;
  }

  if (!media_info_) {
    LOG(WARNING) << "Got sample duration " << sample_duration
                 << " but no media was specified.";
    return;
  }
  if (!media_info_->has_video_info()) {
    // If non video, don't worry about it (at the moment).
    return;
  }
  if (media_info_->video_info().has_frame_duration()) {
    return;
  }

  media_info_->mutable_video_info()->set_frame_duration(sample_duration);
}

void MpdNotifyMuxerListener::OnMediaEnd(bool has_init_range,
                                        uint64_t init_range_start,
                                        uint64_t init_range_end,
                                        bool has_index_range,
                                        uint64_t index_range_start,
                                        uint64_t index_range_end,
                                        float duration_seconds,
                                        uint64_t file_size) {
  if (mpd_notifier_->dash_profile() == kLiveProfile) {
    DCHECK(subsegments_.empty());
    return;
  }

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

  uint32_t id;
  // TODO(kqyang): Check return result.
  mpd_notifier_->NotifyNewContainer(*media_info_, &id);
  for (std::list<SubsegmentInfo>::const_iterator it = subsegments_.begin();
       it != subsegments_.end(); ++it) {
    mpd_notifier_->NotifyNewSegment(id, it->start_time, it->duration,
                                    it->segment_file_size);
  }
  subsegments_.clear();
  mpd_notifier_->Flush();
}

void MpdNotifyMuxerListener::OnNewSegment(uint64_t start_time,
                                          uint64_t duration,
                                          uint64_t segment_file_size) {
  if (mpd_notifier_->dash_profile() == kLiveProfile) {
    // TODO(kqyang): Check return result.
    mpd_notifier_->NotifyNewSegment(
        notification_id_, start_time, duration, segment_file_size);
    mpd_notifier_->Flush();
  } else {
    SubsegmentInfo subsegment = {start_time, duration, segment_file_size};
    subsegments_.push_back(subsegment);
  }
}

}  // namespace media
}  // namespace edash_packager
