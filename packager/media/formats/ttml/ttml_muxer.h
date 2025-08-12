// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_TTML_TTML_MUXER_H_
#define PACKAGER_MEDIA_FORMATS_TTML_TTML_MUXER_H_

#include <cstdint>

#include <packager/media/base/text_muxer.h>
#include <packager/media/formats/ttml/ttml_generator.h>

namespace shaka {
namespace media {
namespace ttml {

class TtmlMuxer : public TextMuxer {
 public:
  explicit TtmlMuxer(const MuxerOptions& options);
  ~TtmlMuxer() override;

 private:
  Status InitializeStream(TextStreamInfo* stream) override;
  Status AddTextSampleInternal(const TextSample& sample) override;
  Status WriteToFile(const std::string& filename, uint64_t* size) override;

  TtmlGenerator generator_;
};

}  // namespace ttml
}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_TTML_TTML_MUXER_H_
