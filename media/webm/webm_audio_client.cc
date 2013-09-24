// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webm/webm_audio_client.h"

#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/webm/webm_constants.h"

namespace media {

WebMAudioClient::WebMAudioClient(const LogCB& log_cb)
    : log_cb_(log_cb) {
  Reset();
}

WebMAudioClient::~WebMAudioClient() {
}

void WebMAudioClient::Reset() {
  channels_ = -1;
  samples_per_second_ = -1;
  output_samples_per_second_ = -1;
}

bool WebMAudioClient::InitializeConfig(
    const std::string& codec_id, const std::vector<uint8>& codec_private,
    bool is_encrypted, AudioDecoderConfig* config) {
  DCHECK(config);

  AudioCodec audio_codec = kUnknownAudioCodec;
  if (codec_id == "A_VORBIS") {
    audio_codec = kCodecVorbis;
  } else {
    MEDIA_LOG(log_cb_) << "Unsupported audio codec_id " << codec_id;
    return false;
  }

  if (samples_per_second_ <= 0)
    return false;

  // Set channel layout default if a Channels element was not present.
  if (channels_ == -1)
    channels_ = 1;

  ChannelLayout channel_layout =  GuessChannelLayout(channels_);

  if (channel_layout == CHANNEL_LAYOUT_UNSUPPORTED) {
    MEDIA_LOG(log_cb_) << "Unsupported channel count " << channels_;
    return false;
  }

  int samples_per_second = samples_per_second_;
  if (output_samples_per_second_ > 0)
    samples_per_second = output_samples_per_second_;

  const uint8* extra_data = NULL;
  size_t extra_data_size = 0;
  if (codec_private.size() > 0) {
    extra_data = &codec_private[0];
    extra_data_size = codec_private.size();
  }

  config->Initialize(
      audio_codec, kSampleFormatPlanarF32, channel_layout,
      samples_per_second, extra_data, extra_data_size, is_encrypted, true);
  return config->IsValidConfig();
}

bool WebMAudioClient::OnUInt(int id, int64 val) {
  if (id == kWebMIdChannels) {
    if (channels_ != -1) {
      MEDIA_LOG(log_cb_) << "Multiple values for id " << std::hex << id
                         << " specified. (" << channels_ << " and " << val
                         << ")";
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
    MEDIA_LOG(log_cb_) << "Multiple values for id " << std::hex << id
                       << " specified (" << *dst << " and " << val << ")";
    return false;
  }

  *dst = val;
  return true;
}

}  // namespace media
