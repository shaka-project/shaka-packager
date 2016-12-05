// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_HLS_NOTIFY_MUXER_LISTENER_H_
#define PACKAGER_MEDIA_EVENT_HLS_NOTIFY_MUXER_LISTENER_H_

#include <string>

#include "packager/base/macros.h"
#include "packager/media/event/muxer_listener.h"

namespace shaka {

namespace hls {
class HlsNotifier;
}  // namespace hls

namespace media {

/// MuxerListener that uses HlsNotifier.
class HlsNotifyMuxerListener : public MuxerListener {
 public:
  /// @param playlist_name is the name of the playlist for the muxer's stream.
  /// @param ext_x_media_name is the name of this playlist. This is the
  ///        value of the NAME attribute for EXT-X-MEDIA, it is not the same as
  ///        @a playlist_name. This may be empty for video.
  /// @param ext_x_media_group_id is the group ID for this playlist. This is the
  ///        value of GROUP-ID attribute for EXT-X-MEDIA. This may be empty for
  ///        video.
  /// @param hls_notifier used by this listener. Ownership does not transfer.
  HlsNotifyMuxerListener(const std::string& playlist_name,
                         const std::string& ext_x_media_name,
                         const std::string& ext_x_media_group_id,
                         hls::HlsNotifier* hls_notifier);
  ~HlsNotifyMuxerListener() override;

  /// @name MuxerListener implementation overrides.
  /// @{
  void OnEncryptionInfoReady(bool is_initial_encryption_info,
                             FourCC protection_scheme,
                             const std::vector<uint8_t>& key_id,
                             const std::vector<uint8_t>& iv,
                             const std::vector<ProtectionSystemSpecificInfo>&
                                 key_system_info) override;
  void OnEncryptionStart() override;
  void OnMediaStart(const MuxerOptions& muxer_options,
                    const StreamInfo& stream_info,
                    uint32_t time_scale,
                    ContainerType container_type) override;
  void OnSampleDurationReady(uint32_t sample_duration) override;
  void OnMediaEnd(bool has_init_range,
                  uint64_t init_range_start,
                  uint64_t init_range_end,
                  bool has_index_range,
                  uint64_t index_range_start,
                  uint64_t index_range_end,
                  float duration_seconds,
                  uint64_t file_size) override;
  void OnNewSegment(const std::string& file_name,
                    uint64_t start_time,
                    uint64_t duration,
                    uint64_t segment_file_size) override;
  /// @}

 private:
  const std::string playlist_name_;
  const std::string ext_x_media_name_;
  const std::string ext_x_media_group_id_;
  hls::HlsNotifier* const hls_notifier_;
  uint32_t stream_id_ = 0;

  bool media_started_ = false;
  bool must_notify_encryption_start_ = false;
  // Cached encryption info before OnMediaStart() is called.
  std::vector<uint8_t> next_key_id_;
  std::vector<uint8_t> next_iv_;
  std::vector<ProtectionSystemSpecificInfo> next_key_system_infos_;

  DISALLOW_COPY_AND_ASSIGN(HlsNotifyMuxerListener);
};

}  // namespace media
}  // namespace shaka
#endif  // PACKAGER_MEDIA_EVENT_HLS_NOTIFY_MUXER_LISTENER_H_
