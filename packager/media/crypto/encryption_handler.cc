// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/crypto/encryption_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/aes_pattern_cryptor.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vp8_parser.h"
#include "packager/media/codecs/vp9_parser.h"

namespace shaka {
namespace media {

namespace {
const size_t kCencBlockSize = 16u;

// Adds one or more subsamples to |*subsamples|.  This may add more than one
// if one of the values overflows the integer in the subsample.
void AddSubsample(uint64_t clear_bytes,
                  uint64_t cipher_bytes,
                  DecryptConfig* decrypt_config) {
  CHECK_LT(cipher_bytes, std::numeric_limits<uint32_t>::max());
  const uint64_t kUInt16Max = std::numeric_limits<uint16_t>::max();
  while (clear_bytes > kUInt16Max) {
    decrypt_config->AddSubsample(kUInt16Max, 0);
    clear_bytes -= kUInt16Max;
  }

  if (clear_bytes > 0 || cipher_bytes > 0)
    decrypt_config->AddSubsample(clear_bytes, cipher_bytes);
}

Codec GetVideoCodec(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo) return kUnknownCodec;
  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.codec();
}

uint8_t GetNaluLengthSize(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo)
    return 0;

  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.nalu_length_size();
}

KeySource::TrackType GetTrackTypeForEncryption(const StreamInfo& stream_info,
                                               uint32_t max_sd_pixels,
                                               uint32_t max_hd_pixels,
                                               uint32_t max_uhd1_pixels) {
  if (stream_info.stream_type() == kStreamAudio)
    return KeySource::TRACK_TYPE_AUDIO;

  if (stream_info.stream_type() != kStreamVideo)
    return KeySource::TRACK_TYPE_UNKNOWN;

  DCHECK_EQ(kStreamVideo, stream_info.stream_type());
  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  uint32_t pixels = video_stream_info.width() * video_stream_info.height();
  if (pixels <= max_sd_pixels) {
    return KeySource::TRACK_TYPE_SD;
  } else if (pixels <= max_hd_pixels) {
    return KeySource::TRACK_TYPE_HD;
  } else if (pixels <= max_uhd1_pixels) {
    return KeySource::TRACK_TYPE_UHD1;
  }
  return KeySource::TRACK_TYPE_UHD2;
}
}  // namespace

EncryptionHandler::EncryptionHandler(
    const EncryptionOptions& encryption_options,
    KeySource* key_source)
    : encryption_options_(encryption_options), key_source_(key_source) {}

EncryptionHandler::~EncryptionHandler() {}

Status EncryptionHandler::InitializeInternal() {
  if (num_input_streams() != 1 || next_output_stream_index() != 1) {
    return Status(error::INVALID_ARGUMENT,
                  "Expects exactly one input and output.");
  }
  return Status::OK;
}

Status EncryptionHandler::Process(std::unique_ptr<StreamData> stream_data) {
  Status status;
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      status = ProcessStreamInfo(stream_data->stream_info.get());
      break;
    case StreamDataType::kSegmentInfo:
      new_segment_ = true;
      if (remaining_clear_lead_ > 0)
        remaining_clear_lead_ -= stream_data->segment_info->duration;
      else
        stream_data->segment_info->is_encrypted = true;
      break;
    case StreamDataType::kMediaSample:
      status = ProcessMediaSample(stream_data->media_sample.get());
      break;
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      break;
  }
  return status.ok() ? Dispatch(std::move(stream_data)) : status;
}

