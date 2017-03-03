// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CHUNKING_CHUNKING_HANDLER_
#define PACKAGER_MEDIA_CHUNKING_CHUNKING_HANDLER_

#include <atomic>

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

struct ChunkingOptions {
  /// Segment duration in seconds.
  double segment_duration_in_seconds = 0;

  /// Subsegment duration in seconds. Should not be larger than the segment
  /// duration.
  double subsegment_duration_in_seconds = 0;

  /// Force segments to begin with stream access points. Actual segment duration
  /// may not be exactly what is specified by segment_duration.
  bool segment_sap_aligned = true;

  /// Force subsegments to begin with stream access points. Actual subsegment
  /// duration may not be exactly what is specified by subsegment_duration.
  /// Setting to true implies that segment_sap_aligned is true as well.
  bool subsegment_sap_aligned = true;
};

/// ChunkingHandler splits the samples into segments / subsegments based on the
/// specified chunking options.
/// This handler is a multi-in multi-out handler. If more than one input is
/// provided, there should be one and only one video stream; also, all inputs
/// should come from the same thread and are synchronized.
/// There can be multiple chunking handler running in different threads or even
/// different processes, we use the "consistent chunking algorithm" to make sure
/// the chunks in different streams are aligned without explicit communcating
/// with each other - which is not efficient and often difficult.
///
/// Consistent Chunking Algorithm:
///  1. Find the consistent chunkable boundary
///  Let the timestamps for video frames be (t1, t2, t3, ...). Then a
///  consistent chunkable boundary is simply the first chunkable boundary after
///  (tk / N) != (tk-1 / N), where '/' denotes integer division, and N is the
///  intended chunk duration.
///  2. Chunk only at the consistent chunkable boundary
///
/// This algorithm will make sure the chunks from different video streams are
/// aligned if they have aligned GoPs. However, this algorithm will only work
/// for video streams. To be able to chunk non video streams at similar
/// positions as video streams, ChunkingHandler is designed to accept one video
/// input and multiple non video inputs, the non video inputs are chunked when
/// the video input is chunked. If the inputs are synchronized - which is true
/// if the inputs come from the same demuxer, the video and non video chunks
/// are aligned.
class ChunkingHandler : public MediaHandler {
 public:
  explicit ChunkingHandler(const ChunkingOptions& chunking_options);
  ~ChunkingHandler() override;

 protected:
  /// @name MediaHandler implementation overrides.
  /// @{
  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;
  /// @}

 private:
  friend class ChunkingHandlerTest;

  ChunkingHandler(const ChunkingHandler&) = delete;
  ChunkingHandler& operator=(const ChunkingHandler&) = delete;

  // Processes media sample and apply chunking if needed.
  Status ProcessMediaSample(const MediaSample* sample);

  // Dispatch cached non main stream samples before |timestamp_threshold|.
  Status DispatchNonMainSamples(int64_t timestamp_threshold);

  // The (sub)segments are aligned and dispatched together.
  Status DispatchSegmentInfoForAllStreams();
  Status DispatchSubsegmentInfoForAllStreams();

  const ChunkingOptions chunking_options_;

  // The inputs are expected to come from the same thread.
  std::atomic<int64_t> thread_id_;

  // The video stream is the main stream; if there is only one stream, it is the
  // main stream. The chunking is based on the main stream.
  const size_t kInvalidStreamIndex = static_cast<size_t>(-1);
  size_t main_stream_index_ = kInvalidStreamIndex;
  // Segment and subsegment duration in main stream's time scale.
  int64_t segment_duration_ = 0;
  int64_t subsegment_duration_ = 0;

  // The streams are expected to be synchronized. Cache non main (video) stream
  // samples so we can determine whether the next segment should include these
  // samples. The samples will be dispatched after seeing the next main stream
  // sample.
  std::deque<std::unique_ptr<StreamData>> non_main_samples_;

  // Current segment index, useful to determine where to do chunking.
  int64_t current_segment_index_ = -1;
  // Current subsegment index, useful to determine where to do chunking.
  int64_t current_subsegment_index_ = -1;

  std::vector<std::shared_ptr<SegmentInfo>> segment_info_;
  std::vector<std::shared_ptr<SegmentInfo>> subsegment_info_;
  std::vector<uint32_t> time_scales_;
  // The end timestamp of the last dispatched sample.
  std::vector<int64_t> last_sample_end_timestamps_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_CHUNKING_HANDLER_
