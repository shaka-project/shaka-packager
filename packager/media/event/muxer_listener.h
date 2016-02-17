// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Event handler for events fired by Muxer.

// TODO(rkuroiwa): Document using doxygen style comments.

#ifndef MEDIA_EVENT_MUXER_LISTENER_H_
#define MEDIA_EVENT_MUXER_LISTENER_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace edash_packager {
namespace media {

struct MuxerOptions;
class ProtectionSystemSpecificInfo;
class StreamInfo;

/// MuxerListener is an event handler that can be registered to a muxer.
/// A MuxerListener cannot be shared amongst muxer instances, in other words,
/// every muxer instance either owns a unique MuxerListener instance.
/// This also assumes that there is one media stream per muxer.
class MuxerListener {
 public:
  enum ContainerType {
    kContainerUnknown = 0,
    kContainerMp4,
    kContainerMpeg2ts,
    kContainerWebM
  };

  virtual ~MuxerListener() {};

  // Called when the media's encryption information is ready. This should be
  // called before OnMediaStart(), if the media is encrypted.
  // All the parameters may be empty just to notify that the media is encrypted.
  // |is_initial_encryption_info| is true if this is the first encryption info
  // for the media.
  // In general, this flag should always be true for non-key-rotated media and
  // should be called only once.
  // |key_id| is the key ID for the media.
  // The format should be a vector of uint8_t, i.e. not (necessarily) human
  // readable hex string.
  // For ISO BMFF (MP4) media:
  // If |is_initial_encryption_info| is true then |key_id| is the default_KID in
  // 'tenc' box.
  // If |is_initial_encryption_info| is false then |key_id| is the new key ID
  // for the for the next crypto period.
  virtual void OnEncryptionInfoReady(
      bool is_initial_encryption_info,
      const std::vector<uint8_t>& key_id,
      const std::vector<ProtectionSystemSpecificInfo>& key_system_info) = 0;

  // Called when muxing starts.
  // For MPEG DASH Live profile, the initialization segment information is
  // available from StreamInfo.
  // |time_scale| is a reference time scale that overrides the time scale
  // specified in |stream_info|.
  virtual void OnMediaStart(const MuxerOptions& muxer_options,
                            const StreamInfo& stream_info,
                            uint32_t time_scale,
                            ContainerType container_type) = 0;

  /// Called when the average sample duration of the media is determined.
  /// @param sample_duration in timescale of the media.
  virtual void OnSampleDurationReady(uint32_t sample_duration) = 0;

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
