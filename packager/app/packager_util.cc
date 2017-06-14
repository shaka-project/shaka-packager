// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/packager_util.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/media/base/fixed_key_source.h"
#include "packager/media/base/media_handler.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/playready_key_source.h"
#include "packager/media/base/request_signer.h"
#include "packager/media/base/status.h"
#include "packager/media/base/widevine_key_source.h"
#include "packager/media/chunking/chunking_handler.h"
#include "packager/media/crypto/encryption_handler.h"
#include "packager/media/file/file.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/packager.h"

namespace shaka {
namespace media {
namespace {

FourCC GetProtectionScheme(const std::string& protection_scheme) {
  if (protection_scheme == "cenc") {
    return FOURCC_cenc;
  } else if (protection_scheme == "cens") {
    return FOURCC_cens;
  } else if (protection_scheme == "cbc1") {
    return FOURCC_cbc1;
  } else if (protection_scheme == "cbcs") {
    return FOURCC_cbcs;
  } else {
    LOG(ERROR) << "Unknown protection scheme: " << protection_scheme;
    return FOURCC_NULL;
  }
}

}  // namespace

std::unique_ptr<RequestSigner> CreateSigner(const WidevineSigner& signer) {
  std::unique_ptr<RequestSigner> request_signer;
  switch (signer.signing_key_type) {
    case WidevineSigner::SigningKeyType::kAes:
      request_signer.reset(AesRequestSigner::CreateSigner(
          signer.signer_name, signer.aes.key, signer.aes.iv));
      break;
    case WidevineSigner::SigningKeyType::kRsa:
      request_signer.reset(
          RsaRequestSigner::CreateSigner(signer.signer_name, signer.rsa.key));
      break;
    case WidevineSigner::SigningKeyType::kNone:
      break;
  }
  if (!request_signer)
    LOG(ERROR) << "Failed to create the signer object.";
  return request_signer;
}

std::unique_ptr<KeySource> CreateEncryptionKeySource(
    FourCC protection_scheme,
    const EncryptionParams& encryption_params) {
  std::unique_ptr<KeySource> encryption_key_source;
  switch (encryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      const WidevineEncryptionParams& widevine = encryption_params.widevine;
      if (widevine.key_server_url.empty()) {
        LOG(ERROR) << "'key_server_url' should not be empty.";
        return std::unique_ptr<KeySource>();
      }
      if (widevine.content_id.empty()) {
        LOG(ERROR) << "'content_id' should not be empty.";
        return std::unique_ptr<KeySource>();
      }
      std::unique_ptr<WidevineKeySource> widevine_key_source(
          new WidevineKeySource(widevine.key_server_url,
                                widevine.include_common_pssh));
      widevine_key_source->set_protection_scheme(protection_scheme);
      if (!widevine.signer.signer_name.empty()) {
        std::unique_ptr<RequestSigner> request_signer(
            CreateSigner(widevine.signer));
        if (!request_signer)
          return std::unique_ptr<KeySource>();
        widevine_key_source->set_signer(std::move(request_signer));
      }

      Status status =
          widevine_key_source->FetchKeys(widevine.content_id, widevine.policy);
      if (!status.ok()) {
        LOG(ERROR) << "Widevine encryption key source failed to fetch keys: "
                   << status.ToString();
        return std::unique_ptr<KeySource>();
      }
      encryption_key_source = std::move(widevine_key_source);
      break;
    }
    case KeyProvider::kRawKey: {
      const RawKeyEncryptionParams& raw_key = encryption_params.raw_key;
      const std::string kDefaultTrackType;
      // TODO(kqyang): Refactor FixedKeySource.
      encryption_key_source = FixedKeySource::Create(
          raw_key.key_map.find("")->second.key_id,
          raw_key.key_map.find("")->second.key, raw_key.pssh, raw_key.iv);
      break;
    }
    case KeyProvider::kPlayready: {
      const PlayreadyEncryptionParams& playready = encryption_params.playready;
      if (!playready.key_id.empty() && !playready.key.empty()) {
        encryption_key_source = PlayReadyKeySource::CreateFromKeyAndKeyId(
            playready.key_id, playready.key);
      } else if (!playready.key_server_url.empty() &&
                 !playready.program_identifier.empty()) {
        std::unique_ptr<PlayReadyKeySource> playready_key_source;
        if (!playready.client_cert_file.empty() &&
            !playready.client_cert_private_key_file.empty() &&
            !playready.client_cert_private_key_password.empty()) {
          playready_key_source.reset(new PlayReadyKeySource(
              playready.key_server_url, playready.client_cert_file,
              playready.client_cert_private_key_file,
              playready.client_cert_private_key_password));
        } else {
          playready_key_source.reset(
              new PlayReadyKeySource(playready.key_server_url));
        }
        if (!playready.ca_file.empty()) {
          playready_key_source->SetCaFile(playready.ca_file);
        }
        playready_key_source->FetchKeysWithProgramIdentifier(
            playready.program_identifier);
        encryption_key_source = std::move(playready_key_source);
      } else {
        LOG(ERROR) << "Error creating PlayReady key source.";
        return std::unique_ptr<KeySource>();
      }
      break;
    }
    case KeyProvider::kNone:
      break;
  }
  return encryption_key_source;
}

std::unique_ptr<KeySource> CreateDecryptionKeySource(
    const DecryptionParams& decryption_params) {
  std::unique_ptr<KeySource> decryption_key_source;
  switch (decryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      const WidevineDecryptionParams& widevine = decryption_params.widevine;
      if (widevine.key_server_url.empty()) {
        LOG(ERROR) << "'key_server_url' should not be empty.";
        return std::unique_ptr<KeySource>();
      }
      std::unique_ptr<WidevineKeySource> widevine_key_source(
          new WidevineKeySource(widevine.key_server_url,
                                true /* commmon pssh, does not matter here */));
      if (!widevine.signer.signer_name.empty()) {
        std::unique_ptr<RequestSigner> request_signer(
            CreateSigner(widevine.signer));
        if (!request_signer)
          return std::unique_ptr<KeySource>();
        widevine_key_source->set_signer(std::move(request_signer));
      }

      decryption_key_source = std::move(widevine_key_source);
      break;
    }
    case KeyProvider::kRawKey: {
      const RawKeyDecryptionParams& raw_key = decryption_params.raw_key;
      const std::vector<uint8_t> kNoPssh;
      const std::vector<uint8_t> kNoIv;
      decryption_key_source = FixedKeySource::Create(
          raw_key.key_map.find("")->second.key_id,
          raw_key.key_map.find("")->second.key, kNoPssh, kNoIv);
      break;
    }
    case KeyProvider::kNone:
    case KeyProvider::kPlayready:
      break;
  }
  return decryption_key_source;
}

