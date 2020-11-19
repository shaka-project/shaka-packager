// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MUXER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MUXER_H_

#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/muxer.h"
#include "packager/media/formats/webvtt/webvtt_file_buffer.h"

namespace shaka {
namespace media {
namespace webvtt {

/// Implements WebVtt Muxer.
class WebVttMuxer : public Muxer {
 public:
  /// Create a WebMMuxer object from MuxerOptions.
  explicit WebVttMuxer(const MuxerOptions& options);
  ~WebVttMuxer() override;

 private:
  // Muxer implementation overrides.
  Status InitializeMuxer() override;
  Status Finalize() override;
  Status AddTextSample(size_t stream_id, const TextSample& sample) override;
  Status FinalizeSegment(size_t stream_id,
                         const SegmentInfo& segment_info) override;

  Status WriteToFile(const std::string& filename, uint64_t* size);

  DISALLOW_COPY_AND_ASSIGN(WebVttMuxer);

  std::unique_ptr<WebVttFileBuffer> buffer_;
  uint64_t total_duration_ms_ = 0;
  uint64_t last_cue_ms_ = 0;
  uint32_t segment_index_ = 0;
};

}  // namespace webvtt
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MUXER_H_
