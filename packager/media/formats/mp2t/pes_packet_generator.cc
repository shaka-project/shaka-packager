// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/formats/mp2t/pes_packet_generator.h"

#include <algorithm>
#include <cstring>

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/aes_pattern_cryptor.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/buffer_writer.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/aac_audio_specific_config.h"
#include "packager/media/codecs/nal_unit_to_byte_stream_converter.h"
#include "packager/media/codecs/nalu_reader.h"
#include "packager/media/formats/mp2t/pes_packet.h"

namespace shaka {
namespace media {
namespace mp2t {

namespace {
const bool kEscapeData = true;
const uint8_t kVideoStreamId = 0xE0;
const uint8_t kAudioStreamId = 0xC0;
const double kTsTimescale = 90000.0;

// |target_data| is input as well as output. On success |target_data| contains
// the encrypted sample. The input data should be Nal unit byte stream.
// This function constructs encrypted sample in |encrypted_sample_data| then
// swap with target_data on success.
bool EncryptH264Sample(AesCryptor* encryptor,
                       std::vector<uint8_t>* target_data) {
  BufferWriter encrypted_sample_data(target_data->size() * 1.5);

  const int kLeadingClearBytesSize = 32;
  // Any Nalu smaller than 48 bytes shall not be encrypted.
  const uint64_t kSmallNalUnitSize = 48;

  NaluReader nalu_reader(Nalu::kH264, 0, target_data->data(),
                         target_data->size());
  NaluReader::Result result;
  Nalu nalu;
  while ((result = nalu_reader.Advance(&nalu)) == NaluReader::Result::kOk) {
    encrypted_sample_data.AppendInt(static_cast<uint32_t>(0x00000001));
    const uint64_t nalu_total_size = nalu.header_size() + nalu.payload_size();
    if (nalu.type() != Nalu::H264NaluType::H264_NonIDRSlice &&
        nalu.type() != Nalu::H264NaluType::H264_IDRSlice) {
      VLOG(3) << "Found Nalu type: " << nalu.type() << " skipping encryption.";
      encrypted_sample_data.AppendArray(nalu.data(), nalu_total_size);
      continue;
    }

    if (nalu_total_size <= kSmallNalUnitSize) {
      encrypted_sample_data.AppendArray(nalu.data(), nalu_total_size);
      continue;
    }

    const uint8_t* current = nalu.data() + kLeadingClearBytesSize;
    const uint64_t bytes_remaining = nalu_total_size - kLeadingClearBytesSize;

    if (!encryptor->Crypt(current, bytes_remaining,
                          const_cast<uint8_t*>(current))) {
      return false;
    }
    EscapeNalByteSequence(nalu.data(), nalu_total_size, &encrypted_sample_data);
  }

  encrypted_sample_data.SwapBuffer(target_data);
  return true;
}

bool EncryptAacSample(AesCryptor* encryptor,
                      std::vector<uint8_t>* target_data) {
  const int kUnencryptedLeaderSize = 16;
  if (target_data->size() <= kUnencryptedLeaderSize)
    return true;
  uint8_t* data_ptr = target_data->data() + kUnencryptedLeaderSize;
  return encryptor->Crypt(
      data_ptr, target_data->size() - kUnencryptedLeaderSize, data_ptr);
}
}  // namespace

PesPacketGenerator::PesPacketGenerator()
    : pes_packets_deleter_(&pes_packets_) {}
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
    timescale_scale_ = kTsTimescale / video_stream_info.time_scale();
    converter_.reset(new NalUnitToByteStreamConverter());
    return converter_->Initialize(video_stream_info.codec_config().data(),
                                  video_stream_info.codec_config().size(),
                                  !kEscapeData);
  } else if (stream_type_ == kStreamAudio) {
    const AudioStreamInfo& audio_stream_info =
        static_cast<const AudioStreamInfo&>(stream_info);
    if (audio_stream_info.codec() != AudioCodec::kCodecAAC) {
      NOTIMPLEMENTED() << "Audio codec " << audio_stream_info.codec()
                       << " is not supported yet.";
      return false;
    }
    timescale_scale_ = kTsTimescale / audio_stream_info.time_scale();
    adts_converter_.reset(new AACAudioSpecificConfig());
    return adts_converter_->Parse(audio_stream_info.codec_config());
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
    std::vector<uint8_t> byte_stream;
    if (!converter_->ConvertUnitToByteStream(
            sample->data(), sample->data_size(), sample->is_key_frame(),
            &byte_stream)) {
      LOG(ERROR) << "Failed to convert sample to byte stream.";
      return false;
    }

    if (encryptor_) {
      if (!EncryptH264Sample(encryptor_.get(), &byte_stream)) {
        LOG(ERROR) << "Failed to encrypt byte stream.";
        return false;
      }
    }
    current_processing_pes_->mutable_data()->swap(byte_stream);
    current_processing_pes_->set_stream_id(kVideoStreamId);
    pes_packets_.push_back(current_processing_pes_.release());
    return true;
  }
  DCHECK_EQ(stream_type_, kStreamAudio);
  DCHECK(adts_converter_);

  std::vector<uint8_t> aac_frame(sample->data(),
                                 sample->data() + sample->data_size());

  if (encryptor_) {
    if (!EncryptAacSample(encryptor_.get(), &aac_frame)) {
      LOG(ERROR) << "Failed to encrypt ADTS AAC.";
      return false;
    }
  }

  // TODO(rkuroiwa): ConvertToADTS() makes another copy of aac_frame internally.
  // Optimize copying in this function, possibly by adding a method on
  // AACAudioSpecificConfig that takes {pointer, length} pair and returns a
  // vector that has the ADTS header.
  if (!adts_converter_->ConvertToADTS(&aac_frame))
    return false;

  // TODO(rkuriowa): Put multiple samples in the PES packet to reduce # of PES
  // packets.
  current_processing_pes_->mutable_data()->swap(aac_frame);
  current_processing_pes_->set_stream_id(kAudioStreamId);
  pes_packets_.push_back(current_processing_pes_.release());
  return true;
}

bool PesPacketGenerator::SetEncryptionKey(
    scoped_ptr<EncryptionKey> encryption_key) {
  if (stream_type_ == kStreamVideo) {
    scoped_ptr<AesCbcEncryptor> cbc(
        new AesCbcEncryptor(CbcPaddingScheme::kNoPadding));

    const uint8_t kEncryptedBlocks = 1;
    const uint8_t kClearBlocks = 9;
    encryptor_.reset(new AesPatternCryptor(
        kEncryptedBlocks, kClearBlocks,
        AesPatternCryptor::kSkipIfCryptByteBlockRemaining,
        AesCryptor::ConstantIvFlag::kUseConstantIv, cbc.Pass()));
  } else if (stream_type_ == kStreamAudio) {
    encryptor_.reset(
        new AesCbcEncryptor(CbcPaddingScheme::kNoPadding,
                            AesCryptor::ConstantIvFlag::kUseConstantIv));
  } else {
    LOG(ERROR) << "Cannot encrypt stream type: " << stream_type_;
    return false;
  }

  return encryptor_->InitializeWithIv(encryption_key->key, encryption_key->iv);
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
}  // namespace shaka
