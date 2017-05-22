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

namespace shaka {
namespace media {

// WebVttOutputHandler is the base class for all WebVtt text output handlers.
// It handles taking in the samples and writing the text out, but relies on
// sub classes to handle the logic of when and where to write the information.
class WebVttOutputHandler : public MediaHandler {
 public:
  WebVttOutputHandler() = default;
  virtual ~WebVttOutputHandler() = default;

 protected:
  virtual Status OnStreamInfo(const StreamInfo& info) = 0;
  virtual Status OnSegmentInfo(const SegmentInfo& info) = 0;
  virtual Status OnTextSample(const TextSample& sample) = 0;
  virtual Status OnStreamEnd() = 0;

  // Top level functions for output. These functions should be used by
  // subclasses to write to files.
  void WriteCue(const std::string& id,
                uint64_t start,
                uint64_t end,
                const std::string& settings,
                const std::string& payload);
  // Writes the current state of the current segment to disk. This will
  // reset the internal state and set it up for the next segment.
  Status WriteSegmentToFile(const std::string& filename);

 private:
  WebVttOutputHandler(const WebVttOutputHandler&) = delete;
  WebVttOutputHandler& operator=(const WebVttOutputHandler&) = delete;

  Status InitializeInternal() override;
  Status Process(std::unique_ptr<StreamData> stream_data) override;
  Status OnFlushRequest(size_t input_stream_index) override;

  // A buffer of characters waiting to be written to a file.
  std::string buffer_;
};

// This WebVttt output handler should only be used when the source WebVTT
// content needs to be segmented across multiple files.
class WebVttSegmentedOutputHandler : public WebVttOutputHandler {
 public:
  WebVttSegmentedOutputHandler(const MuxerOptions& muxer_options,
                               std::unique_ptr<MuxerListener> muxer_listener);

 private:
  Status OnStreamInfo(const StreamInfo& info) override;
  Status OnSegmentInfo(const SegmentInfo& info) override;
  Status OnTextSample(const TextSample& sample) override;
  Status OnStreamEnd() override;

  Status OnSegmentEnded();

  void GoToNextSegment(uint64_t start_time_ms);

  const MuxerOptions muxer_options_;
  std::unique_ptr<MuxerListener> muxer_listener_;

  // Sum together all segment durations so we know how long the stream is.
  uint64_t total_duration_ms_ = 0;
  uint32_t segment_index_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_TEXT_HANDLER_H_
