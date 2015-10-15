// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_
#define MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_

#include <string>
#include <vector>

#include "packager/base/memory/scoped_ptr.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/formats/webm/webm_parser.h"

namespace edash_packager {
namespace media {
class AudioDecoderConfig;

/// Helper class used to parse an Audio element inside a TrackEntry element.
class WebMAudioClient : public WebMParserClient {
 public:
  WebMAudioClient();
  ~WebMAudioClient() override;

  /// Reset this object's state so it can process a new audio track element.
  void Reset();

  /// Create an AudioStreamInfo with the data in |track_num|, |codec_id|,
  /// |codec_private|, |is_encrypted| and the fields parsed from the last audio
  /// track element this object was used to parse.
  /// @return An AudioStreamInfo scoped_refptr if successful.
  /// @return An empty scoped_refptr if there was unexpected values in the
  ///         provided parameters or audio track element fields.
  scoped_refptr<AudioStreamInfo> GetAudioStreamInfo(
      int64_t track_num,
      const std::string& codec_id,
      const std::vector<uint8_t>& codec_private,
      const std::string& language,
      bool is_encrypted);

 private:
  // WebMParserClient implementation.
  bool OnUInt(int id, int64_t val) override;
  bool OnFloat(int id, double val) override;

  int channels_;
  double samples_per_second_;
  double output_samples_per_second_;

  DISALLOW_COPY_AND_ASSIGN(WebMAudioClient);
};

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_
