// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/media/crypto/encryption_handler.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <absl/log/check.h>

#include <packager/macros/logging.h>
#include <packager/macros/status.h>
#include <packager/media/base/aes_encryptor.h>
#include <packager/media/base/audio_stream_info.h>
#include <packager/media/base/common_pssh_generator.h>
#include <packager/media/base/key_source.h>
#include <packager/media/base/media_sample.h>
#include <packager/media/base/playready_pssh_generator.h>
#include <packager/media/base/protection_system_ids.h>
#include <packager/media/base/video_stream_info.h>
#include <packager/media/base/widevine_pssh_generator.h>
#include <packager/media/crypto/aes_encryptor_factory.h>
#include <packager/media/crypto/subsample_generator.h>

namespace shaka {
namespace media {

namespace {
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

bool IsPatternEncryptionScheme(FourCC protection_scheme) {
  return protection_scheme == kAppleSampleAesProtectionScheme ||
         protection_scheme == FOURCC_cbcs || protection_scheme == FOURCC_cens;
}

void FillPsshGenerators(
    const EncryptionParams& encryption_params,
    std::vector<std::unique_ptr<PsshGenerator>>* pssh_generators,
    std::vector<std::vector<uint8_t>>* no_pssh_systems) {
  if (has_flag(encryption_params.protection_systems,
               ProtectionSystem::kCommon)) {
    pssh_generators->emplace_back(new CommonPsshGenerator());
  }

  if (has_flag(encryption_params.protection_systems,
               ProtectionSystem::kPlayReady)) {
    pssh_generators->emplace_back(new PlayReadyPsshGenerator(
        encryption_params.playready_extra_header_data,
        static_cast<FourCC>(encryption_params.protection_scheme)));
  }

  if (has_flag(encryption_params.protection_systems,
               ProtectionSystem::kWidevine)) {
    pssh_generators->emplace_back(new WidevinePsshGenerator(
        static_cast<FourCC>(encryption_params.protection_scheme)));
  }

  if (has_flag(encryption_params.protection_systems,
               ProtectionSystem::kFairPlay)) {
    no_pssh_systems->emplace_back(std::begin(kFairPlaySystemId),
                                  std::end(kFairPlaySystemId));
  }
  // We only support Marlin Adaptive Streaming Specification – Simple Profile
  // with Implicit Content ID Mapping, which does not need a PSSH. Marlin
  // specific PSSH with Explicit Content ID Mapping is not generated.
  if (has_flag(encryption_params.protection_systems,
               ProtectionSystem::kMarlin)) {
    no_pssh_systems->emplace_back(std::begin(kMarlinSystemId),
                                  std::end(kMarlinSystemId));
  }

  if (pssh_generators->empty() && no_pssh_systems->empty() &&
      (encryption_params.key_provider != KeyProvider::kRawKey ||
       encryption_params.raw_key.pssh.empty())) {
    pssh_generators->emplace_back(new CommonPsshGenerator());
  }
}

void AddProtectionSystemIfNotExist(
    const ProtectionSystemSpecificInfo& pssh_info,
    EncryptionConfig* encryption_config) {
  for (const auto& info : encryption_config->key_system_info) {
    if (info.system_id == pssh_info.system_id)
      return;
  }
  encryption_config->key_system_info.push_back(pssh_info);
}

Status FillProtectionSystemInfo(const EncryptionParams& encryption_params,
                                const EncryptionKey& encryption_key,
                                EncryptionConfig* encryption_config) {
  // If generating dummy keys for key rotation, don't generate PSSH info.
  if (encryption_key.key_ids.empty())
    return Status::OK;

  std::vector<std::unique_ptr<PsshGenerator>> pssh_generators;
  std::vector<std::vector<uint8_t>> no_pssh_systems;
  FillPsshGenerators(encryption_params, &pssh_generators, &no_pssh_systems);

  encryption_config->key_system_info = encryption_key.key_system_info;
  for (const auto& pssh_generator : pssh_generators) {
    const bool support_multiple_keys = pssh_generator->SupportMultipleKeys();
    if (support_multiple_keys) {
      ProtectionSystemSpecificInfo info;
      RETURN_IF_ERROR(pssh_generator->GeneratePsshFromKeyIds(
          encryption_key.key_ids, &info));
      AddProtectionSystemIfNotExist(info, encryption_config);
    } else {
      ProtectionSystemSpecificInfo info;
      RETURN_IF_ERROR(pssh_generator->GeneratePsshFromKeyIdAndKey(
          encryption_key.key_id, encryption_key.key, &info));
      AddProtectionSystemIfNotExist(info, encryption_config);
    }
  }

  for (const auto& no_pssh_system : no_pssh_systems) {
    ProtectionSystemSpecificInfo info;
    info.system_id = no_pssh_system;
    AddProtectionSystemIfNotExist(info, encryption_config);
  }

  return Status::OK;
}

}  // namespace

EncryptionHandler::EncryptionHandler(const EncryptionParams& encryption_params,
                                     KeySource* key_source)
    : encryption_params_(encryption_params),
      protection_scheme_(
          static_cast<FourCC>(encryption_params.protection_scheme)),
      key_source_(key_source),
      subsample_generator_(
          new SubsampleGenerator(encryption_params.vp9_subsample_encryption)),
      encryptor_factory_(new AesEncryptorFactory) {}

EncryptionHandler::~EncryptionHandler() = default;

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
  RETURN_IF_ERROR(
      subsample_generator_->Initialize(protection_scheme_, *stream_info));

