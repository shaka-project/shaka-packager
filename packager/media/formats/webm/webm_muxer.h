// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_MUXER_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_MUXER_H_

#include <packager/macros/classes.h>
#include <packager/media/base/muxer.h>

namespace shaka {
namespace media {
namespace webm {

class Segmenter;

/// Implements WebM Muxer.
class WebMMuxer : public Muxer {
 public:
  /// Create a WebMMuxer object from MuxerOptions.
  explicit WebMMuxer(const MuxerOptions& options);
  ~WebMMuxer() override;

 private:
  // Muxer implementation overrides.
  Status InitializeMuxer() override;
  Status Finalize() override;
  Status AddMediaSample(size_t stream_id, const MediaSample& sample) override;
  Status FinalizeSegment(size_t stream_id,
                         const SegmentInfo& segment_info) override;

  void FireOnMediaStartEvent();
  void FireOnMediaEndEvent();

  std::unique_ptr<Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(WebMMuxer);
};

}  // namespace webm
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_MUXER_H_
