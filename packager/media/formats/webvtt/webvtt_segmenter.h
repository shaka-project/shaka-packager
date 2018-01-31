// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SEGMENTER_H_

#include <stdint.h>

#include <list>
#include <queue>

#include "packager/media/base/media_handler.h"
#include "packager/status.h"

namespace shaka {
namespace media {

// Because a text sample can be in multiple segments, this struct
// allows us to associate a segment with a sample. This allows us
// to easily sort samples base on segment then time.
struct WebVttSegmentedTextSample {
  uint64_t segment = 0;
  std::shared_ptr<const TextSample> sample;
};

class WebVttSegmentedTextSampleCompare {
 public:
  bool operator()(const WebVttSegmentedTextSample& left,
                  const WebVttSegmentedTextSample& right) const {
    // If the samples are in the same segment, then the start time is the
    // only way to order the two segments.
    if (left.segment == right.segment) {
      return left.sample->start_time() > right.sample->start_time();
    }

    // Time will not matter as the samples are not in the same segment.
    return left.segment > right.segment;
  }
};

class WebVttSegmenter : public MediaHandler {
 public:
  explicit WebVttSegmenter(uint64_t segment_duration_ms);

 protected:
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

 private:
  WebVttSegmenter(const WebVttSegmenter&) = delete;
  WebVttSegmenter& operator=(const WebVttSegmenter&) = delete;

  Status InitializeInternal() override;

  Status OnTextSample(std::shared_ptr<const TextSample> sample);

  Status OnSegmentEnd(uint64_t segment);

  uint64_t current_segment_ = 0;
  uint64_t segment_duration_ms_;
  std::priority_queue<WebVttSegmentedTextSample,
                      std::vector<WebVttSegmentedTextSample>,
                      WebVttSegmentedTextSampleCompare>
      samples_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SEGMENTER_H_
