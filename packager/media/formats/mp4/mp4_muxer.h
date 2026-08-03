// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_MP4_MP4_MUXER_H_
#define PACKAGER_MEDIA_FORMATS_MP4_MP4_MUXER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/media_handler.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/muxer.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/range.h>
#include <packager/status.h>

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
///
/// More than one input stream may be connected, in which case each input
/// stream becomes a track of a single multiplexed output file.
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
  Status OnStreamEnded(size_t stream_id) override;

  Status DelayInitializeMuxer();
  Status UpdateEditListOffsetFromSample(size_t stream_id,
                                        const MediaSample& sample);

  // Initialization is delayed until the first sample of every track has been
  // seen, because each track's edit list is derived from its own first sample.
  // Returns true once no track can still contribute an edit list offset, i.e.
  // every track has either produced a sample or been flushed.
  bool ReadyToInitialize() const;

  // Initialize the muxer and hand the samples buffered while waiting for
  // ReadyToInitialize() over to the segmenter, in arrival order.
  Status InitializeAndFlushPendingSamples();

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

  bool to_be_initialized_ = true;
  // Edit list offset of each track, in that track's own time scale, indexed by
  // input stream index. Empty until the track's first sample is seen.
  std::vector<std::optional<int64_t>> edit_list_offsets_;
  // Input stream indices that were flushed without producing any sample.
  std::set<size_t> ended_streams_;
  // Samples received before the muxer could be initialized, in arrival order.
  std::vector<std::pair<size_t, std::shared_ptr<MediaSample>>> pending_samples_;

  std::unique_ptr<Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(MP4Muxer);
};

}  // namespace mp4
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_MP4_MP4_MUXER_H_
