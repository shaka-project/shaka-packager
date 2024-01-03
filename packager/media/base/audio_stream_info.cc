// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/base/audio_stream_info.h>

#include <cinttypes>

#include <absl/log/log.h>
#include <absl/strings/str_format.h>

#include <packager/macros/compiler.h>
#include <packager/macros/logging.h>
#include <packager/media/base/limits.h>

namespace shaka {
namespace media {

namespace {
std::string AudioCodecToString(Codec codec) {
  switch (codec) {
    case kCodecAAC:
      return "AAC";
    case kCodecAC3:
      return "AC3";
    case kCodecDTSC:
      return "DTSC";
    case kCodecDTSE:
      return "DTSE";
    case kCodecDTSH:
      return "DTSH";
    case kCodecDTSL:
      return "DTSL";
    case kCodecDTSM:
      return "DTS-";
    case kCodecDTSP:
      return "DTS+";
    case kCodecEAC3:
      return "EAC3";
    case kCodecAC4:
      return "AC4";
    case kCodecFlac:
      return "FLAC";
    case kCodecOpus:
      return "Opus";
    case kCodecVorbis:
      return "Vorbis";
    case kCodecMP3:
      return "MP3";
    default:
      NOTIMPLEMENTED() << "Unknown Audio Codec: " << codec;
      return "UnknownCodec";
  }
}

FourCC CodecToFourCC(Codec codec) {
  switch (codec) {
    case kCodecMha1:
      return FOURCC_mha1;
    case kCodecMhm1:
      return FOURCC_mhm1;
    default:
      return FOURCC_NULL;
  }
}
}  // namespace

AudioStreamInfo::AudioStreamInfo(int track_id,
                                 int32_t time_scale,
                                 int64_t duration,
                                 Codec codec,
                                 const std::string& codec_string,
                                 const uint8_t* codec_config,
                                 size_t codec_config_size,
                                 uint8_t sample_bits,
                                 uint8_t num_channels,
                                 uint32_t sampling_frequency,
                                 uint64_t seek_preroll_ns,
                                 uint64_t codec_delay_ns,
                                 uint32_t max_bitrate,
                                 uint32_t avg_bitrate,
                                 const std::string& language,
                                 bool is_encrypted)
    : StreamInfo(kStreamAudio,
                 track_id,
                 time_scale,
                 duration,
                 codec,
                 codec_string,
                 codec_config,
                 codec_config_size,
                 language,
                 is_encrypted),
      sample_bits_(sample_bits),
      num_channels_(num_channels),
      sampling_frequency_(sampling_frequency),
      seek_preroll_ns_(seek_preroll_ns),
      codec_delay_ns_(codec_delay_ns),
      max_bitrate_(max_bitrate),
      avg_bitrate_(avg_bitrate) {}

AudioStreamInfo::~AudioStreamInfo() {}

bool AudioStreamInfo::IsValidConfig() const {
  return codec() != kUnknownCodec && num_channels_ != 0 &&
         num_channels_ <= limits::kMaxChannels && sample_bits_ > 0 &&
         sample_bits_ <= limits::kMaxBitsPerSample && sampling_frequency_ > 0 &&
         sampling_frequency_ <= limits::kMaxSampleRate;
}

std::string AudioStreamInfo::ToString() const {
  std::string str = absl::StrFormat(
      "%s codec: %s\n sample_bits: %d\n num_channels: %d\n "
      "sampling_frequency: %d\n language: %s\n",
      StreamInfo::ToString().c_str(), AudioCodecToString(codec()).c_str(),
      sample_bits_, num_channels_, sampling_frequency_, language().c_str());
  if (seek_preroll_ns_ != 0) {
    absl::StrAppendFormat(&str, " seek_preroll_ns: %" PRIu64 "\n",
                          seek_preroll_ns_);
  }
  if (codec_delay_ns_ != 0) {
    absl::StrAppendFormat(&str, " codec_delay_ns: %" PRIu64 "\n",
                          codec_delay_ns_);
  }
  return str;
}

std::unique_ptr<StreamInfo> AudioStreamInfo::Clone() const {
  return std::unique_ptr<StreamInfo>(new AudioStreamInfo(*this));
}

std::string AudioStreamInfo::GetCodecString(Codec codec,
                                            uint8_t audio_object_type) {
  switch (codec) {
    case kCodecAAC:
      return "mp4a.40." + absl::StrFormat("%hhu", audio_object_type);
    case kCodecAC3:
      return "ac-3";
    case kCodecDTSC:
      return "dtsc";
    case kCodecDTSE:
      return "dtse";
    case kCodecDTSH:
      return "dtsh";
    case kCodecDTSL:
      return "dtsl";
    case kCodecDTSM:
      return "dts-";
    case kCodecDTSP:
      return "dts+";
    case kCodecEAC3:
      return "ec-3";
    case kCodecAC4:
      // ETSI TS 103 190-2 Digital Audio Compression (AC-4) Standard; Part 2:
      // Immersive and personalized audio E.13. audio_object_type is composed of
      // bitstream_version (3bits), presentation_version (2bits) and
      // mdcompat (3bits).
      return absl::StrFormat(
          "ac-4.%02d.%02d.%02d", (audio_object_type & 0xE0) >> 5,
          (audio_object_type & 0x18) >> 3, audio_object_type & 0x7);
    case kCodecFlac:
      return "flac";
    case kCodecOpus:
      return "opus";
    case kCodecMP3:
      return "mp3";
    case kCodecVorbis:
      return "vorbis";
    case kCodecMha1:
    case kCodecMhm1:
      // The signalling of the codecs parameters is according to RFC6381 [11]
      //   and ISO/IEC 23008-3 clause 21 [7].
      // The value consists of the following two parts separated by a dot:
      //  - the sample entry 4CC code ('mha1', 'mha2', 'mhm1', 'mhm2')
      //  - ‘0x’ followed by the hex value of the profile-levelid, as defined 
      //      in in ISO/IEC 23008-3 [7]
      return absl::StrFormat("%s.0x%02x",
                             FourCCToString(CodecToFourCC(codec)).c_str(),
                             audio_object_type);
    default:
      NOTIMPLEMENTED() << "Codec: " << codec;
      return "unknown";
  }
}

}  // namespace media
}  // namespace shaka
