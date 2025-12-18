// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MEDIA_BASE_STREAM_INFO_H_
#define PACKAGER_MEDIA_BASE_STREAM_INFO_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <packager/media/base/encryption_config.h>

namespace shaka {
namespace media {

enum StreamType {
  kStreamUnknown = 0,
  kStreamAudio,
  kStreamVideo,
  kStreamText,
};

std::string StreamTypeToString(StreamType type);

enum Codec {
  kUnknownCodec = 0,

  kCodecVideo = 100,
  kCodecAV1 = kCodecVideo,
  kCodecH264,
  kCodecH265,
  kCodecH265DolbyVision,
  kCodecVP8,
  kCodecVP9,
  kCodecVideoMaxPlusOne,

  kCodecAudio = 200,
  kCodecAAC = kCodecAudio,
  kCodecAC3,
  kCodecAC4,
  kCodecALAC,
  // TODO(kqyang): Use kCodecDTS and a kDtsStreamFormat for the various DTS
  // streams.
  kCodecDTSC,
  kCodecDTSE,
  kCodecDTSH,
  kCodecDTSL,
  kCodecDTSM,
  kCodecDTSP,
  kCodecDTSX,
  kCodecEAC3,
  kCodecFlac,
  kCodecIAMF,
  kCodecOpus,
  kCodecPcm,
  kCodecVorbis,
  kCodecMP3,
  kCodecMha1,
  kCodecMhm1,
  kCodecAudioMaxPlusOne,

  kCodecText = 300,
  kCodecWebVtt = kCodecText,
  kCodecTtml,
};

/// Abstract class holds stream information.
class StreamInfo {
 public:
  StreamInfo() = default;

  StreamInfo(StreamType stream_type,
             int track_id,
             int32_t time_scale,
             int64_t duration,
             Codec codec,
             const std::string& codec_string,
             const uint8_t* codec_config,
             size_t codec_config_size,
             const std::string& language,
             bool is_encrypted);

  virtual ~StreamInfo();

  /// @return true if this object has appropriate configuration values, false
  ///         otherwise.
  virtual bool IsValidConfig() const = 0;

  /// @return A human-readable string describing the stream info.
  virtual std::string ToString() const;

  /// @return A new copy of this stream info. The copy will be of the same
  ///         type as the original. This should be used when a copy is needed
  ///         without explicitly knowing the stream info type.
  virtual std::unique_ptr<StreamInfo> Clone() const = 0;

  StreamType stream_type() const { return stream_type_; }
  uint32_t track_id() const { return track_id_; }
  int32_t time_scale() const { return time_scale_; }
  int64_t duration() const { return duration_; }
  Codec codec() const { return codec_; }
  const std::string& codec_string() const { return codec_string_; }
  const std::vector<uint8_t>& codec_config() const { return codec_config_; }
  const std::vector<uint8_t>& layered_codec_config() const {
    return layered_codec_config_;
  }
  const std::string& language() const { return language_; }
  bool is_encrypted() const { return is_encrypted_; }
  bool has_clear_lead() const { return has_clear_lead_; }
  const EncryptionConfig& encryption_config() const {
    return encryption_config_;
  }

  void set_duration(int64_t duration) { duration_ = duration; }
  void set_codec(Codec codec) { codec_ = codec; }
  void set_codec_config(const std::vector<uint8_t>& data) {
    codec_config_ = data;
  }

  void set_layered_codec_config(const std::vector<uint8_t>& data) {
    layered_codec_config_ = data;
  }

  void set_codec_string(const std::string& codec_string) {
    codec_string_ = codec_string;
  }
  void set_language(const std::string& language) { language_ = language; }
  void set_is_encrypted(bool is_encrypted) { is_encrypted_ = is_encrypted; }
  void set_has_clear_lead(bool has_clear_lead) {
    has_clear_lead_ = has_clear_lead;
  }
  void set_encryption_config(const EncryptionConfig& encryption_config) {
    encryption_config_ = encryption_config;
  }

 private:
  // Whether the stream is Audio or Video.
  StreamType stream_type_;
  uint32_t track_id_;
  // The actual time is calculated as time / time_scale_ in seconds.
  int32_t time_scale_;
  // Duration base on time_scale.
  int64_t duration_;
  Codec codec_;
  std::string codec_string_;
  std::string language_;
  // Whether the stream is potentially encrypted.
  // Note that in a potentially encrypted stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted_;
  // Whether the stream has clear lead.
  bool has_clear_lead_ = false;
  EncryptionConfig encryption_config_;
  // Optional byte data required for some audio/video decoders such as Vorbis
  // codebooks.
  std::vector<uint8_t> codec_config_;
  // Optional byte data required for some layered video decoders such as
  // MV-HEVC.
  std::vector<uint8_t> layered_codec_config_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_BASE_STREAM_INFO_H_
