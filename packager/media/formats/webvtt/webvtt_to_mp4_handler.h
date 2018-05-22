// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MP4_CUE_HANDLER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MP4_CUE_HANDLER_H_

#include <stdint.h>

#include <list>
#include <queue>

#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

// A media handler that should come after the cue aligner and segmenter and
// should come before the muxer. This handler is to convert text samples
// to media samples so that they can be sent to a mp4 muxer.
class WebVttToMp4Handler : public MediaHandler {
 public:
  WebVttToMp4Handler() = default;
  virtual ~WebVttToMp4Handler() override = default;

 private:
  WebVttToMp4Handler(const WebVttToMp4Handler&) = delete;
  WebVttToMp4Handler& operator=(const WebVttToMp4Handler&) = delete;

  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;

  Status OnStreamInfo(std::unique_ptr<StreamData> stream_data);
  Status OnCueEvent(std::unique_ptr<StreamData> stream_data);
  Status OnSegmentInfo(std::unique_ptr<StreamData> stream_data);
  Status OnTextSample(std::unique_ptr<StreamData> stream_data);

  Status DispatchCurrentSegment(int64_t segment_start, int64_t segment_end);
  Status MergeDispatchSamples(int64_t start_in_seconds,
                              int64_t end_in_seconds,
                              const std::list<const TextSample*>& state);

  std::list<std::shared_ptr<const TextSample>> current_segment_;

  // This is the current state of the box we are writing.
  BufferWriter box_writer_;
};

}  // namespace media
}  // namespace shaka
#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MP4_CUE_HANDLER_H_
