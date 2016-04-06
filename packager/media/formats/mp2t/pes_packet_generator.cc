// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/pes_packet_generator.h"

#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/filters/nal_unit_to_byte_stream_converter.h"
#include "packager/media/formats/mp2t/pes_packet.h"
#include "packager/media/formats/mp4/aac_audio_specific_config.h"

namespace edash_packager {
namespace media {
namespace mp2t {

namespace {
const bool kEscapeData = true;
const uint8_t kVideoStreamId = 0xe0;
const uint8_t kAudioStreamId = 0xc0;
}  // namespace

PesPacketGenerator::PesPacketGenerator() : pes_packets_deleter_(&pes_packets_) {}
PesPacketGenerator::~PesPacketGenerator() {}

bool PesPacketGenerator::Initialize(const StreamInfo& stream_info) {
  STLDeleteElements(&pes_packets_);
  stream_type_ = stream_info.stream_type();

  if (stream_type_ == kStreamVideo) {
    const VideoStreamInfo& video_stream_info =
        static_cast<const VideoStreamInfo&>(stream_info);
    if (video_stream_info.codec() != VideoCodec::kCodecH264) {
      NOTIMPLEMENTED() << "Video codec " << video_stream_info.codec()
                       << " is not supported.";
      return false;
    }
    timescale_scale_ = 90000.0 / video_stream_info.time_scale();
    converter_.reset(new NalUnitToByteStreamConverter());
    return converter_->Initialize(video_stream_info.extra_data().data(),
                                  video_stream_info.extra_data().size(),
                                  !kEscapeData);
  } else if (stream_type_ == kStreamAudio) {
    const AudioStreamInfo& audio_stream_info =
        static_cast<const AudioStreamInfo&>(stream_info);
    if (audio_stream_info.codec() != AudioCodec::kCodecAAC) {
      NOTIMPLEMENTED() << "Audio codec " << audio_stream_info.codec()
                       << " is not supported yet.";
      return false;
    }
    timescale_scale_ = 90000.0 / audio_stream_info.time_scale();
    adts_converter_.reset(new mp4::AACAudioSpecificConfig());
    return adts_converter_->Parse(audio_stream_info.extra_data());
  }

  NOTIMPLEMENTED() << "Stream type: " << stream_type_ << " not implemented.";
  return false;
}

bool PesPacketGenerator::PushSample(scoped_refptr<MediaSample> sample) {
  if (!current_processing_pes_)
    current_processing_pes_.reset(new PesPacket());

  current_processing_pes_->set_pts(timescale_scale_ * sample->pts());
  current_processing_pes_->set_dts(timescale_scale_ * sample->dts());
  if (stream_type_ == kStreamVideo) {
    DCHECK(converter_);
    if (!converter_->ConvertUnitToByteStream(
            sample->data(), sample->data_size(), sample->is_key_frame(),
            current_processing_pes_->mutable_data())) {
      LOG(ERROR) << "Failed to convert sample to byte stream.";
      return false;
    }
    current_processing_pes_->set_stream_id(kVideoStreamId);
    pes_packets_.push_back(current_processing_pes_.release());
    return true;
  }
  DCHECK_EQ(stream_type_, kStreamAudio);
  DCHECK(adts_converter_);

  std::vector<uint8_t> aac_frame(sample->data(),
                                 sample->data() + sample->data_size());
  if (!adts_converter_->ConvertToADTS(&aac_frame))
    return false;

  // TODO(rkuriowa): Put multiple samples in the PES packet to reduce # of PES
  // packets.
  current_processing_pes_->mutable_data()->swap(aac_frame);
  current_processing_pes_->set_stream_id(kAudioStreamId);
  pes_packets_.push_back(current_processing_pes_.release());
  return true;
}

size_t PesPacketGenerator::NumberOfReadyPesPackets() {
  return pes_packets_.size();
}

scoped_ptr<PesPacket> PesPacketGenerator::GetNextPesPacket() {
  DCHECK(!pes_packets_.empty());
  PesPacket* pes = pes_packets_.front();
  pes_packets_.pop_front();
  return scoped_ptr<PesPacket>(pes);
}

bool PesPacketGenerator::Flush() {
  return true;
}

}  // namespace mp2t
}  // namespace media
}  // namespace edash_packager
