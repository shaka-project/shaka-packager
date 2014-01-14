// Event handler for events fired by Muxer.
#ifndef MEDIA_EVENT_MUXER_LISTENER_H_
#define MEDIA_EVENT_MUXER_LISTENER_H_

#include <vector>

#include "base/basictypes.h"

namespace media {

class MuxerOptions;
class StreamInfo;

namespace event {

// TODO(rkuroiwa): Need a solution to report a problem to the user. One idea is
// to add GetStatus() method somewhere (maybe in MuxerListener, maybe not).
class MuxerListener {
 public:
  MuxerListener() {};
  virtual ~MuxerListener() {};

  // Called when muxing starts. This event happens before any other events.
  // For MPEG DASH Live profile, the initialization segment information is
  // available from StreamInfo.
  // |time_scale| is a reference time scale regardless of the time scale(s)
  // specified in |stream_infos|.
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const std::vector<StreamInfo*>& stream_infos,
                            uint32 time_scale) = 0;

  // Called when all files are written out and the muxer object does not output
  // any more files.
  // Note: This event might not be very interesting to MPEG DASH Live profile.
  // |init_range_{start,end}| is the byte range of initialization segment, in
  // the media file. If |has_init_range| is false, these values are ignored.
  // |index_range_{start,end}| is the byte range of segment index, in the media
  // file. If |has_index_range| is false, these values are ignored.
  // Both ranges are inclusive.
  // Media length of |duration_seconds|.
  // |file_size| of the media in bytes.
  virtual void OnMediaEnd(const std::vector<StreamInfo*>& stream_infos,
                          bool has_init_range,
                          uint64 init_range_start,
                          uint64 init_range_end,
                          bool has_index_range,
                          uint64 index_range_start,
                          uint64 index_range_end,
                          float duration_seconds,
                          uint64 file_size) = 0;

  // Called when a segment has been muxed and the file has been written.
  // Note: For video on demand (VOD), this would be for subsegments.
  // |start_time| and |duration| are relative to time scale specified
  // OnMediaStart().
  // |segment_file_size| in bytes.
  virtual void OnNewSegment(uint64 start_time,
                            uint64 duration,
                            uint64 segment_file_size) = 0;
};

}  // namespace event
}  // namespace media

#endif  // MEDIA_EVENT_MUXER_LISTENER_H_