ChunkingOptions GetChunkingOptions(const ChunkingParams& chunking_params) {
  ChunkingOptions chunking_options;
  chunking_options.segment_duration_in_seconds =
      chunking_params.segment_duration_in_seconds;
  chunking_options.subsegment_duration_in_seconds =
      chunking_params.subsegment_duration_in_seconds;
  chunking_options.segment_sap_aligned = chunking_params.segment_sap_aligned;
  chunking_options.subsegment_sap_aligned =
      chunking_params.subsegment_sap_aligned;
  return chunking_options;
}

EncryptionOptions GetEncryptionOptions(
    const EncryptionParams& encryption_params) {
  EncryptionOptions encryption_options;
  encryption_options.clear_lead_in_seconds =
      encryption_params.clear_lead_in_seconds;
  encryption_options.protection_scheme =
      GetProtectionScheme(encryption_params.protection_scheme);
  encryption_options.crypto_period_duration_in_seconds =
      encryption_params.crypto_period_duration_in_seconds;
  encryption_options.vp9_subsample_encryption =
      encryption_params.vp9_subsample_encryption;
  encryption_options.stream_label_func = encryption_params.stream_label_func;
  return encryption_options;
}

MuxerOptions GetMuxerOptions(const std::string& temp_dir,
                             const Mp4OutputParams& mp4_params) {
  MuxerOptions muxer_options;
  muxer_options.num_subsegments_per_sidx = mp4_params.num_subsegments_per_sidx;
  muxer_options.mp4_include_pssh_in_stream = mp4_params.include_pssh_in_stream;
  muxer_options.mp4_use_decoding_timestamp_in_timeline =
      mp4_params.use_decoding_timestamp_in_timeline;
  muxer_options.temp_dir = temp_dir;
  return muxer_options;
}

MpdOptions GetMpdOptions(bool on_demand_profile, const MpdParams& mpd_params) {
  MpdOptions mpd_options;
  mpd_options.dash_profile =
      on_demand_profile ? DashProfile::kOnDemand : DashProfile::kLive;
  mpd_options.mpd_type =
      (on_demand_profile || mpd_params.generate_static_live_mpd)
          ? MpdType::kStatic
          : MpdType::kDynamic;
  mpd_options.minimum_update_period = mpd_params.minimum_update_period;
  mpd_options.min_buffer_time = mpd_params.min_buffer_time;
  mpd_options.time_shift_buffer_depth = mpd_params.time_shift_buffer_depth;
  mpd_options.suggested_presentation_delay =
      mpd_params.suggested_presentation_delay;
  mpd_options.default_language = mpd_params.default_language;
  return mpd_options;
}

Status ConnectHandlers(std::vector<std::shared_ptr<MediaHandler>>& handlers) {
  size_t num_handlers = handlers.size();
  Status status;
  for (size_t i = 1; i < num_handlers; ++i) {
    status.Update(handlers[i - 1]->AddHandler(handlers[i]));
  }
  return status;
}

}  // namespace media
}  // namespace shaka
