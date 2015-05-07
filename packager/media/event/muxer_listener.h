// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Event handler for events fired by Muxer.

#ifndef MEDIA_EVENT_MUXER_LISTENER_H_
#define MEDIA_EVENT_MUXER_LISTENER_H_

#include <stdint.h>

#include <vector>

namespace edash_packager {
namespace media {

class StreamInfo;
struct MuxerOptions;

class MuxerListener {
 public:
  enum ContainerType {
    kContainerUnknown = 0,
    kContainerMp4,
    kContainerMpeg2ts,
    kContainerWebM
  };

  virtual ~MuxerListener() {};

  // Called when muxing starts. This event happens before any other events.
  // For MPEG DASH Live profile, the initialization segment information is
  // available from StreamInfo.
  // |time_scale| is a reference time scale regardless of the time scale(s)
  // specified in |stream_infos|.
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const std::vector<StreamInfo*>& stream_infos,
                            uint32_t time_scale,
                            ContainerType container_type,
                            bool is_encrypted) = 0;

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
  virtual void OnMediaEnd(bool has_init_range,
                          uint64_t init_range_start,
                          uint64_t init_range_end,
                          bool has_index_range,
                          uint64_t index_range_start,
                          uint64_t index_range_end,
                          float duration_seconds,
                          uint64_t file_size) = 0;

  // Called when a segment has been muxed and the file has been written.
  // Note: For video on demand (VOD), this would be for subsegments.
  // |start_time| and |duration| are relative to time scale specified
  // OnMediaStart().
  // |segment_file_size| in bytes.
  virtual void OnNewSegment(uint64_t start_time,
                            uint64_t duration,
                            uint64_t segment_file_size) = 0;

 protected:
  MuxerListener() {};
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_EVENT_MUXER_LISTENER_H_