Status EncryptionHandler::ProcessStreamInfo(StreamInfo* stream_info) {
  if (stream_info->is_encrypted()) {
    return Status(error::INVALID_ARGUMENT,
                  "Input stream is already encrypted.");
  }

  remaining_clear_lead_ =
      encryption_options_.clear_lead_in_seconds * stream_info->time_scale();
  crypto_period_duration_ =
      encryption_options_.crypto_period_duration_in_seconds *
      stream_info->time_scale();
  nalu_length_size_ = GetNaluLengthSize(*stream_info);
  video_codec_ = GetVideoCodec(*stream_info);
  track_type_ = GetTrackTypeForEncryption(
      *stream_info, encryption_options_.max_sd_pixels,
      encryption_options_.max_hd_pixels, encryption_options_.max_uhd1_pixels);
  switch (video_codec_) {
    case kCodecVP8:
      vpx_parser_.reset(new VP8Parser);
      break;
    case kCodecVP9:
      vpx_parser_.reset(new VP9Parser);
      break;
    case kCodecH264:
      header_parser_.reset(new H264VideoSliceHeaderParser);
      break;
    case kCodecHVC1:
      FALLTHROUGH_INTENDED;
    case kCodecHEV1:
      header_parser_.reset(new H265VideoSliceHeaderParser);
      break;
    default:
      // Expect an audio codec with nalu length size == 0.
      if (nalu_length_size_ > 0) {
        LOG(WARNING) << "Unknown video codec '" << video_codec_ << "'";
        return Status(error::ENCRYPTION_FAILURE, "Unknown video codec.");
      }
  }
  if (header_parser_ &&
      !header_parser_->Initialize(stream_info->codec_config())) {
    return Status(error::ENCRYPTION_FAILURE, "Fail to read SPS and PPS data.");
  }

  // Set up protection pattern.
  if (encryption_options_.protection_scheme == FOURCC_cbcs ||
      encryption_options_.protection_scheme == FOURCC_cens) {
    if (stream_info->stream_type() == kStreamVideo) {
      // Use 1:9 pattern for video.
      crypt_byte_block_ = 1u;
      skip_byte_block_ = 9u;
    } else {
      // Tracks other than video are protected using whole-block full-sample
      // encryption, which is essentially a pattern of 1:0. Note that this may
      // not be the same as the non-pattern based encryption counterparts, e.g.
      // in 'cens' for full sample encryption, the whole sample is encrypted up
      // to the last 16-byte boundary, see 23001-7:2016(E) 9.7; while in 'cenc'
      // for full sample encryption, the last partial 16-byte block is also
      // encrypted, see 23001-7:2016(E) 9.4.2. Another difference is the use of
      // constant iv.
      crypt_byte_block_ = 1u;
      skip_byte_block_ = 0u;
    }
  } else {
    // Not using pattern encryption.
    crypt_byte_block_ = 0u;
    skip_byte_block_ = 0u;
  }

  stream_info->set_is_encrypted(true);
  return Status::OK;
}

Status EncryptionHandler::ProcessMediaSample(MediaSample* sample) {
  // We need to parse the frame (which also updates the vpx parser) even if the
  // frame is not encrypted as the next (encrypted) frame may be dependent on
  // this clear frame.
  std::vector<VPxFrameInfo> vpx_frames;
  if (vpx_parser_ &&
      !vpx_parser_->Parse(sample->data(), sample->data_size(), &vpx_frames)) {
    return Status(error::ENCRYPTION_FAILURE, "Failed to parse vpx frame.");
  }
  if (remaining_clear_lead_ > 0)
    return Status::OK;

  Status status;
  if (new_segment_) {
    EncryptionKey encryption_key;
    bool create_encryptor = false;
    if (crypto_period_duration_ != 0) {
      const int64_t current_crypto_period_index =
          sample->dts() / crypto_period_duration_;
      if (current_crypto_period_index != prev_crypto_period_index_) {
        status = key_source_->GetCryptoPeriodKey(current_crypto_period_index,
                                                 track_type_, &encryption_key);
        if (!status.ok())
          return status;
        create_encryptor = true;
      }
    } else if (!encryptor_) {
      status = key_source_->GetKey(track_type_, &encryption_key);
      if (!status.ok())
        return status;
      create_encryptor = true;
    }
    if (create_encryptor && !CreateEncryptor(&encryption_key))
      return Status(error::ENCRYPTION_FAILURE, "Failed to create encryptor");
    new_segment_ = false;
  }

  std::unique_ptr<DecryptConfig> decrypt_config(new DecryptConfig(
      key_id_, encryptor_->iv(), std::vector<SubsampleEntry>(),
      encryption_options_.protection_scheme, crypt_byte_block_,
      skip_byte_block_));
  if (vpx_parser_) {
    if (!EncryptVpxFrame(vpx_frames, sample, decrypt_config.get()))
      return Status(error::ENCRYPTION_FAILURE, "Failed to encrypt VPx frames.");
    DCHECK_EQ(decrypt_config->GetTotalSizeOfSubsamples(), sample->data_size());
  } else if (nalu_length_size_ > 0) {
    if (!EncryptNalFrame(sample, decrypt_config.get())) {
      return Status(error::ENCRYPTION_FAILURE,
                    "Failed to encrypt video frames.");
    }
    DCHECK_EQ(decrypt_config->GetTotalSizeOfSubsamples(), sample->data_size());
  } else {
    DCHECK_LE(crypt_byte_block_, 1u);
    DCHECK_EQ(skip_byte_block_, 0u);
    EncryptBytes(sample->writable_data(), sample->data_size());
  }
  sample->set_decrypt_config(std::move(decrypt_config));
  encryptor_->UpdateIv();
  return Status::OK;
}

