// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Implementation of MuxerListener that deals with MpdNotifier.

#ifndef MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_
#define MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_

#include <vector>

#include "packager/base/compiler_specific.h"
#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/event/muxer_listener.h"

namespace edash_packager {

class MediaInfo;
class MpdNotifier;

namespace media {

class MpdNotifyMuxerListener : public MuxerListener {
 public:
  /// @param mpd_notifier must be initialized, i.e mpd_notifier->Init() must be
  ///        called.
  MpdNotifyMuxerListener(MpdNotifier* mpd_notifier);
  virtual ~MpdNotifyMuxerListener();

  /// If the stream is encrypted use this as 'schemeIdUri' attribute for
  /// ContentProtection element.
  void SetContentProtectionSchemeIdUri(const std::string& scheme_id_uri);

  /// @name MuxerListener implementation overrides.
  /// @{
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const StreamInfo& stream_info,
                            uint32_t time_scale,
                            ContainerType container_type,
                            bool is_encrypted) OVERRIDE;
  virtual void OnSampleDurationReady(uint32_t sample_duration) OVERRIDE;
  virtual void OnMediaEnd(bool has_init_range,
                          uint64_t init_range_start,
                          uint64_t init_range_end,
                          bool has_index_range,
                          uint64_t index_range_start,
                          uint64_t index_range_end,
                          float duration_seconds,
                          uint64_t file_size) OVERRIDE;
  virtual void OnNewSegment(uint64_t start_time,
                            uint64_t duration,
                            uint64_t segment_file_size) OVERRIDE;
  /// @}

 private:
  MpdNotifier* const mpd_notifier_;
  uint32_t notification_id_;
  scoped_ptr<MediaInfo> media_info_;
  std::string scheme_id_uri_;

  DISALLOW_COPY_AND_ASSIGN(MpdNotifyMuxerListener);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_
