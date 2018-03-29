// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_TEXT_PADDER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_TEXT_PADDER_H_

#include "packager/media/base/media_handler.h"

namespace shaka {
namespace media {

/// A media handler that will inject empty text samples to fill any gaps
/// that may appear in the text stream. A min duration can be given to
/// ensure that the stream will have samples up to the given duration.
class TextPadder : public MediaHandler {
 public:
  /// Create a new text padder that will ensure the stream's duration is
  // at least |duration_ms| long.
  explicit TextPadder(int64_t duration_ms);
  ~TextPadder() override = default;

 private:
  TextPadder(const TextPadder&) = delete;
  TextPadder& operator=(const TextPadder&) = delete;

  Status InitializeInternal() override;

  Status Process(std::unique_ptr<StreamData> data) override;
  Status OnFlushRequest(size_t index) override;
  Status OnTextSample(std::unique_ptr<StreamData> data);

  int64_t duration_ms_;
  int64_t max_end_time_ms_ = 0;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_TEXT_PADDER_H_
