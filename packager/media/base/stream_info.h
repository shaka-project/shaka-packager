// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_BASE_STREAM_INFO_H_
#define MEDIA_BASE_STREAM_INFO_H_

#include <string>
#include <vector>

#include "packager/media/base/encryption_config.h"

namespace shaka {
namespace media {

enum StreamType {
  kStreamUnknown = 0,
  kStreamAudio,
  kStreamVideo,
  kStreamText,
};

enum Codec {
  kUnknownCodec = 0,

  kCodecVideo = 100,
  kCodecH264 = kCodecVideo,
  kCodecHEV1,
  kCodecHVC1,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
  kCodecVP9,
  kCodecVP10,
  kCodecVideoMaxPlusOne,

  kCodecAudio = 200,
  kCodecAAC = kCodecAudio,
  kCodecAC3,
  kCodecDTSC,
  kCodecDTSE,
  kCodecDTSH,
  kCodecDTSL,
  kCodecDTSM,
  kCodecDTSP,
  kCodecEAC3,
  kCodecOpus,
  kCodecVorbis,
  kCodecAudioMaxPlusOne,

  kCodecText = 300,
  kCodecWebVtt = kCodecText,
};

/// Abstract class holds stream information.
class StreamInfo {
 public:
  StreamInfo(StreamType stream_type, int track_id, uint32_t time_scale,
             uint64_t duration, Codec codec, const std::string& codec_string,
             const uint8_t* codec_config, size_t codec_config_size,
             const std::string& language, bool is_encrypted);

  virtual ~StreamInfo();

  /// @return true if this object has appropriate configuration values, false
  ///         otherwise.
  virtual bool IsValidConfig() const = 0;

  /// @return A human-readable string describing the stream info.
  virtual std::string ToString() const;

  StreamType stream_type() const { return stream_type_; }
  uint32_t track_id() const { return track_id_; }
  uint32_t time_scale() const { return time_scale_; }
  uint64_t duration() const { return duration_; }
  Codec codec() const { return codec_; }
  const std::string& codec_string() const { return codec_string_; }
  const std::vector<uint8_t>& codec_config() const { return codec_config_; }
  const std::string& language() const { return language_; }
  bool is_encrypted() const { return is_encrypted_; }
  const EncryptionConfig& encryption_config() const {
    return encryption_config_;
  }

  void set_duration(uint64_t duration) { duration_ = duration; }
  void set_codec(Codec codec) { codec_ = codec; }
  void set_codec_config(const std::vector<uint8_t>& data) { codec_config_ = data; }
  void set_codec_string(const std::string& codec_string) {
    codec_string_ = codec_string;
  }
  void set_language(const std::string& language) { language_ = language; }
  void set_is_encrypted(bool is_encrypted) { is_encrypted_ = is_encrypted; }
  void set_encryption_config(const EncryptionConfig& encryption_config) {
    encryption_config_ = encryption_config;
  }

 private:
  // Whether the stream is Audio or Video.
  StreamType stream_type_;
  uint32_t track_id_;
  // The actual time is calculated as time / time_scale_ in seconds.
  uint32_t time_scale_;
  // Duration base on time_scale.
  uint64_t duration_;
  Codec codec_;
  std::string codec_string_;
  std::string language_;
  // Whether the stream is potentially encrypted.
  // Note that in a potentially encrypted stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted_;
  EncryptionConfig encryption_config_;
  // Optional byte data required for some audio/video decoders such as Vorbis
  // codebooks.
  std::vector<uint8_t> codec_config_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // MEDIA_BASE_STREAM_INFO_H_
