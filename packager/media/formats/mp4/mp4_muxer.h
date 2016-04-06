// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_MP4_MP4_MUXER_H_
#define MEDIA_FORMATS_MP4_MP4_MUXER_H_

#include <vector>

#include "packager/media/base/muxer.h"

namespace edash_packager {
namespace media {

class AudioStreamInfo;
class StreamInfo;
class VideoStreamInfo;

namespace mp4 {

class Segmenter;

struct ProtectionSchemeInfo;
struct Track;

/// Implements MP4 Muxer for ISO-BMFF. Please refer to ISO/IEC 14496-12: ISO
/// base media file format for details.
class MP4Muxer : public Muxer {
 public:
  /// Create a MP4Muxer object from MuxerOptions.
  explicit MP4Muxer(const MuxerOptions& options);
  ~MP4Muxer() override;

 private:
  // Muxer implementation overrides.
  Status Initialize() override;
  Status Finalize() override;
  Status DoAddSample(const MediaStream* stream,
                     scoped_refptr<MediaSample> sample) override;

  // Generate Audio/Video Track box.
  void InitializeTrak(const StreamInfo* info, Track* trak);
  void GenerateAudioTrak(const AudioStreamInfo* audio_info,
                         Track* trak,
                         uint32_t track_id);
  void GenerateVideoTrak(const VideoStreamInfo* video_info,
                         Track* trak,
                         uint32_t track_id);

  // Gets |start| and |end| initialization range. Returns true if there is an
  // init range and sets start-end byte-range-spec specified in RFC2616.
  bool GetInitRangeStartAndEnd(uint32_t* start, uint32_t* end);

  // Gets |start| and |end| index range. Returns true if there is an index range
  // and sets start-end byte-range-spec specified in RFC2616.
  bool GetIndexRangeStartAndEnd(uint32_t* start, uint32_t* end);

  // Fire events if there are no errors and Muxer::muxer_listener() is not NULL.
  void FireOnMediaStartEvent();
  void FireOnMediaEndEvent();

  // Get time in seconds since midnight, Jan. 1, 1904, in UTC Time.
  uint64_t IsoTimeNow();

  scoped_ptr<Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(MP4Muxer);
};

}  // namespace mp4
}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_MP4_MP4_MUXER_H_
