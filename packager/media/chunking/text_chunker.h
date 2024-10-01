// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_
#define PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_

#include <list>

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

// Media handler for taking a single stream of text samples and inserting
// segment info based on a fixed segment duration and on cue events. The
// only time a segment's duration will not match the fixed segment duration
// is when a cue event is seen.
class TextChunker : public MediaHandler {
 public:
  explicit TextChunker(double segment_duration_in_seconds);
  explicit TextChunker(double segment_duration_in_seconds,
                       int64_t end_segment_number);

 private:
  TextChunker(const TextChunker&) = delete;
  TextChunker& operator=(const TextChunker&) = delete;

  Status InitializeInternal() override { return Status::OK; }

  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  Status OnStreamInfo(std::shared_ptr<const StreamInfo> info);
  Status OnCueEvent(std::shared_ptr<const CueEvent> cue);
  Status OnTextSample(std::shared_ptr<const TextSample> sample);

  // This does two things that should always happen together:
  //    1. Dispatch all the samples and a segment info for the time range
  //       segment_start_ to segment_start_ + duration
  //    2. Set the next segment to start at segment_start_ + duration and
  //       remove all samples that don't last into that segment.
  Status DispatchSegment(int64_t duration);

  int64_t ScaleTime(double seconds) const;

  double segment_duration_in_seconds_;

  int64_t time_scale_ = -1;  // Set in OnStreamInfo

  // Time values are in scaled units.
  int64_t segment_start_ = -1;     // Set when the first sample comes in.
  int64_t segment_duration_ = -1;  // Set in OnStreamInfo.

  // A shift in PTS values for text heart beats from other MPEG-2 TS
  // elementary streams. Can be set from command line.
  int64_t ts_text_trigger_shift_ = 180000;

  // Used to check if media heart beats are coming before text timestamps
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
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_
