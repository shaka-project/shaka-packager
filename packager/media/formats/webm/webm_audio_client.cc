// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/formats/webm/webm_audio_client.h>

#include <absl/log/log.h>

#include <packager/media/formats/webm/webm_constants.h>

namespace {
// Timestamps are represented in double in WebM. Convert to int64_t in us.
const int32_t kWebMTimeScale = 1000000;
}  // namespace

namespace shaka {
namespace media {

WebMAudioClient::WebMAudioClient() {
  Reset();
}

WebMAudioClient::~WebMAudioClient() {
}

void WebMAudioClient::Reset() {
  channels_ = -1;
  samples_per_second_ = -1;
  output_samples_per_second_ = -1;
}

std::shared_ptr<AudioStreamInfo> WebMAudioClient::GetAudioStreamInfo(
    int64_t track_num,
    const std::string& codec_id,
    const std::vector<uint8_t>& codec_private,
    int64_t seek_preroll,
    int64_t codec_delay,
    const std::string& language,
    bool is_encrypted) {
  Codec audio_codec = kUnknownCodec;
  if (codec_id == "A_VORBIS") {
    audio_codec = kCodecVorbis;
  } else if (codec_id == "A_OPUS") {
    audio_codec = kCodecOpus;
  } else {
    LOG(ERROR) << "Unsupported audio codec_id " << codec_id;
    return std::shared_ptr<AudioStreamInfo>();
  }

  if (samples_per_second_ <= 0)
    return std::shared_ptr<AudioStreamInfo>();

  // Set channel layout default if a Channels element was not present.
  if (channels_ == -1)
    channels_ = 1;

  uint32_t sampling_frequency = samples_per_second_;
  // Always use 48kHz for OPUS.  See the "Input Sample Rate" section of the
  // spec: http://tools.ietf.org/html/draft-terriberry-oggopus-01#page-11
  if (audio_codec == kCodecOpus) {
    sampling_frequency = 48000;
  }

  const uint8_t* codec_config = NULL;
  size_t codec_config_size = 0;
  if (codec_private.size() > 0) {
    codec_config = &codec_private[0];
    codec_config_size = codec_private.size();
  }

  const uint8_t kSampleSizeInBits = 16u;
  return std::make_shared<AudioStreamInfo>(
      track_num, kWebMTimeScale, 0, audio_codec,
      AudioStreamInfo::GetCodecString(audio_codec, 0), codec_config,
      codec_config_size, kSampleSizeInBits, channels_, sampling_frequency,
      seek_preroll < 0 ? 0 : seek_preroll, codec_delay < 0 ? 0 : codec_delay, 0,
      0, language, is_encrypted);
}

bool WebMAudioClient::OnUInt(int id, int64_t val) {
  if (id == kWebMIdChannels) {
    if (channels_ != -1) {
      LOG(ERROR) << "Multiple values for id " << std::hex << id
                 << " specified. (" << channels_ << " and " << val << ")";
      return false;
    }

    channels_ = val;
  }
  return true;
}

bool WebMAudioClient::OnFloat(int id, double val) {
  double* dst = NULL;

  switch (id) {
    case kWebMIdSamplingFrequency:
      dst = &samples_per_second_;
      break;
    case kWebMIdOutputSamplingFrequency:
      dst = &output_samples_per_second_;
      break;
    default:
      return true;
  }

  if (val <= 0)
    return false;

  if (*dst != -1) {
    LOG(ERROR) << "Multiple values for id " << std::hex << id << " specified ("
               << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

}  // namespace media
}  // namespace shaka
