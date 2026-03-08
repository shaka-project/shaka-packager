// Copyright 2025 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CHUNKING_SEGMENT_COORDINATOR_H_
#define PACKAGER_MEDIA_CHUNKING_SEGMENT_COORDINATOR_H_

#include <optional>
#include <set>

#include <packager/media/base/media_handler.h>

namespace shaka {
namespace media {

/// SegmentCoordinator is a N-to-N media handler that coordinates segment
/// boundaries across different stream types. All streams (video, audio, text)
/// go through the same coordinator instance, similar to CueAlignmentHandler.
///
/// It receives SegmentInfo events from video/audio streams (emitted by
/// ChunkingHandler) and replicates them to registered teletext streams,
/// ensuring that teletext segments align with media segments.
///
/// This handler is designed to solve the alignment problem where teletext
/// segments use mathematical calculation (segment_start + segment_duration)
/// while video/audio use actual keyframe timestamps, causing misalignment
/// especially with:
/// - Arbitrary input timestamps (not divisible by segment duration)
/// - MPEG-TS timestamp wrap-around scenarios
/// - Sources with offset keyframe timestamps
///
/// The coordinator only replicates SegmentInfo to teletext streams (cc_index >=
/// 0). Other text formats (WebVTT files, TTML) and video/audio streams pass
/// through unchanged.
///
/// Pipeline placement (all streams go through the same coordinator instance):
///   Video/Audio → CueAligner → SegmentCoordinator → ChunkingHandler → ...
///   Teletext    → CueAligner → SegmentCoordinator → CcStreamFilter →
///   TextChunker → ...
///
/// When ChunkingHandler emits SegmentInfo, it flows back through the
/// coordinator, which then broadcasts it to all registered teletext stream
/// indices.
class SegmentCoordinator : public MediaHandler {
 public:
  SegmentCoordinator();
  ~SegmentCoordinator() override = default;

  /// Mark a stream as a teletext stream that should receive segment boundaries.
  /// This should be called during pipeline setup before processing begins.
  /// @param input_stream_index The input stream index for the teletext stream.
  void MarkAsTeletextStream(size_t input_stream_index);

 protected:
  /// @name MediaHandler implementation overrides.
  /// @{
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  /// @}

 private:
  SegmentCoordinator(const SegmentCoordinator&) = delete;
  SegmentCoordinator& operator=(const SegmentCoordinator&) = delete;

  /// Handle incoming SegmentInfo from video/audio streams and replicate to
  /// teletext streams.
  /// @param input_stream_index The input stream index that sent the
  /// SegmentInfo.
  /// @param info The segment information to process.
  /// @return Status of the operation.
  Status OnSegmentInfo(size_t input_stream_index,
                       std::shared_ptr<const SegmentInfo> info);

  /// Check if a stream index corresponds to a teletext stream.
  /// @param input_stream_index The input stream index to check.
  /// @return true if the stream is a teletext stream.
  bool IsTeletextStream(size_t input_stream_index) const;

  /// Latest segment boundary timestamp from video/audio streams.
  /// Used for logging and debugging purposes.
  int64_t latest_segment_boundary_ = 0;

  /// Set of input stream indices that are teletext streams and should receive
  /// replicated SegmentInfo from video/audio streams.
  std::set<size_t> teletext_stream_indices_;

  /// The stream index that acts as the sync source for segment boundaries.
  /// Only SegmentInfo from this stream is replicated to teletext streams.
  /// This is set to the first non-teletext stream that sends SegmentInfo,
  /// ensuring consistent alignment even when video and audio have different
  /// segment boundaries.
  std::optional<size_t> sync_source_stream_index_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_SEGMENT_COORDINATOR_H_
