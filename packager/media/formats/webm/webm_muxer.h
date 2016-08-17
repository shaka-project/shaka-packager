// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FORMATS_WEBM_WEBM_MUXER_H_
#define MEDIA_FORMATS_WEBM_WEBM_MUXER_H_

#include "packager/media/base/muxer.h"

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
  Status Initialize() override;
  Status Finalize() override;
  Status DoAddSample(const MediaStream* stream,
                     scoped_refptr<MediaSample> sample) override;

  void FireOnMediaStartEvent();
  void FireOnMediaEndEvent();

  std::unique_ptr<Segmenter> segmenter_;

  DISALLOW_COPY_AND_ASSIGN(WebMMuxer);
};

}  // namespace webm
}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBM_WEBM_MUXER_H_
