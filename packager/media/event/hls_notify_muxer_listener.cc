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
// if OnEncryptionStart() is called before the media has been started then
// OnMediaStart() is responsible for notifying that the segments are encrypted
// right away i.e. call OnEncryptionStart().
// For now (because Live HLS is not implemented yet) this should be called once,
// before media is started. So the logic after the first if statement should not
// be taken.
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
    return;
  }
  for (const ProtectionSystemSpecificInfo& info : key_system_infos) {
    const bool result = hls_notifier_->NotifyEncryptionUpdate(
        stream_id_, key_id, info.system_id(), iv, info.pssh_data());
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
        info.pssh_data());
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

void HlsNotifyMuxerListener::OnMediaEnd(bool has_init_range,
                                        uint64_t init_range_start,
                                        uint64_t init_range_end,
                                        bool has_index_range,
                                        uint64_t index_range_start,
                                        uint64_t index_range_end,
                                        float duration_seconds,
                                        uint64_t file_size) {
  // Don't flush the notifier here. Flushing here would write all the playlists
  // before all Media Playlists are read. Which could cause problems
  // setting the correct EXT-X-TARGETDURATION.
}

void HlsNotifyMuxerListener::OnNewSegment(const std::string& file_name,
                                          uint64_t start_time,
                                          uint64_t duration,
                                          uint64_t segment_file_size) {
  const bool result = hls_notifier_->NotifyNewSegment(
      stream_id_, file_name, start_time, duration, segment_file_size);
  LOG_IF(WARNING, !result) << "Failed to add new segment.";
}

}  // namespace media
}  // namespace shaka
