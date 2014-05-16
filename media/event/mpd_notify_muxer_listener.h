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

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/muxer_options.h"
#include "media/event/muxer_listener.h"

namespace dash_packager {
class MediaInfo;
class MpdNotifier;
}  // namespace dash_packager

namespace media {
namespace event {

class MpdNotifyMuxerListener : public MuxerListener {
 public:
  /// @param mpd_notifier must be initialized, i.e mpd_notifier->Init() must be
  ///        called.
  MpdNotifyMuxerListener(dash_packager::MpdNotifier* mpd_notifier);
  virtual ~MpdNotifyMuxerListener();

  /// If the stream is encrypted use this as 'schemeIdUri' attribute for
  /// ContentProtection element.
  void SetContentProtectionSchemeIdUri(const std::string& scheme_id_uri);

  /// @name MuxerListener implementation overrides.
  /// @{
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const std::vector<StreamInfo*>& stream_infos,
                            uint32 time_scale,
                            ContainerType container_type,
                            bool is_encrypted) OVERRIDE;

  virtual void OnMediaEnd(bool has_init_range,
                          uint64 init_range_start,
                          uint64 init_range_end,
                          bool has_index_range,
                          uint64 index_range_start,
                          uint64 index_range_end,
                          float duration_seconds,
                          uint64 file_size) OVERRIDE;

  virtual void OnNewSegment(uint64 start_time,
                            uint64 duration,
                            uint64 segment_file_size) OVERRIDE;
  /// @}

 private:
  dash_packager::MpdNotifier* const mpd_notifier_;
  uint32 notification_id_;
  scoped_ptr<dash_packager::MediaInfo> media_info_;
  std::string scheme_id_uri_;

  DISALLOW_COPY_AND_ASSIGN(MpdNotifyMuxerListener);
};

}  // namespace event
}  // namespace media

#endif  // MEDIA_EVENT_MPD_NOTIFY_MUXER_LISTENER_H_
