// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_TEXT_HANDLER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_TEXT_HANDLER_H_

#include <stdint.h>

#include <vector>

#include "packager/media/base/media_handler.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/event/muxer_listener.h"
#include "packager/media/formats/webvtt/webvtt_file_buffer.h"

namespace shaka {
namespace media {

class WebVttTextOutputHandler : public MediaHandler {
 public:
  WebVttTextOutputHandler(const MuxerOptions& muxer_options,
                          std::unique_ptr<MuxerListener> muxer_listener);
  virtual ~WebVttTextOutputHandler() = default;

 private:
  WebVttTextOutputHandler(const WebVttTextOutputHandler&) = delete;
  WebVttTextOutputHandler& operator=(const WebVttTextOutputHandler&) = delete;

  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  Status OnStreamInfo(const StreamInfo& info);
  Status OnSegmentInfo(const SegmentInfo& info);
  Status OnCueEvent(const CueEvent& event);
  void OnTextSample(const TextSample& sample);

  Status OnSegmentEnded();

  void GoToNextSegment(uint64_t start_time_ms);

  const MuxerOptions muxer_options_;
  std::unique_ptr<MuxerListener> muxer_listener_;

  // Sum together all segment durations so we know how long the stream is.
  uint64_t total_duration_ms_ = 0;
  uint32_t segment_index_ = 0;

  std::unique_ptr<WebVttFileBuffer> buffer_;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_TEXT_HANDLER_H_
