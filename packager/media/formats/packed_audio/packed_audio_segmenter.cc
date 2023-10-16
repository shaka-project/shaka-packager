// Copyright 2018 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/packed_audio/packed_audio_segmenter.h>

#include <memory>

#include <absl/log/check.h>

#include <packager/macros/status.h>
#include <packager/media/base/id3_tag.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/codecs/aac_audio_specific_config.h>
#include <packager/media/codecs/hls_audio_util.h>

namespace shaka {
namespace media {
namespace {
std::string TimestampToString(int64_t timestamp) {
  // https://tools.ietf.org/html/rfc8216 The ID3 payload MUST be a 33-bit MPEG-2
  // Program Elementary Stream timestamp expressed as a big-endian eight-octet
  // number, with the upper 31 bits set to zero.
  timestamp &= 0x1FFFFFFFFull;

  BufferWriter buffer;
  buffer.AppendInt(timestamp);
  return std::string(buffer.Buffer(), buffer.Buffer() + buffer.Size());
}
}  // namespace

PackedAudioSegmenter::PackedAudioSegmenter(
    int32_t transport_stream_timestamp_offset)
    : transport_stream_timestamp_offset_(transport_stream_timestamp_offset) {}

PackedAudioSegmenter::~PackedAudioSegmenter() = default;

Status PackedAudioSegmenter::Initialize(const StreamInfo& stream_info) {
  const StreamType stream_type = stream_info.stream_type();
  if (stream_type != StreamType::kStreamAudio) {
    LOG(ERROR) << "PackedAudioSegmenter cannot handle stream type "
               << stream_type;
    return Status(error::MUXER_FAILURE, "Unsupported stream type.");
  }

  codec_ = stream_info.codec();
  audio_codec_config_ = stream_info.codec_config();
  timescale_scale_ = kPackedAudioTimescale / stream_info.time_scale();

  if (codec_ == kCodecAAC) {
    adts_converter_ = CreateAdtsConverter();
    if (!adts_converter_->Parse(audio_codec_config_)) {
      return Status(error::MUXER_FAILURE, "Invalid audio codec configuration.");
    }
  }

  return Status::OK;
}

Status PackedAudioSegmenter::AddSample(const MediaSample& sample) {
  if (sample.is_encrypted() && audio_setup_information_.empty())
    RETURN_IF_ERROR(EncryptionAudioSetup(sample));

  if (start_of_new_segment_) {
    RETURN_IF_ERROR(StartNewSegment(sample));
    start_of_new_segment_ = false;
  }

  if (adts_converter_) {
    std::vector<uint8_t> audio_frame;
    if (!adts_converter_->ConvertToADTS(sample.data(), sample.data_size(),
                                        &audio_frame))
      return Status(error::MUXER_FAILURE, "Failed to convert to ADTS.");
    segment_buffer_.AppendArray(audio_frame.data(), audio_frame.size());
  } else {
    segment_buffer_.AppendArray(sample.data(), sample.data_size());
  }
  return Status::OK;
}

Status PackedAudioSegmenter::FinalizeSegment() {
  start_of_new_segment_ = true;
  return Status::OK;
}

double PackedAudioSegmenter::TimescaleScale() const {
  return timescale_scale_;
}

std::unique_ptr<AACAudioSpecificConfig>
PackedAudioSegmenter::CreateAdtsConverter() {
  return std::unique_ptr<AACAudioSpecificConfig>(new AACAudioSpecificConfig);
}

std::unique_ptr<Id3Tag> PackedAudioSegmenter::CreateId3Tag() {
  return std::unique_ptr<Id3Tag>(new Id3Tag);
}

Status PackedAudioSegmenter::EncryptionAudioSetup(const MediaSample& sample) {
  // For codecs other than AC3, audio setup data is the audio codec
  // configuration data.
  const uint8_t* audio_setup_data = audio_codec_config_.data();
  size_t audio_setup_data_size = audio_codec_config_.size();
  if (codec_ == kCodecAC3) {
    // https://goo.gl/N7Tvqi MPEG-2 Stream Encryption Format for HTTP Live
    // Streaming 2.3.2.2 AC-3 Setup: For AC-3, the setup_data in the
    // audio_setup_information is the first 10 bytes of the audio data (the
    // syncframe()).
    const size_t kSetupDataSize = 10u;
    if (sample.data_size() < kSetupDataSize) {
      LOG(ERROR) << "Sample is too small for AC3: " << sample.data_size();
      return Status(error::MUXER_FAILURE, "Sample is too small for AC3.");
    }
    audio_setup_data = sample.data();
    audio_setup_data_size = kSetupDataSize;
  }

  BufferWriter buffer;
  if (!WriteAudioSetupInformation(codec_, audio_setup_data,
                                  audio_setup_data_size, &buffer)) {
    return Status(error::MUXER_FAILURE,
                  "Failed to write audio setup information.");
  }
  audio_setup_information_.assign(buffer.Buffer(),
                                  buffer.Buffer() + buffer.Size());
  return Status::OK;
}

Status PackedAudioSegmenter::StartNewSegment(const MediaSample& sample) {
  segment_buffer_.Clear();

  const int64_t pts =
      sample.pts() * timescale_scale_ + transport_stream_timestamp_offset_;
  if (pts < 0) {
    LOG(ERROR) << "Seeing negative timestamp " << pts
               << " after applying offset "
               << transport_stream_timestamp_offset_
               << ". Please check if it is expected. Adjust "
                  "--transport_stream_timestamp_offset_ms if needed.";
    return Status(error::MUXER_FAILURE, "Unsupported negative timestamp.");
  }

  // Use a unique_ptr so it can be mocked for testing.
  std::unique_ptr<Id3Tag> id3_tag = CreateId3Tag();
  id3_tag->AddPrivateFrame(kTimestampOwnerIdentifier, TimestampToString(pts));
  if (!audio_setup_information_.empty()) {
    id3_tag->AddPrivateFrame(kAudioDescriptionOwnerIdentifier,
                             audio_setup_information_);
  }
  CHECK(id3_tag->WriteToBuffer(&segment_buffer_));

  return Status::OK;
}

}  // namespace media
}  // namespace shaka
