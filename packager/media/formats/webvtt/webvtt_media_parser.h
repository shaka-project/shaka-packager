// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MEDIA_PARSER_H_
#define PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MEDIA_PARSER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "packager/base/compiler_specific.h"
#include "packager/media/base/media_parser.h"
#include "packager/media/formats/webvtt/cue.h"
#include "packager/media/formats/webvtt/webvtt_sample_converter.h"

namespace shaka {
namespace media {

// WebVTT parser.
// The input may not be encrypted so decryption_key_source is ignored.
class WebVttMediaParser : public MediaParser {
 public:
  WebVttMediaParser();
  ~WebVttMediaParser() override;

  /// @name MediaParser implementation overrides.
  /// @{
  void Init(const InitCB& init_cb,
            const NewSampleCB& new_sample_cb,
            KeySource* decryption_key_source) override;
  bool Flush() override WARN_UNUSED_RESULT;
  bool Parse(const uint8_t* buf, int size) override WARN_UNUSED_RESULT;
  /// @}

  void InjectWebVttSampleConvertForTesting(
      std::unique_ptr<WebVttSampleConverter> converter);

 private:
  enum WebVttReadingState {
    kHeader,
    kMetadata,
    kCueIdentifierOrTimingOrComment,
    kCueTiming,
    kCuePayload,
    kComment,
    kParseError,
  };

  // Sends current cue to sample converter, and dispatches any ready samples to
  // the callback.
  // current_cue_ is always cleared.
  bool ProcessCurrentCue(bool flush);

  InitCB init_cb_;
  NewSampleCB new_sample_cb_;

  // All the unprocessed data passed to this parser.
  std::string data_;

  // The WEBVTT text + metadata header (global settings) for this webvtt.
  // One element per line.
  std::vector<std::string> header_;

  // This is set to what the parser is expecting. For example, if the parse is
  // expecting a kCueTiming, then the next line that it parses should be a
  // WebVTT timing line or an empty line.
  WebVttReadingState state_;

  Cue current_cue_;

  std::unique_ptr<WebVttSampleConverter> sample_converter_;

  DISALLOW_COPY_AND_ASSIGN(WebVttMediaParser);
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBVTT_WEBVTT_MEDIA_PARSER_H_
