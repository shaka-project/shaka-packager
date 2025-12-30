// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_
#define PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_

#include <list>

#include <packager/chunking_params.h>
#include <packager/media/base/media_handler.h>
#include <packager/media/base/timestamp_util.h>

namespace shaka {
namespace media {

// Media handler for taking a single stream of text samples and inserting
// segment info based on a fixed segment duration and on cue events. The
// only time a segment's duration will not match the fixed segment duration
// is when a cue event is seen.
class TextChunker : public MediaHandler {
 public:
  explicit TextChunker(double segment_duration_in_seconds,
                       int64_t start_segment_number);
  explicit TextChunker(double segment_duration_in_seconds,
                       int64_t start_segment_number,
                       int64_t ts_ttx_heartbeat_shift);
  explicit TextChunker(double segment_duration_in_seconds,
                       int64_t start_segment_number,
                       int64_t ts_ttx_heartbeat_shift,
                       bool use_segment_coordinator);

 private:
  TextChunker(const TextChunker&) = delete;
  TextChunker& operator=(const TextChunker&) = delete;

  Status InitializeInternal() override { return Status::OK; }

  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  Status OnStreamInfo(std::shared_ptr<const StreamInfo> info);
  Status OnCueEvent(std::shared_ptr<const CueEvent> cue);
  Status OnTextSample(std::shared_ptr<const TextSample> sample);
  Status OnSegmentInfo(std::shared_ptr<const SegmentInfo> info);

  // This does two things that should always happen together:
  //    1. Dispatch all the samples and a segment info for the time range
  //       segment_start_ to segment_start_ + duration
  //    2. Set the next segment to start at segment_start_ + duration and
  //       remove all samples that don't last into that segment.
  Status DispatchSegment(int64_t duration);

  // Creates cropped copies of ongoing cues (samples_without_end_) that span
  // into the current segment and adds them to samples_in_current_segment_.
  // This must be called before DispatchSegment to ensure ongoing cues are
  // included in the segment output.
  void AddOngoingCuesToCurrentSegment(int64_t segment_end);

  int64_t ScaleTime(double seconds) const;

  double segment_duration_in_seconds_;

  int64_t time_scale_ = -1;  // Set in OnStreamInfo

  // Time values are in scaled units.
  int64_t segment_start_ = -1;     // Set when the first sample comes in.
  int64_t segment_duration_ = -1;  // Set in OnStreamInfo.

  // Segment number that keeps monotonically increasing.
  // Set to start_segment_number in constructor.
  int64_t segment_number_ = 1;

  // A shift in PTS values for text heart beats from other MPEG-2 TS
  // elementary streams. Can be set from command line.
  int64_t ts_ttx_heartbeat_shift_ = kDefaultTtxHeartbeatShift;

  // Used to check if media heart beats are coming before text timestamps
  // This value has the shift applied and is used for warnings
  int64_t latest_media_heartbeat_time_ = -1;

  // All samples that make up the current segment. We must store the samples
  // until the segment ends because a cue event may end the segment sooner
  // than we expected.
  std::list<std::shared_ptr<const TextSample>> samples_in_current_segment_;

  // For live input which we cannot wait for sample end time since
  // it may come after the current segment is supposed to finish.
  // By storing them in this list we can retrieve them and crop them
  // to the segment interval before adding them to samples_in_current_segment_
  std::list<std::shared_ptr<const TextSample>> samples_without_end_;

  // When true, TextChunker uses SegmentInfo from SegmentCoordinator to align
  // segment boundaries with video/audio streams. When false (default for
  // non-teletext streams), uses mathematical calculation.
  bool use_segment_coordinator_ = false;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_