  remaining_clear_lead_ =
      encryption_params_.clear_lead_in_seconds * stream_info->time_scale();
  crypto_period_duration_ =
      encryption_params_.crypto_period_duration_in_seconds *
      stream_info->time_scale();
  codec_ = stream_info->codec();
  stream_label_ = GetStreamLabelForEncryption(
      *stream_info, encryption_params_.stream_label_func);

  SetupProtectionPattern(stream_info->stream_type());

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

  // Process the frame even if the frame is not encrypted as the next
  // (encrypted) frame may be dependent on this clear frame.
  std::vector<SubsampleEntry> subsamples;
  RETURN_IF_ERROR(subsample_generator_->GenerateSubsamples(
      clear_sample->data(), clear_sample->data_size(), &subsamples));

  // Need to setup the encryptor for new segments even if this segment does not
  // need to be encrypted, so we can signal encryption metadata earlier to
  // allows clients to prefetch the keys.
  if (check_new_crypto_period_) {
    // |dts| can be negative, e.g. after EditList adjustments. Normalized to 0
    // in that case.
    const int64_t dts = std::max(clear_sample->dts(), static_cast<int64_t>(0));
    const int64_t current_crypto_period_index = dts / crypto_period_duration_;
    const int32_t crypto_period_duration_in_seconds = static_cast<int32_t>(
        encryption_params_.crypto_period_duration_in_seconds);
    if (current_crypto_period_index != prev_crypto_period_index_) {
      EncryptionKey encryption_key;
      RETURN_IF_ERROR(key_source_->GetCryptoPeriodKey(
          current_crypto_period_index, crypto_period_duration_in_seconds,
          stream_label_, &encryption_key));
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

  size_t ciphertext_size =
      encryptor_->RequiredOutputSize(clear_sample->data_size());

  std::shared_ptr<uint8_t> cipher_sample_data(new uint8_t[ciphertext_size],
                                              std::default_delete<uint8_t[]>());

  const uint8_t* source = clear_sample->data();
  uint8_t* dest = cipher_sample_data.get();
  if (!subsamples.empty()) {
    size_t total_size = 0;
    for (const SubsampleEntry& subsample : subsamples) {
      if (subsample.clear_bytes > 0) {
        // clear_bytes is the number of bytes to leave in the clear
        memcpy(dest, source, subsample.clear_bytes);
        source += subsample.clear_bytes;
        dest += subsample.clear_bytes;
        total_size += subsample.clear_bytes;
      }
      if (subsample.cipher_bytes > 0) {
        // cipher_bytes is the number of bytes we want to encrypt
        EncryptBytes(source, subsample.cipher_bytes, dest, ciphertext_size);
        source += subsample.cipher_bytes;
        dest += subsample.cipher_bytes;
        total_size += subsample.cipher_bytes;
      }
    }
    DCHECK_EQ(total_size, clear_sample->data_size());
  } else {
    EncryptBytes(source, clear_sample->data_size(), dest, ciphertext_size);
  }

  std::shared_ptr<MediaSample> cipher_sample(clear_sample->Clone());
  cipher_sample->TransferData(std::move(cipher_sample_data),
                              clear_sample->data_size());

  // Finish initializing the sample before sending it downstream. We must
  // wait until now to finish the initialization as we will lose access to
  // |decrypt_config| once we set it.
  cipher_sample->set_is_encrypted(true);
  std::unique_ptr<DecryptConfig> decrypt_config(new DecryptConfig(
      encryption_config_->key_id, encryptor_->iv(), subsamples,
      protection_scheme_, crypt_byte_block_, skip_byte_block_));
  cipher_sample->set_decrypt_config(std::move(decrypt_config));

  encryptor_->UpdateIv();

  return DispatchMediaSample(kStreamIndex, std::move(cipher_sample));
}

void EncryptionHandler::SetupProtectionPattern(StreamType stream_type) {
  if (stream_type == kStreamVideo &&
      IsPatternEncryptionScheme(protection_scheme_)) {
    crypt_byte_block_ = encryption_params_.crypt_byte_block;
    skip_byte_block_ = encryption_params_.skip_byte_block;
  } else {
    // Audio stream in pattern encryption scheme does not use pattern; it uses
    // whole-block full sample encryption instead. Non-pattern encryption does
    // not have pattern.
    crypt_byte_block_ = 0u;
    skip_byte_block_ = 0u;
  }
}

bool EncryptionHandler::CreateEncryptor(const EncryptionKey& encryption_key) {
  std::unique_ptr<AesCryptor> encryptor = encryptor_factory_->CreateEncryptor(
      protection_scheme_, crypt_byte_block_, skip_byte_block_, codec_,
      encryption_key.key, encryption_key.iv);
  if (!encryptor)
    return false;
  encryptor_ = std::move(encryptor);

  encryption_config_.reset(new EncryptionConfig);
  encryption_config_->protection_scheme = protection_scheme_;
  encryption_config_->crypt_byte_block = crypt_byte_block_;
  encryption_config_->skip_byte_block = skip_byte_block_;

  const std::vector<uint8_t>& iv = encryptor_->iv();
  if (encryptor_->use_constant_iv()) {
    encryption_config_->per_sample_iv_size = 0;
    encryption_config_->constant_iv = iv;
  } else {
    encryption_config_->per_sample_iv_size = static_cast<uint8_t>(iv.size());
  }

  encryption_config_->key_id = encryption_key.key_id;
  const auto status = FillProtectionSystemInfo(
      encryption_params_, encryption_key, encryption_config_.get());
  return status.ok();
}

void EncryptionHandler::EncryptBytes(const uint8_t* source,
                                     size_t source_size,
                                     uint8_t* dest,
                                     size_t dest_size) {
  DCHECK(source);
  DCHECK(dest);
  DCHECK(encryptor_);
  CHECK(encryptor_->Crypt(source, source_size, dest, &dest_size));
}

void EncryptionHandler::InjectSubsampleGeneratorForTesting(
    std::unique_ptr<SubsampleGenerator> generator) {
  subsample_generator_ = std::move(generator);
}

void EncryptionHandler::InjectEncryptorFactoryForTesting(
    std::unique_ptr<AesEncryptorFactory> encryptor_factory) {
  encryptor_factory_ = std::move(encryptor_factory);
}

}  // namespace media
}  // namespace shaka
