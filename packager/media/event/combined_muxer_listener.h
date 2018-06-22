// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_EVENT_COMBINED_MUXER_LISTENER_H_
#define PACKAGER_MEDIA_EVENT_COMBINED_MUXER_LISTENER_H_

#include <list>
#include <memory>

#include "packager/media/event/muxer_listener.h"

namespace shaka {
namespace media {

class CombinedMuxerListener : public MuxerListener {
 public:
  CombinedMuxerListener() = default;

  void AddListener(std::unique_ptr<MuxerListener> listener);

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
                    int64_t start_time,
                    int64_t duration,
                    uint64_t segment_file_size) override;
  void OnKeyFrame(int64_t timestamp, uint64_t start_byte_offset, uint64_t size);
  void OnCueEvent(int64_t timestamp, const std::string& cue_data) override;

 private:
  std::list<std::unique_ptr<MuxerListener>> muxer_listeners_;

  DISALLOW_COPY_AND_ASSIGN(CombinedMuxerListener);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_EVENT_COMBINED_MUXER_LISTENER_H_
