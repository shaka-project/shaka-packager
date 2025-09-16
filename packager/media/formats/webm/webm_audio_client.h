// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_
#define PACKAGER_MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <packager/macros/classes.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/formats/webm/webm_parser.h>

namespace shaka {
namespace media {
class AudioDecoderConfig;

/// Helper class used to parse an Audio element inside a TrackEntry element.
class WebMAudioClient : public WebMParserClient {
 public:
  WebMAudioClient();
  ~WebMAudioClient() override;

  /// Reset this object's state so it can process a new audio track element.
  void Reset();

  /// Create an AudioStreamInfo with the parameters specified.
  /// @param track_num indicates the track number.
  /// @param codec_id is the codec identifier.
  /// @param codec_private contains codec specific data.
  /// @param seek_preroll indicates seek preroll in nanoseconds. A negative
  ///        value means that the value is not set; in this case, a default
  ///        value of 0 is used.
  /// @param codec delay indicates codec delay in nanoseconds. A negative
  ///        value means that the value is not set; in this case, a default
  ///        value of 0 is used.
  /// @param language indicates the language for the track.
  /// @param is_encrypted indicates whether the stream is encrypted.
  /// @return An AudioStreamInfo if successful.
  /// @return An empty pointer if there was unexpected values in the
  ///         provided parameters or audio track element fields.
  std::shared_ptr<AudioStreamInfo> GetAudioStreamInfo(
      int64_t track_num,
      const std::string& codec_id,
      const std::vector<uint8_t>& codec_private,
      int64_t seek_preroll,
      int64_t codec_delay,
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
}  // namespace shaka

#endif  // PACKAGER_MEDIA_FORMATS_WEBM_WEBM_AUDIO_CLIENT_H_
