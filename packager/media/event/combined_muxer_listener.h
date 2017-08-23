// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_COMBINED_MUXER_LISTENER_H_
#define PACKAGER_MEDIA_EVENT_COMBINED_MUXER_LISTENER_H_

#include <string>

#include "packager/media/event/hls_notify_muxer_listener.h"
#include "packager/media/event/mpd_notify_muxer_listener.h"

namespace shaka {

namespace media {

class CombinedMuxerListener : public MuxerListener {
 public:
  explicit CombinedMuxerListener(std::list<std::unique_ptr<MuxerListener>>* muxer_listeners);
  ~CombinedMuxerListener() override;

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
  void OnMediaEnd(const MediaRanges& media_ranges,
                  float duration_seconds) override;
  void OnNewSegment(const std::string& file_name,
                    uint64_t start_time,
                    uint64_t duration,
                    uint64_t segment_file_size) override;

  DISALLOW_COPY_AND_ASSIGN(CombinedMuxerListener);

  private:
  std::list<std::unique_ptr<MuxerListener>> muxer_listeners_;
};

}  // namespace media
}  // namespace shaka
#endif  // PACKAGER_MEDIA_EVENT_COMBINED_MUXER_LISTENER_H_
