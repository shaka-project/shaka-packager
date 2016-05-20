// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/hls_notify_muxer_listener.h"

#include "packager/base/logging.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/hls/base/hls_notifier.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/protection_system_specific_info.h"
#include "packager/media/event/muxer_listener_internal.h"

namespace shaka {
namespace media {

HlsNotifyMuxerListener::HlsNotifyMuxerListener(const std::string& playlist_name,
                                               hls::HlsNotifier* hls_notifier)
    : playlist_name_(playlist_name), hls_notifier_(hls_notifier) {
  DCHECK(hls_notifier);
}

HlsNotifyMuxerListener::~HlsNotifyMuxerListener() {}

void HlsNotifyMuxerListener::OnEncryptionInfoReady(
    bool is_initial_encryption_info,
    FourCC protection_scheme,
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& iv,
    const std::vector<ProtectionSystemSpecificInfo>& key_system_infos) {
  for (const ProtectionSystemSpecificInfo& info : key_system_infos) {
    const bool result = hls_notifier_->NotifyEncryptionUpdate(
        stream_id_, key_id, info.system_id(), iv, info.pssh_data());
    LOG_IF(WARNING, !result) << "Failed to add encryption info.";
  }
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
      media_info, playlist_name_, muxer_options.hls_name,
      muxer_options.hls_group_id, &stream_id_);
  LOG_IF(WARNING, !result) << "Failed to notify new stream.";
}

void HlsNotifyMuxerListener::OnSampleDurationReady(uint32_t sample_duration) {
}

void HlsNotifyMuxerListener::OnMediaEnd(bool has_init_range,
                                        uint64_t init_range_start,
                                        uint64_t init_range_end,
                                        bool has_index_range,
                                        uint64_t index_range_start,
                                        uint64_t index_range_end,
                                        float duration_seconds,
                                        uint64_t file_size) {
  const bool result = hls_notifier_->Flush();
  LOG_IF(WARNING, !result) << "Failed to flush.";
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
