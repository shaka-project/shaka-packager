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
/// that may appear in the text stream.
class TextPadder : public MediaHandler {
 public:
  /// Create a new text padder.
  ///
  /// |zero_start_bias_ms| is the threshold used to determine if we should
  /// assume that the stream actually starts at time zero. If the first sample
  /// comes before the |zero_start_bias_ms|, then the start will be padded as
  /// the stream is assumed to start at zero. If the first sample comes after
  /// |zero_start_bias_ms| then the start of the stream will not be padded as
  /// we cannot assume the start time of the stream.
  explicit TextPadder(int64_t zero_start_bias_ms);
  ~TextPadder() override = default;

 private:
  TextPadder(const TextPadder&) = delete;
  TextPadder& operator=(const TextPadder&) = delete;

  Status InitializeInternal() override;

  Status Process(std::unique_ptr<StreamData> data) override;
  Status OnTextSample(std::unique_ptr<StreamData> data);

  const int64_t zero_start_bias_ms_;
  // Will be set once we see our first sample. Using -1 to signal that we have
  // not seen the first sample yet.
  int64_t max_end_time_ms_ = -1;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_TEXT_PADDER_H_
