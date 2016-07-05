// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implementation of MuxerListener that deals with MpdNotifier.

#ifndef MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_
#define MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_

#include <list>
#include <vector>

#include "packager/base/compiler_specific.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/event/muxer_listener.h"

namespace shaka {

class MediaInfo;
class MpdNotifier;

namespace media {

class MpdNotifyMuxerListener : public MuxerListener {
 public:
  /// @param mpd_notifier must be initialized, i.e mpd_notifier->Init() must be
  ///        called.
  explicit MpdNotifyMuxerListener(MpdNotifier* mpd_notifier);
  ~MpdNotifyMuxerListener() override;

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
  // This stores data passed into OnNewSegment() for VOD.
  struct SubsegmentInfo {
    uint64_t start_time;
    uint64_t duration;
    uint64_t segment_file_size;
  };

  MpdNotifier* const mpd_notifier_;
  uint32_t notification_id_;
  scoped_ptr<MediaInfo> media_info_;

  bool is_encrypted_;
  // Storage for values passed to OnEncryptionInfoReady().
  FourCC protection_scheme_;
  std::string default_key_id_;
  std::vector<ProtectionSystemSpecificInfo> key_system_info_;

  // Saves all the subsegment information for VOD. This should be used to call
  // MpdNotifier::NotifyNewSegment() after NotifyNewSegment() is called
  // (in OnMediaEnd). This is not used for live because NotifyNewSegment() is
  // called immediately in OnNewSegment().
  std::list<SubsegmentInfo> subsegments_;

  DISALLOW_COPY_AND_ASSIGN(MpdNotifyMuxerListener);
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_
