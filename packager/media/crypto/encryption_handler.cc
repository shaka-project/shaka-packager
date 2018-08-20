// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/crypto/encryption_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>

#include "packager/media/base/aes_encryptor.h"
#include "packager/media/base/aes_pattern_cryptor.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/key_source.h"
#include "packager/media/base/media_sample.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/codecs/video_slice_header_parser.h"
#include "packager/media/codecs/vp8_parser.h"
#include "packager/media/codecs/vp9_parser.h"
#include "packager/media/crypto/sample_aes_ec3_cryptor.h"
#include "packager/status_macros.h"

namespace shaka {
namespace media {

namespace {
const size_t kCencBlockSize = 16u;

// The encryption handler only supports a single output.
const size_t kStreamIndex = 0;

// The default KID, KEY and IV for key rotation are all 0s.
// They are placeholders and are not really being used to encrypt data.
const uint8_t kKeyRotationDefaultKeyId[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
const uint8_t kKeyRotationDefaultKey[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
const uint8_t kKeyRotationDefaultIv[] = {
    0, 0, 0, 0, 0, 0, 0, 0,
};

// Adds one or more subsamples to |*decrypt_config|.  This may add more than one
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

EncryptionHandler::EncryptionHandler(const EncryptionParams& encryption_params,
                                     KeySource* key_source)
    : encryption_params_(encryption_params),
      protection_scheme_(
          static_cast<FourCC>(encryption_params.protection_scheme)),
      key_source_(key_source) {}

EncryptionHandler::~EncryptionHandler() {}

Status EncryptionHandler::InitializeInternal() {
  if (!encryption_params_.stream_label_func) {
    return Status(error::INVALID_ARGUMENT, "Stream label function not set.");
  }
  if (num_input_streams() != 1 || next_output_stream_index() != 1) {
    return Status(error::INVALID_ARGUMENT,
                  "Expects exactly one input and output.");
  }
  return Status::OK;
}

Status EncryptionHandler::Process(std::unique_ptr<StreamData> stream_data) {
  switch (stream_data->stream_data_type) {
    case StreamDataType::kStreamInfo:
      return ProcessStreamInfo(*stream_data->stream_info);
    case StreamDataType::kSegmentInfo: {
      std::shared_ptr<SegmentInfo> segment_info(new SegmentInfo(
          *stream_data->segment_info));

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

      return DispatchSegmentInfo(kStreamIndex, segment_info);
    }
    case StreamDataType::kMediaSample:
      return ProcessMediaSample(std::move(stream_data->media_sample));
    default:
      VLOG(3) << "Stream data type "
              << static_cast<int>(stream_data->stream_data_type) << " ignored.";
      return Dispatch(std::move(stream_data));
  }
}

Status EncryptionHandler::ProcessStreamInfo(const StreamInfo& clear_info) {
  if (clear_info.is_encrypted()) {
    return Status(error::INVALID_ARGUMENT,
                  "Input stream is already encrypted.");
  }

  DCHECK_NE(kStreamUnknown, clear_info.stream_type());
  DCHECK_NE(kStreamText, clear_info.stream_type());
  std::shared_ptr<StreamInfo> stream_info = clear_info.Clone();

  remaining_clear_lead_ =
      encryption_params_.clear_lead_in_seconds * stream_info->time_scale();
  crypto_period_duration_ =
      encryption_params_.crypto_period_duration_in_seconds *
      stream_info->time_scale();
  codec_ = stream_info->codec();
  nalu_length_size_ = GetNaluLengthSize(*stream_info);
  stream_label_ = GetStreamLabelForEncryption(
      *stream_info, encryption_params_.stream_label_func);
  switch (codec_) {
    case kCodecVP9:
      if (encryption_params_.vp9_subsample_encryption)
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

  RETURN_IF_ERROR(SetupProtectionPattern(stream_info->stream_type()));

  EncryptionKey encryption_key;
  const bool key_rotation_enabled = crypto_period_duration_ != 0;
  if (key_rotation_enabled) {
    check_new_crypto_period_ = true;
    // Setup dummy key id, key and iv to signal encryption for key rotation.
    encryption_key.key_id.assign(std::begin(kKeyRotationDefaultKeyId),
                                 std::end(kKeyRotationDefaultKeyId));
    encryption_key.key.assign(std::begin(kKeyRotationDefaultKey),
                              std::end(kKeyRotationDefaultKey));
    encryption_key.iv.assign(std::begin(kKeyRotationDefaultIv),
                             std::end(kKeyRotationDefaultIv));
  } else {
    RETURN_IF_ERROR(key_source_->GetKey(stream_label_, &encryption_key));
  }
  if (!CreateEncryptor(encryption_key))
    return Status(error::ENCRYPTION_FAILURE, "Failed to create encryptor");

  stream_info->set_is_encrypted(true);
  stream_info->set_has_clear_lead(encryption_params_.clear_lead_in_seconds > 0);
  stream_info->set_encryption_config(*encryption_config_);

  return DispatchStreamInfo(kStreamIndex, stream_info);
}

Status EncryptionHandler::ProcessMediaSample(
    std::shared_ptr<const MediaSample> clear_sample) {
  DCHECK(clear_sample);

  // We need to parse the frame (which also updates the vpx parser) even if the
  // frame is not encrypted as the next (encrypted) frame may be dependent on
  // this clear frame.
  std::vector<VPxFrameInfo> vpx_frames;
  if (vpx_parser_ && !vpx_parser_->Parse(clear_sample->data(),
                                         clear_sample->data_size(),
                                         &vpx_frames)) {
    return Status(error::ENCRYPTION_FAILURE, "Failed to parse vpx frame.");
  }

  // Need to setup the encryptor for new segments even if this segment does not
  // need to be encrypted, so we can signal encryption metadata earlier to
  // allows clients to prefetch the keys.
  if (check_new_crypto_period_) {
    // |dts| can be negative, e.g. after EditList adjustments. Normalized to 0
    // in that case.
    const int64_t dts = std::max(clear_sample->dts(), static_cast<int64_t>(0));
    const int64_t current_crypto_period_index = dts / crypto_period_duration_;
    if (current_crypto_period_index != prev_crypto_period_index_) {
      EncryptionKey encryption_key;
      RETURN_IF_ERROR(key_source_->GetCryptoPeriodKey(
          current_crypto_period_index, stream_label_, &encryption_key));
      if (!CreateEncryptor(encryption_key))
        return Status(error::ENCRYPTION_FAILURE, "Failed to create encryptor");
      prev_crypto_period_index_ = current_crypto_period_index;
    }
    check_new_crypto_period_ = false;
  }

  // Since there is no encryption needed right now, send the clear copy
  // downstream so we can save the costs of copying it.
  if (remaining_clear_lead_ > 0) {
    return DispatchMediaSample(kStreamIndex, std::move(clear_sample));
  }

  std::unique_ptr<DecryptConfig> decrypt_config(new DecryptConfig(
      encryption_config_->key_id,
      encryptor_->iv(),
      std::vector<SubsampleEntry>(),
      protection_scheme_,
      crypt_byte_block_,
      skip_byte_block_));

  // Now that we know that this sample must be encrypted, make a copy of
  // the sample first so that all the encryption operations can be done
  // in-place.
  std::shared_ptr<MediaSample> cipher_sample(clear_sample->Clone());
  // |cipher_sample| above still contains the old clear sample data. We will
  // use |cipher_sample_data| to hold cipher sample data then transfer it to
  // |cipher_sample| after encryption.
  std::shared_ptr<uint8_t> cipher_sample_data(
      new uint8_t[clear_sample->data_size()], std::default_delete<uint8_t[]>());

  if (vpx_parser_) {
    if (!EncryptVpxFrame(vpx_frames, clear_sample->data(),
                         clear_sample->data_size(),
                         &cipher_sample_data.get()[0], decrypt_config.get())) {
      return Status(error::ENCRYPTION_FAILURE, "Failed to encrypt VPX frame.");
    }
    DCHECK_EQ(decrypt_config->GetTotalSizeOfSubsamples(),
              clear_sample->data_size());
  } else if (header_parser_) {
    if (!EncryptNalFrame(clear_sample->data(), clear_sample->data_size(),
                         &cipher_sample_data.get()[0], decrypt_config.get())) {
      return Status(error::ENCRYPTION_FAILURE, "Failed to encrypt NAL frame.");
    }
    DCHECK_EQ(decrypt_config->GetTotalSizeOfSubsamples(),
              clear_sample->data_size());
  } else {
    memcpy(&cipher_sample_data.get()[0], clear_sample->data(),
           std::min(clear_sample->data_size(), leading_clear_bytes_size_));
    if (clear_sample->data_size() > leading_clear_bytes_size_) {
      // The residual block is left unecrypted (copied without encryption). No
      // need to do special handling here.
      EncryptBytes(clear_sample->data() + leading_clear_bytes_size_,
                   clear_sample->data_size() - leading_clear_bytes_size_,
                   &cipher_sample_data.get()[leading_clear_bytes_size_]);
    }
  }

  cipher_sample->TransferData(std::move(cipher_sample_data),
                              clear_sample->data_size());
  // Finish initializing the sample before sending it downstream. We must
  // wait until now to finish the initialization as we will lose access to
  // |decrypt_config| once we set it.
  cipher_sample->set_is_encrypted(true);
  cipher_sample->set_decrypt_config(std::move(decrypt_config));

  encryptor_->UpdateIv();

  return DispatchMediaSample(kStreamIndex, std::move(cipher_sample));
}

Status EncryptionHandler::SetupProtectionPattern(StreamType stream_type) {
  switch (protection_scheme_) {
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
          FALLTHROUGH_INTENDED;
        case kCodecEAC3:
          // Audio is whole sample encrypted. We could not use a
          // crypto_byte_block_ of 1 here as if there is one crypto block
          // remaining, it need not be encrypted for video but it needs to be
          // encrypted for audio.
          crypt_byte_block_ = 0u;
          skip_byte_block_ = 0u;
          // E-AC3 encryption is handled by SampleAesEc3Cryptor, which also
          // manages leading clear bytes.
          leading_clear_bytes_size_ =
              codec_ == kCodecEAC3 ? 0 : kAudioLeadingClearBytesSize;
          min_protected_data_size_ = leading_clear_bytes_size_ + 15u;
          break;
        default:
          return Status(
              error::ENCRYPTION_FAILURE,
              "Only AAC/AC3/EAC3 and H264 are supported in Sample AES.");
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
        // encryption. Note that this may not be the same as the non-pattern
        // based encryption counterparts, e.g. in 'cens' whole-block full sample
        // encryption, the whole sample is encrypted up to the last 16-byte
        // boundary, see 23001-7:2016(E) 9.7; while in 'cenc' full sample
        // encryption, the last partial 16-byte block is also encrypted, see
        // 23001-7:2016(E) 9.4.2. Another difference is the use of constant iv.
        crypt_byte_block_ = 0u;
        skip_byte_block_ = 0u;
      }
      break;
    default:
      // Not using pattern encryption.
      crypt_byte_block_ = 0u;
      skip_byte_block_ = 0u;
      break;
  }
  return Status::OK;
}

bool EncryptionHandler::CreateEncryptor(const EncryptionKey& encryption_key) {
  std::unique_ptr<AesCryptor> encryptor;
  switch (protection_scheme_) {
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
        if (codec_ == kCodecEAC3) {
          encryptor.reset(new SampleAesEc3Cryptor(
              std::unique_ptr<AesCryptor>(new AesCbcEncryptor(kNoPadding))));
        } else {
          encryptor.reset(
              new AesCbcEncryptor(kNoPadding, AesCryptor::kUseConstantIv));
        }
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
    if (!AesCryptor::GenerateRandomIv(protection_scheme_, &iv)) {
      LOG(ERROR) << "Failed to generate random iv.";
      return false;
    }
  }
  const bool initialized =
      encryptor->InitializeWithIv(encryption_key.key, iv);
  encryptor_ = std::move(encryptor);

  encryption_config_.reset(new EncryptionConfig);
  encryption_config_->protection_scheme = protection_scheme_;
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
    const uint8_t* source,
    size_t source_size,
    uint8_t* dest,
    DecryptConfig* decrypt_config) {
  const uint8_t* data = source;
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
    memcpy(dest, data, clear_bytes);
    if (cipher_bytes > 0)
      EncryptBytes(data + clear_bytes, cipher_bytes, dest + clear_bytes);
    data += frame.frame_size;
    dest += frame.frame_size;
  }
  // Add subsample for the superframe index if exists.
  const bool is_superframe = vpx_frames.size() > 1;
  if (is_superframe) {
    size_t index_size = source + source_size - data;
    DCHECK_LE(index_size, 2 + vpx_frames.size() * 4);
    DCHECK_GE(index_size, 2 + vpx_frames.size() * 1);
    uint16_t clear_bytes = static_cast<uint16_t>(index_size);
    uint32_t cipher_bytes = 0;
    decrypt_config->AddSubsample(clear_bytes, cipher_bytes);
    memcpy(dest, data, clear_bytes);
  }
  return true;
}

bool EncryptionHandler::EncryptNalFrame(const uint8_t* source,
                                        size_t source_size,
                                        uint8_t* dest,
                                        DecryptConfig* decrypt_config) {
  DCHECK_NE(nalu_length_size_, 0u);
  DCHECK(header_parser_);
  const Nalu::CodecType nalu_type =
      (codec_ == kCodecH265) ? Nalu::kH265 : Nalu::kH264;
  NaluReader reader(nalu_type, nalu_length_size_, source, source_size);

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
      if (protection_scheme_ == FOURCC_cbc1 ||
          protection_scheme_ == FOURCC_cens ||
          protection_scheme_ == FOURCC_cenc) {
        const uint16_t misalign_bytes = cipher_bytes % kCencBlockSize;
        current_clear_bytes += misalign_bytes;
        cipher_bytes -= misalign_bytes;
      }

      accumulated_clear_bytes += nalu_length_size_ + current_clear_bytes;
      AddSubsample(accumulated_clear_bytes, cipher_bytes, decrypt_config);
      memcpy(dest, source, accumulated_clear_bytes);
      source += accumulated_clear_bytes;
      dest += accumulated_clear_bytes;
      accumulated_clear_bytes = 0;

      DCHECK_EQ(nalu.data() + current_clear_bytes, source);
      EncryptBytes(source, cipher_bytes, dest);
      source += cipher_bytes;
      dest += cipher_bytes;
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
  memcpy(dest, source, accumulated_clear_bytes);
  return true;
}

void EncryptionHandler::EncryptBytes(const uint8_t* source,
                                     size_t source_size,
                                     uint8_t* dest) {
  DCHECK(source);
  DCHECK(dest);
  DCHECK(encryptor_);
  CHECK(encryptor_->Crypt(source, source_size, dest));
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
