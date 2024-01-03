// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_MP4_MUXER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_MP4_MUXER_H_

#include <optional>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/muxer.h>

namespace shaka {
namespace media {

class AudioStreamInfo;
class StreamInfo;
class TextStreamInfo;
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
  Status InitializeMuxer() override;
  Status Finalize() override;
  Status AddMediaSample(size_t stream_id, const MediaSample& sample) override;
  Status FinalizeSegment(size_t stream_id,
                         const SegmentInfo& segment_info) override;

  Status DelayInitializeMuxer();
  Status UpdateEditListOffsetFromSample(const MediaSample& sample);

  // Generate Audio/Video Track box.
  void InitializeTrak(const StreamInfo* info, Track* trak);
  bool GenerateAudioTrak(const AudioStreamInfo* audio_info, Track* trak);
  bool GenerateVideoTrak(const VideoStreamInfo* video_info, Track* trak);
  bool GenerateTextTrak(const TextStreamInfo* video_info, Track* trak);

  // Gets |start| and |end| initialization range. Returns true if there is an
  // init range and sets start-end byte-range-spec specified in RFC2616.
  std::optional<Range> GetInitRangeStartAndEnd();

  // Gets |start| and |end| index range. Returns true if there is an index range
  // and sets start-end byte-range-spec specified in RFC2616.
  std::optional<Range> GetIndexRangeStartAndEnd();

  // Fire events if there are no errors and Muxer::muxer_listener() is not NULL.
  void FireOnMediaStartEvent();
  void FireOnMediaEndEvent();

  // Get time in seconds since midnight, Jan. 1, 1904, in UTC Time.
  uint64_t IsoTimeNow();

  // Assumes single stream (multiplexed a/v not supported yet).
  bool to_be_initialized_ = true;
  std::optional<int64_t> edit_list_offset_;

  std::unique_ptr<Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(MP4Muxer);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MP4_MUXER_H_
