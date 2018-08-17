// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_PARSER_H_

#include <stdint.h>

#include <vector>

#include "packager/media/formats/webvtt/text_readers.h"
#include "packager/media/origin/origin_handler.h"

namespace shaka {
namespace media {

// Used to parse a WebVTT source into Cues that will be sent downstream.
class WebVttParser : public OriginHandler {
 public:
  WebVttParser(std::unique_ptr<FileReader> source, const std::string& language);

  Status Run() override;
  void Cancel() override;

 private:
  WebVttParser(const WebVttParser&) = delete;
  WebVttParser& operator=(const WebVttParser&) = delete;

  Status InitializeInternal() override;
  bool ValidateOutputStreamIndex(size_t stream_index) const override;

  bool Parse();
  bool ParseCueWithNoId(const std::vector<std::string>& block);
  bool ParseCueWithId(const std::vector<std::string>& block);
  Status ParseCue(const std::string& id,
                  const std::string* block,
                  size_t block_size);

  Status DispatchTextStreamInfo();

  BlockReader reader_;
  std::string language_;
  std::string style_region_config_;
  bool stream_info_dispatched_ = false;
  bool keep_reading_ = true;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_WEBVTT_PARSER_H_