bool EncryptionHandler::CreateEncryptor(EncryptionKey* encryption_key) {
  std::unique_ptr<AesCryptor> encryptor;
  switch (encryption_options_.protection_scheme) {
    case FOURCC_cenc:
      encryptor.reset(new AesCtrEncryptor);
      break;
    case FOURCC_cbc1:
      encryptor.reset(new AesCbcEncryptor(kNoPadding));
      break;
    case FOURCC_cens:
      encryptor.reset(new AesPatternCryptor(
          crypt_byte_block_, skip_byte_block_,
          AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
          AesCryptor::kDontUseConstantIv,
          std::unique_ptr<AesCryptor>(new AesCtrEncryptor())));
      break;
    case FOURCC_cbcs:
      encryptor.reset(new AesPatternCryptor(
          crypt_byte_block_, skip_byte_block_,
          AesPatternCryptor::kEncryptIfCryptByteBlockRemaining,
          AesCryptor::kUseConstantIv,
          std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
      break;
    default:
      LOG(ERROR) << "Unsupported protection scheme.";
      return false;
  }

  if (encryption_key->iv.empty()) {
    if (!AesCryptor::GenerateRandomIv(encryption_options_.protection_scheme,
                                      &encryption_key->iv)) {
      LOG(ERROR) << "Failed to generate random iv.";
      return false;
    }
  }
  const bool initialized =
      encryptor->InitializeWithIv(encryption_key->key, encryption_key->iv);
  encryptor_ = std::move(encryptor);
  key_id_ = encryption_key->key_id;
  return initialized;
}

bool EncryptionHandler::EncryptVpxFrame(const std::vector<VPxFrameInfo>& vpx_frames,
                                        MediaSample* sample,
                                        DecryptConfig* decrypt_config) {
  uint8_t* data = sample->writable_data();
  const bool is_superframe = vpx_frames.size() > 1;
  for (const VPxFrameInfo& frame : vpx_frames) {
    uint16_t clear_bytes =
        static_cast<uint16_t>(frame.uncompressed_header_size);
    uint32_t cipher_bytes = static_cast<uint32_t>(
        frame.frame_size - frame.uncompressed_header_size);

    // "VP Codec ISO Media File Format Binding" document requires that the
    // encrypted bytes of each frame within the superframe must be block
    // aligned so that the counter state can be computed for each frame
    // within the superframe.
    // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
    // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to
    // avoid partial blocks in Subsamples.
    if (is_superframe || encryption_options_.protection_scheme == FOURCC_cbc1 ||
        encryption_options_.protection_scheme == FOURCC_cens) {
      const uint16_t misalign_bytes = cipher_bytes % kCencBlockSize;
      clear_bytes += misalign_bytes;
      cipher_bytes -= misalign_bytes;
    }

    decrypt_config->AddSubsample(clear_bytes, cipher_bytes);
    if (cipher_bytes > 0)
      EncryptBytes(data + clear_bytes, cipher_bytes);
    data += frame.frame_size;
  }
  // Add subsample for the superframe index if exists.
  if (is_superframe) {
    size_t index_size = sample->data() + sample->data_size() - data;
    DCHECK_LE(index_size, 2 + vpx_frames.size() * 4);
    DCHECK_GE(index_size, 2 + vpx_frames.size() * 1);
    uint16_t clear_bytes = static_cast<uint16_t>(index_size);
    uint32_t cipher_bytes = 0;
    decrypt_config->AddSubsample(clear_bytes, cipher_bytes);
  }
  return true;
}

bool EncryptionHandler::EncryptNalFrame(MediaSample* sample,
                                        DecryptConfig* decrypt_config) {
  const Nalu::CodecType nalu_type =
      (video_codec_ == kCodecHVC1 || video_codec_ == kCodecHEV1) ? Nalu::kH265
                                                                 : Nalu::kH264;
  NaluReader reader(nalu_type, nalu_length_size_, sample->writable_data(),
                    sample->data_size());

  // Store the current length of clear data.  This is used to squash
  // multiple unencrypted NAL units into fewer subsample entries.
  uint64_t accumulated_clear_bytes = 0;

  Nalu nalu;
  NaluReader::Result result;
  while ((result = reader.Advance(&nalu)) == NaluReader::kOk) {
    if (nalu.is_video_slice()) {
      // For video-slice NAL units, encrypt the video slice.  This skips
      // the frame header.  If this is an unrecognized codec, the whole NAL unit
      // will be encrypted.
      const int64_t video_slice_header_size =
          header_parser_ ? header_parser_->GetHeaderSize(nalu) : 0;
      if (video_slice_header_size < 0) {
        LOG(ERROR) << "Failed to read slice header.";
        return false;
      }

      uint64_t current_clear_bytes =
          nalu.header_size() + video_slice_header_size;
      uint64_t cipher_bytes = nalu.payload_size() - video_slice_header_size;

      // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
      // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to
      // avoid partial blocks in Subsamples.
      if (encryption_options_.protection_scheme == FOURCC_cbc1 ||
          encryption_options_.protection_scheme == FOURCC_cens) {
        const uint16_t misalign_bytes = cipher_bytes % kCencBlockSize;
        current_clear_bytes += misalign_bytes;
        cipher_bytes -= misalign_bytes;
      }

      const uint8_t* nalu_data = nalu.data() + current_clear_bytes;
      EncryptBytes(const_cast<uint8_t*>(nalu_data), cipher_bytes);

      AddSubsample(
          accumulated_clear_bytes + nalu_length_size_ + current_clear_bytes,
          cipher_bytes, decrypt_config);
      accumulated_clear_bytes = 0;
    } else {
      // For non-video-slice NAL units, don't encrypt.
      accumulated_clear_bytes +=
          nalu_length_size_ + nalu.header_size() + nalu.payload_size();
    }
  }
  if (result != NaluReader::kEOStream) {
    LOG(ERROR) << "Failed to parse NAL units.";
    return false;
  }
  AddSubsample(accumulated_clear_bytes, 0, decrypt_config);
  return true;
}

void EncryptionHandler::EncryptBytes(uint8_t* data, size_t size) {
  DCHECK(encryptor_);
  CHECK(encryptor_->Crypt(data, size, data));
}

void EncryptionHandler::InjectVpxParserForTesting(
    std::unique_ptr<VPxParser> vpx_parser) {
  vpx_parser_ = std::move(vpx_parser);
}

void EncryptionHandler::InjectVideoSliceHeaderParserForTesting(
    std::unique_ptr<VideoSliceHeaderParser> header_parser) {
  header_parser_ = std::move(header_parser);
}

}  // namespace media
}  // namespace shaka
