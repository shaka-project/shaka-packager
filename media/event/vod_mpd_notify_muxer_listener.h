// Implementation of MuxerListener that deals with MpdNotifier.
#ifndef MEDIA_EVENT_VOD_MPD_NOTIFY_MUXER_LISTENER_H_
#define MEDIA_EVENT_VOD_MPD_NOTIFY_MUXER_LISTENER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "media/event/muxer_listener.h"

namespace dash_packager {
class MpdNotifier;
}  // namespace dash_packager

namespace media {
namespace event {

class VodMpdNotifyMuxerListener : public MuxerListener {
 public:
  // |mpd_notifier| must be initialized, i.e mpd_notifier->Init() must be
  // called.
  VodMpdNotifyMuxerListener(dash_packager::MpdNotifier* mpd_notifier);
  virtual ~VodMpdNotifyMuxerListener();

  // MuxerListener implementation.
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const std::vector<StreamInfo*>& stream_infos,
                            uint32 time_scale,
                            ContainerType container_type) OVERRIDE;

  virtual void OnMediaEnd(const std::vector<StreamInfo*>& stream_infos,
                          bool has_init_range,
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

 private:
  dash_packager::MpdNotifier* const mpd_notifier_;

  DISALLOW_COPY_AND_ASSIGN(VodMpdNotifyMuxerListener);
};

}  // namespace event
}  // namespace media

#endif  // MEDIA_EVENT_VOD_MPD_NOTIFY_MUXER_LISTENER_H_
