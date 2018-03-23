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

class TextChunker : public MediaHandler {
 public:
  explicit TextChunker(int64_t segment_duration_ms);

 private:
  TextChunker(const TextChunker&) = delete;
  TextChunker& operator=(const TextChunker&) = delete;

  Status InitializeInternal() override;

  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  Status OnStreamInfo(std::shared_ptr<const StreamInfo> info);
  Status OnCueEvent(std::shared_ptr<const CueEvent> cue);
  Status OnTextSample(std::shared_ptr<const TextSample> sample);

  Status EndSegment(int64_t segment_actual_end_ms);
  void StartNewSegment(int64_t start_ms);

  int64_t segment_duration_ms_;

  // The segment that we are currently outputting samples for. The segment
  // will end once a new sample with start time greater or equal to the
  // segment's end time arrives.
  int64_t segment_start_ms_;
  int64_t segment_expected_end_ms_;
  std::list<std::shared_ptr<const TextSample>> segment_samples_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_CHUNKING_TEXT_CHUNKER_H_
