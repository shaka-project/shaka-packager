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
#include "packager/media/base/media_sample.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vp8_parser.h"
#include "packager/media/codecs/vp9_parser.h"

namespace shaka {
namespace media {

namespace {
const size_t kCencBlockSize = 16u;

// The default KID for key rotation is all 0s.
const uint8_t kKeyRotationDefaultKeyId[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

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

uint8_t GetNaluLengthSize(const StreamInfo& stream_info) {
  if (stream_info.stream_type() != kStreamVideo)
    return 0;

  const VideoStreamInfo& video_stream_info =
      static_cast<const VideoStreamInfo&>(stream_info);
  return video_stream_info.nalu_length_size();
}

std::string GetStreamLabelForEncryption(
    const StreamInfo& stream_info,
    const std::function<std::string(
        const EncryptionParams::EncryptedStreamAttributes& stream_attributes)>&
        stream_label_func) {
  EncryptionParams::EncryptedStreamAttributes stream_attributes;
  if (stream_info.stream_type() == kStreamAudio) {
    stream_attributes.stream_type =
        EncryptionParams::EncryptedStreamAttributes::kAudio;
  } else if (stream_info.stream_type() == kStreamVideo) {
    const VideoStreamInfo& video_stream_info =
        static_cast<const VideoStreamInfo&>(stream_info);
    stream_attributes.stream_type =
        EncryptionParams::EncryptedStreamAttributes::kVideo;
    stream_attributes.oneof.video.width = video_stream_info.width();
    stream_attributes.oneof.video.height = video_stream_info.height();
  }
  return stream_label_func(stream_attributes);
}
}  // namespace

EncryptionHandler::EncryptionHandler(
    const EncryptionOptions& encryption_options,
    KeySource* key_source)
    : encryption_options_(encryption_options), key_source_(key_source) {}

EncryptionHandler::~EncryptionHandler() {}

Status EncryptionHandler::InitializeInternal() {
  if (!encryption_options_.stream_label_func) {
    return Status(error::INVALID_ARGUMENT, "Stream label function not set.");
  }
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
    case StreamDataType::kSegmentInfo: {
      SegmentInfo* segment_info = stream_data->segment_info.get();
      segment_info->is_encrypted = remaining_clear_lead_ <= 0;

      const bool key_rotation_enabled = crypto_period_duration_ != 0;
      if (key_rotation_enabled)
        segment_info->key_rotation_encryption_config = encryption_config_;
      if (!segment_info->is_subsegment) {
        if (key_rotation_enabled)
          check_new_crypto_period_ = true;
        if (remaining_clear_lead_ > 0)
          remaining_clear_lead_ -= segment_info->duration;
      }
      break;
    }
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
  codec_ = stream_info->codec();
  nalu_length_size_ = GetNaluLengthSize(*stream_info);
  stream_label_ = GetStreamLabelForEncryption(
      *stream_info, encryption_options_.stream_label_func);
  switch (codec_) {
    case kCodecVP9:
      if (encryption_options_.vp9_subsample_encryption)
        vpx_parser_.reset(new VP9Parser);
      break;
    case kCodecH264:
      header_parser_.reset(new H264VideoSliceHeaderParser);
      break;
    case kCodecH265:
      header_parser_.reset(new H265VideoSliceHeaderParser);
      break;
    default:
      // Other codecs should have nalu length size == 0.
      if (nalu_length_size_ > 0) {
        LOG(WARNING) << "Unknown video codec '" << codec_ << "'";
        return Status(error::ENCRYPTION_FAILURE, "Unknown video codec.");
      }
  }
  if (header_parser_) {
    CHECK_NE(nalu_length_size_, 0u) << "AnnexB stream is not supported yet";
    if (!header_parser_->Initialize(stream_info->codec_config())) {
      return Status(error::ENCRYPTION_FAILURE,
                    "Fail to read SPS and PPS data.");
    }
  }

  Status status = SetupProtectionPattern(stream_info->stream_type());
  if (!status.ok())
    return status;

  EncryptionKey encryption_key;
  const bool key_rotation_enabled = crypto_period_duration_ != 0;
  if (key_rotation_enabled) {
    check_new_crypto_period_ = true;
    // Setup dummy key id and key to signal encryption for key rotation.
    encryption_key.key_id.assign(
        kKeyRotationDefaultKeyId,
        kKeyRotationDefaultKeyId + sizeof(kKeyRotationDefaultKeyId));
    // The key is not really used to encrypt any data. It is there just for
    // convenience.
    encryption_key.key = encryption_key.key_id;
  } else {
    status = key_source_->GetKey(stream_label_, &encryption_key);
    if (!status.ok())
      return status;
  }
  if (!CreateEncryptor(encryption_key))
    return Status(error::ENCRYPTION_FAILURE, "Failed to create encryptor");

  stream_info->set_is_encrypted(true);
  stream_info->set_has_clear_lead(encryption_options_.clear_lead_in_seconds >
                                  0);
  stream_info->set_encryption_config(*encryption_config_);
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

  // Need to setup the encryptor for new segments even if this segment does not
  // need to be encrypted, so we can signal encryption metadata earlier to
  // allows clients to prefetch the keys.
  if (check_new_crypto_period_) {
    const int64_t current_crypto_period_index =
        sample->dts() / crypto_period_duration_;
    if (current_crypto_period_index != prev_crypto_period_index_) {
      EncryptionKey encryption_key;
      Status status = key_source_->GetCryptoPeriodKey(
          current_crypto_period_index, stream_label_, &encryption_key);
      if (!status.ok())
        return status;
      if (!CreateEncryptor(encryption_key))
        return Status(error::ENCRYPTION_FAILURE, "Failed to create encryptor");
    }
    check_new_crypto_period_ = false;
  }

  if (remaining_clear_lead_ > 0)
    return Status::OK;

  std::unique_ptr<DecryptConfig> decrypt_config(new DecryptConfig(
      encryption_config_->key_id, encryptor_->iv(),
      std::vector<SubsampleEntry>(), encryption_options_.protection_scheme,
      crypt_byte_block_, skip_byte_block_));
  bool result = true;
  if (vpx_parser_) {
    result = EncryptVpxFrame(vpx_frames, sample, decrypt_config.get());
    if (result) {
      DCHECK_EQ(decrypt_config->GetTotalSizeOfSubsamples(),
                sample->data_size());
    }
  } else if (header_parser_) {
    result = EncryptNalFrame(sample, decrypt_config.get());
    if (result) {
      DCHECK_EQ(decrypt_config->GetTotalSizeOfSubsamples(),
                sample->data_size());
    }
  } else {
    if (sample->data_size() > leading_clear_bytes_size_) {
      EncryptBytes(sample->writable_data() + leading_clear_bytes_size_,
                   sample->data_size() - leading_clear_bytes_size_);
    }
  }
  if (!result)
    return Status(error::ENCRYPTION_FAILURE, "Failed to encrypt samples.");
  sample->set_is_encrypted(true);
  sample->set_decrypt_config(std::move(decrypt_config));
  encryptor_->UpdateIv();
  return Status::OK;
}

Status EncryptionHandler::SetupProtectionPattern(StreamType stream_type) {
  switch (encryption_options_.protection_scheme) {
    case kAppleSampleAesProtectionScheme: {
      const size_t kH264LeadingClearBytesSize = 32u;
      const size_t kSmallNalUnitSize = 32u + 16u;
      const size_t kAudioLeadingClearBytesSize = 16u;
      switch (codec_) {
        case kCodecH264:
          // Apple Sample AES uses 1:9 pattern for video.
          crypt_byte_block_ = 1u;
          skip_byte_block_ = 9u;
          leading_clear_bytes_size_ = kH264LeadingClearBytesSize;
          min_protected_data_size_ = kSmallNalUnitSize + 1u;
          break;
        case kCodecAAC:
          FALLTHROUGH_INTENDED;
        case kCodecAC3:
          // Audio is whole sample encrypted. We could not use a
          // crypto_byte_block_ of 1 here as if there is one crypto block
          // remaining, it need not be encrypted for video but it needs to be
          // encrypted for audio.
          crypt_byte_block_ = 0u;
          skip_byte_block_ = 0u;
          leading_clear_bytes_size_ = kAudioLeadingClearBytesSize;
          min_protected_data_size_ = leading_clear_bytes_size_ + 1u;
          break;
        default:
          return Status(error::ENCRYPTION_FAILURE,
                        "Only AAC/AC3 and H264 are supported in Sample AES.");
      }
      break;
    }
    case FOURCC_cbcs:
      FALLTHROUGH_INTENDED;
    case FOURCC_cens:
      if (stream_type == kStreamVideo) {
        // Use 1:9 pattern for video.
        crypt_byte_block_ = 1u;
        skip_byte_block_ = 9u;
      } else {
        // Tracks other than video are protected using whole-block full-sample
        // encryption, which is essentially a pattern of 1:0. Note that this may
        // not be the same as the non-pattern based encryption counterparts,
        // e.g. in 'cens' for full sample encryption, the whole sample is
        // encrypted up to the last 16-byte boundary, see 23001-7:2016(E) 9.7;
        // while in 'cenc' for full sample encryption, the last partial 16-byte
        // block is also encrypted, see 23001-7:2016(E) 9.4.2. Another
        // difference is the use of constant iv.
        crypt_byte_block_ = 1u;
        skip_byte_block_ = 0u;
      }
      break;
    default:
      // Not using pattern encryption.
      crypt_byte_block_ = 0u;
      skip_byte_block_ = 0u;
  }
  return Status::OK;
}

bool EncryptionHandler::CreateEncryptor(const EncryptionKey& encryption_key) {
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
    case kAppleSampleAesProtectionScheme:
      if (crypt_byte_block_ == 0 && skip_byte_block_ == 0) {
        encryptor.reset(
            new AesCbcEncryptor(kNoPadding, AesCryptor::kUseConstantIv));
      } else {
        encryptor.reset(new AesPatternCryptor(
            crypt_byte_block_, skip_byte_block_,
            AesPatternCryptor::kSkipIfCryptByteBlockRemaining,
            AesCryptor::kUseConstantIv,
            std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
      }
      break;
    default:
      LOG(ERROR) << "Unsupported protection scheme.";
      return false;
  }

  std::vector<uint8_t> iv = encryption_key.iv;
  if (iv.empty()) {
    if (!AesCryptor::GenerateRandomIv(encryption_options_.protection_scheme,
                                      &iv)) {
      LOG(ERROR) << "Failed to generate random iv.";
      return false;
    }
  }
  const bool initialized =
      encryptor->InitializeWithIv(encryption_key.key, iv);
  encryptor_ = std::move(encryptor);

  encryption_config_.reset(new EncryptionConfig);
  encryption_config_->protection_scheme = encryption_options_.protection_scheme;
  encryption_config_->crypt_byte_block = crypt_byte_block_;
  encryption_config_->skip_byte_block = skip_byte_block_;
  if (encryptor_->use_constant_iv()) {
    encryption_config_->per_sample_iv_size = 0;
    encryption_config_->constant_iv = iv;
  } else {
    encryption_config_->per_sample_iv_size = static_cast<uint8_t>(iv.size());
  }
  encryption_config_->key_id = encryption_key.key_id;
  encryption_config_->key_system_info = encryption_key.key_system_info;
  return initialized;
}

bool EncryptionHandler::EncryptVpxFrame(
    const std::vector<VPxFrameInfo>& vpx_frames,
    MediaSample* sample,
    DecryptConfig* decrypt_config) {
  uint8_t* data = sample->writable_data();
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
    // For consistency, apply block alignment to all frames.
    const uint16_t misalign_bytes = cipher_bytes % kCencBlockSize;
    clear_bytes += misalign_bytes;
    cipher_bytes -= misalign_bytes;

    decrypt_config->AddSubsample(clear_bytes, cipher_bytes);
    if (cipher_bytes > 0)
      EncryptBytes(data + clear_bytes, cipher_bytes);
    data += frame.frame_size;
  }
  // Add subsample for the superframe index if exists.
  const bool is_superframe = vpx_frames.size() > 1;
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
  DCHECK_NE(nalu_length_size_, 0u);
  DCHECK(header_parser_);
  const Nalu::CodecType nalu_type =
      (codec_ == kCodecH265) ? Nalu::kH265 : Nalu::kH264;
  NaluReader reader(nalu_type, nalu_length_size_, sample->writable_data(),
                    sample->data_size());

  // Store the current length of clear data.  This is used to squash
  // multiple unencrypted NAL units into fewer subsample entries.
  uint64_t accumulated_clear_bytes = 0;

  Nalu nalu;
  NaluReader::Result result;
  while ((result = reader.Advance(&nalu)) == NaluReader::kOk) {
    const uint64_t nalu_total_size = nalu.header_size() + nalu.payload_size();
    if (nalu.is_video_slice() && nalu_total_size >= min_protected_data_size_) {
      uint64_t current_clear_bytes = leading_clear_bytes_size_;
      if (current_clear_bytes == 0) {
        // For video-slice NAL units, encrypt the video slice.  This skips
        // the frame header.
        const int64_t video_slice_header_size =
            header_parser_->GetHeaderSize(nalu);
        if (video_slice_header_size < 0) {
          LOG(ERROR) << "Failed to read slice header.";
          return false;
        }
        current_clear_bytes = nalu.header_size() + video_slice_header_size;
      }
      uint64_t cipher_bytes = nalu_total_size - current_clear_bytes;

      // ISO/IEC 23001-7:2016 10.2 'cbc1' 10.3 'cens'
      // The BytesOfProtectedData size SHALL be a multiple of 16 bytes to
      // avoid partial blocks in Subsamples.
      // CMAF requires 'cenc' scheme BytesOfProtectedData SHALL be a multiple
      // of 16 bytes; while 'cbcs' scheme BytesOfProtectedData SHALL start on
      // the first byte of video data following the slice header.
      if (encryption_options_.protection_scheme == FOURCC_cbc1 ||
          encryption_options_.protection_scheme == FOURCC_cens ||
          encryption_options_.protection_scheme == FOURCC_cenc) {
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
      // For non-video-slice or small NAL units, don't encrypt.
      accumulated_clear_bytes += nalu_length_size_ + nalu_total_size;
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
