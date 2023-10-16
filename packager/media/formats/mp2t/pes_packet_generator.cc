// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/formats/mp2t/pes_packet_generator.h>

#include <algorithm>
#include <cstring>
#include <memory>

#include <absl/log/check.h>

#include <packager/macros/logging.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/buffer_writer.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/codecs/aac_audio_specific_config.h>
#include <packager/media/codecs/nal_unit_to_byte_stream_converter.h>
#include <packager/media/codecs/nalu_reader.h>
#include <packager/media/formats/mp2t/pes_packet.h>

namespace shaka {
namespace media {
namespace mp2t {

namespace {
const uint8_t kVideoStreamId = 0xE0;
const uint8_t kAacAudioStreamId = 0xC0;
const uint8_t kAc3AudioStreamId = 0xBD;  // AC3 uses private stream 1 id.
const double kTsTimescale = 90000.0;
}  // namespace

PesPacketGenerator::PesPacketGenerator(
    int32_t transport_stream_timestamp_offset)
    : transport_stream_timestamp_offset_(transport_stream_timestamp_offset) {}

PesPacketGenerator::~PesPacketGenerator() {}

bool PesPacketGenerator::Initialize(const StreamInfo& stream_info) {
  pes_packets_.clear();
  stream_type_ = stream_info.stream_type();

  if (stream_type_ == kStreamVideo) {
    const VideoStreamInfo& video_stream_info =
        static_cast<const VideoStreamInfo&>(stream_info);
    if (video_stream_info.codec() != Codec::kCodecH264) {
      NOTIMPLEMENTED() << "Video codec " << video_stream_info.codec()
                       << " is not supported.";
      return false;
    }
    timescale_scale_ = kTsTimescale / video_stream_info.time_scale();
    converter_.reset(new NalUnitToByteStreamConverter());
    return converter_->Initialize(video_stream_info.codec_config().data(),
                                  video_stream_info.codec_config().size());
  }
  if (stream_type_ == kStreamAudio) {
    const AudioStreamInfo& audio_stream_info =
        static_cast<const AudioStreamInfo&>(stream_info);
    timescale_scale_ = kTsTimescale / audio_stream_info.time_scale();
    if (audio_stream_info.codec() == Codec::kCodecAAC) {
      audio_stream_id_ = kAacAudioStreamId;
      adts_converter_.reset(new AACAudioSpecificConfig());
      return adts_converter_->Parse(audio_stream_info.codec_config());
    }
    if (audio_stream_info.codec() == Codec::kCodecAC3 ||
        audio_stream_info.codec() == Codec::kCodecEAC3 ||
        audio_stream_info.codec() == Codec::kCodecMP3) {
      audio_stream_id_ = kAc3AudioStreamId;
      // No converter needed for AC3, E-AC3 and MP3.
      return true;
    }
    NOTIMPLEMENTED() << "Audio codec " << audio_stream_info.codec()
                     << " is not supported yet.";
    return false;
  }

  NOTIMPLEMENTED() << "Stream type: " << stream_type_ << " not implemented.";
  return false;
}

bool PesPacketGenerator::PushSample(const MediaSample& sample) {
  if (!current_processing_pes_)
    current_processing_pes_.reset(new PesPacket());

  const int64_t pts =
      sample.pts() * timescale_scale_ + transport_stream_timestamp_offset_;
  const int64_t dts =
      sample.dts() * timescale_scale_ + transport_stream_timestamp_offset_;

  if (pts < 0 || dts < 0) {
    LOG(ERROR) << "Seeing negative timestamp (" << pts << "," << dts << ")"
               << " after applying offset "
               << transport_stream_timestamp_offset_
               << ". Please check if it is expected. Adjust "
                  "--transport_stream_timestamp_offset_ms if needed.";
    return false;
  }

  current_processing_pes_->set_is_key_frame(sample.is_key_frame());
  current_processing_pes_->set_pts(pts);
  current_processing_pes_->set_dts(dts);
  if (stream_type_ == kStreamVideo) {
    DCHECK(converter_);
    std::vector<SubsampleEntry> subsamples;
    if (sample.decrypt_config())
      subsamples = sample.decrypt_config()->subsamples();
    const bool kEscapeEncryptedNalu = true;
    std::vector<uint8_t> byte_stream;
    if (!converter_->ConvertUnitToByteStreamWithSubsamples(
            sample.data(), sample.data_size(), sample.is_key_frame(),
            kEscapeEncryptedNalu, &byte_stream, &subsamples)) {
      LOG(ERROR) << "Failed to convert sample to byte stream.";
      return false;
    }

    current_processing_pes_->mutable_data()->swap(byte_stream);
    current_processing_pes_->set_stream_id(kVideoStreamId);
    pes_packets_.push_back(std::move(current_processing_pes_));
    return true;
  }
  DCHECK_EQ(stream_type_, kStreamAudio);

  std::vector<uint8_t> audio_frame;

  // AAC is carried in ADTS.
  if (adts_converter_) {
    if (!adts_converter_->ConvertToADTS(sample.data(), sample.data_size(),
                                        &audio_frame))
      return false;
  } else {
    audio_frame.assign(sample.data(), sample.data() + sample.data_size());
  }

  // TODO(rkuriowa): Put multiple samples in the PES packet to reduce # of PES
  // packets.
  current_processing_pes_->mutable_data()->swap(audio_frame);
  current_processing_pes_->set_stream_id(audio_stream_id_);
  pes_packets_.push_back(std::move(current_processing_pes_));
  return true;
}

size_t PesPacketGenerator::NumberOfReadyPesPackets() {
  return pes_packets_.size();
}

std::unique_ptr<PesPacket> PesPacketGenerator::GetNextPesPacket() {
  DCHECK(!pes_packets_.empty());
  std::unique_ptr<PesPacket> pes = std::move(pes_packets_.front());
  pes_packets_.pop_front();
  return pes;
}

bool PesPacketGenerator::Flush() {
  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace shaka
