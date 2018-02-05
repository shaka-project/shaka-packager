// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SEGMENTER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SEGMENTER_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "packager/media/base/media_handler.h"
#include "packager/status.h"

namespace shaka {
namespace media {

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

  uint64_t segment_duration_ms_;

  using WebVttSample = std::shared_ptr<const TextSample>;
  using WebVttSegment = std::vector<WebVttSample>;

  // Mapping of segment number to segment.
  std::map<uint64_t, WebVttSegment> segment_map_;
  uint64_t head_segment_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_SEGMENTER_H_
