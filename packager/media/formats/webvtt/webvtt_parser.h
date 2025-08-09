// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_PARSER_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <packager/media/base/media_parser.h>
#include <packager/media/base/text_sample.h>
#include <packager/media/base/text_stream_info.h>
#include <packager/media/formats/webvtt/text_readers.h>

namespace shaka {
namespace media {

// Used to parse a WebVTT source into Cues that will be sent downstream.
class WebVttParser : public MediaParser {
 public:
  WebVttParser();

  void Init(const InitCB& init_cb,
            const NewMediaSampleCB& new_media_sample_cb,
            const NewTextSampleCB& new_text_sample_cb,
            KeySource* decryption_key_source) override;
  bool Flush() override;
  bool Parse(const uint8_t* buf, int size) override;

 private:
  bool Parse();
  bool ParseBlock(const std::vector<std::string>& block);
  bool ParseRegion(const std::vector<std::string>& block);
  bool ParseCueWithNoId(const std::vector<std::string>& block);
  bool ParseCueWithId(const std::vector<std::string>& block);
  bool ParseCue(const std::string& id,
                const std::string* block,
                size_t block_size);

  void DispatchTextStreamInfo();

  InitCB init_cb_;
  NewTextSampleCB new_text_sample_cb_;

  BlockReader reader_;
  std::map<std::string, TextRegion> regions_;
  std::string css_styles_;
  bool saw_cue_ = false;
  bool stream_info_dispatched_ = false;
  bool initialized_ = false;
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_FORMATS_WEBVTT_WEBVTT_PARSER_H_
