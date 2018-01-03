// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/hls_notify_muxer_listener.h"

#include <memory>
#include "packager/base/logging.h"
#include "packager/hls/base/hls_notifier.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/event/muxer_listener_internal.h"

namespace shaka {
namespace media {

HlsNotifyMuxerListener::HlsNotifyMuxerListener(
    const std::string& playlist_name,
    const std::string& ext_x_media_name,
    const std::string& ext_x_media_group_id,
    hls::HlsNotifier* hls_notifier)
    : playlist_name_(playlist_name),
      ext_x_media_name_(ext_x_media_name),
      ext_x_media_group_id_(ext_x_media_group_id),
      hls_notifier_(hls_notifier) {
  DCHECK(hls_notifier);
}

HlsNotifyMuxerListener::~HlsNotifyMuxerListener() {}

// These methods work together to notify that the media is encrypted.
// If OnEncryptionInfoReady() is called before the media has been started, then
// the information is stored and handled when OnEncryptionStart() is called.
// If OnEncryptionStart() is called before the media has been started then
// OnMediaStart() is responsible for notifying that the segments are encrypted
// right away i.e. call OnEncryptionStart().
void HlsNotifyMuxerListener::OnEncryptionInfoReady(
    bool is_initial_encryption_info,
    FourCC protection_scheme,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& iv,
    const std::vector<ProtectionSystemSpecificInfo>& key_system_infos) {
  if (!media_started_) {
    next_key_id_ = key_id;
    next_iv_ = iv;
    next_key_system_infos_ = key_system_infos;
    protection_scheme_ = protection_scheme;
    return;
  }
  for (const ProtectionSystemSpecificInfo& info : key_system_infos) {
    const bool result = hls_notifier_->NotifyEncryptionUpdate(
        stream_id_, key_id, info.system_id(), iv, info.CreateBox());
    LOG_IF(WARNING, !result) << "Failed to add encryption info.";
  }
}

void HlsNotifyMuxerListener::OnEncryptionStart() {
  if (!media_started_) {
    must_notify_encryption_start_ = true;
    return;
  }
  if (next_key_id_.empty()) {
    DCHECK(next_iv_.empty());
    DCHECK(next_key_system_infos_.empty());
    return;
  }

  for (const ProtectionSystemSpecificInfo& info : next_key_system_infos_) {
    const bool result = hls_notifier_->NotifyEncryptionUpdate(
        stream_id_, next_key_id_, info.system_id(), next_iv_,
        info.CreateBox());
    LOG_IF(WARNING, !result) << "Failed to add encryption info";
  }
  next_key_id_.clear();
  next_iv_.clear();
  next_key_system_infos_.clear();
  must_notify_encryption_start_ = false;
}

void HlsNotifyMuxerListener::OnMediaStart(const MuxerOptions& muxer_options,
                                          const StreamInfo& stream_info,
                                          uint32_t time_scale,
                                          ContainerType container_type) {
  MediaInfo media_info;
  if (!internal::GenerateMediaInfo(muxer_options, stream_info, time_scale,
                                   container_type, &media_info)) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }
  if (protection_scheme_ != FOURCC_NULL) {
    internal::SetContentProtectionFields(protection_scheme_, next_key_id_,
                                         next_key_system_infos_, &media_info);
  }

  media_info_ = media_info;
  if (!media_info_.has_segment_template()) {
    return;
  }

  const bool result = hls_notifier_->NotifyNewStream(
      media_info, playlist_name_, ext_x_media_name_, ext_x_media_group_id_,
      &stream_id_);
  if (!result) {
    LOG(WARNING) << "Failed to notify new stream.";
    return;
  }

  media_started_ = true;
  if (must_notify_encryption_start_) {
    OnEncryptionStart();
  }
}

void HlsNotifyMuxerListener::OnSampleDurationReady(uint32_t sample_duration) {}

void HlsNotifyMuxerListener::OnMediaEnd(const MediaRanges& media_ranges,
                                        float duration_seconds) {
  // TODO(kqyang): Should we just Flush here to avoid calling Flush explicitly?
  // Don't flush the notifier here. Flushing here would write all the playlists
  // before all Media Playlists are read. Which could cause problems
  // setting the correct EXT-X-TARGETDURATION.
  if (media_info_.has_segment_template()) {
    return;
  }
  if (media_ranges.init_range) {
    shaka::Range* init_range = media_info_.mutable_init_range();
    init_range->set_begin(media_ranges.init_range.value().start);
    init_range->set_end(media_ranges.init_range.value().end);
  }
  if (media_ranges.index_range) {
    shaka::Range* index_range = media_info_.mutable_index_range();
    index_range->set_begin(media_ranges.index_range.value().start);
    index_range->set_end(media_ranges.index_range.value().end);
  }

  // TODO(rkuroiwa): Make this a method. This is the same as OnMediaStart().
  const bool result = hls_notifier_->NotifyNewStream(
      media_info_, playlist_name_, ext_x_media_name_, ext_x_media_group_id_,
      &stream_id_);
  if (!result) {
    LOG(WARNING) << "Failed to notify new stream for VOD.";
    return;
  }

  // TODO(rkuroiwa); Keep track of which (sub)segments are encrypted so that the
  // notification is sent right before the enecrypted (sub)segments.
  media_started_ = true;
  if (must_notify_encryption_start_) {
    OnEncryptionStart();
  }

  if (!media_ranges.subsegment_ranges.empty()) {
    const std::vector<Range>& subsegment_ranges =
        media_ranges.subsegment_ranges;
    size_t num_subsegments = subsegment_ranges.size();
    if (subsegments_.size() != num_subsegments) {
      LOG(WARNING) << "Number of subsegment ranges (" << num_subsegments
                   << ") does not match the number of subsegments notified to "
                      "OnNewSegment() ("
                   << subsegments_.size() << ").";
      num_subsegments = std::min(subsegments_.size(), num_subsegments);
    }
    for (size_t i = 0; i < num_subsegments; ++i) {
      const Range& range = subsegment_ranges[i];
      const SubsegmentInfo& subsegment_info = subsegments_[i];
      if (subsegment_info.cue_break) {
        hls_notifier_->NotifyCueEvent(stream_id_, subsegment_info.start_time);
      }
      hls_notifier_->NotifyNewSegment(
          stream_id_, media_info_.media_file_name(), subsegment_info.start_time,
          subsegment_info.duration, range.start, range.end + 1 - range.start);
    }
  }
}

void HlsNotifyMuxerListener::OnNewSegment(const std::string& file_name,
                                          uint64_t start_time,
                                          uint64_t duration,
                                          uint64_t segment_file_size) {
  if (!media_info_.has_segment_template()) {
    SubsegmentInfo subsegment = {start_time, duration, segment_file_size,
                                 next_subsegment_contains_cue_break_};
    subsegments_.push_back(subsegment);
    next_subsegment_contains_cue_break_ = false;
    return;
  }
  // For multisegment, it always starts from the beginning of the file.
  const size_t kStartingByteOffset = 0u;
  const bool result = hls_notifier_->NotifyNewSegment(
      stream_id_, file_name, start_time, duration, kStartingByteOffset,
      segment_file_size);
  LOG_IF(WARNING, !result) << "Failed to add new segment.";
}

void HlsNotifyMuxerListener::OnCueEvent(uint64_t timestamp,
                                        const std::string& cue_data) {
  if (!media_info_.has_segment_template()) {
    next_subsegment_contains_cue_break_ = true;
    return;
  }
  hls_notifier_->NotifyCueEvent(stream_id_, timestamp);
}

}  // namespace media
}  // namespace shaka
