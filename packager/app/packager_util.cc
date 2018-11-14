// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/packager_util.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"
#include "packager/base/strings/string_split.h"
#include "packager/file/file.h"
#include "packager/media/base/media_handler.h"
#include "packager/media/base/muxer_options.h"
#include "packager/media/base/playready_key_source.h"
#include "packager/media/base/raw_key_source.h"
#include "packager/media/base/request_signer.h"
#include "packager/media/base/widevine_key_source.h"
#include "packager/mpd/base/mpd_options.h"
#include "packager/status.h"

namespace shaka {
namespace media {
namespace {

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

int GetProtectionSystemsFlag(
    const std::vector<EncryptionParams::ProtectionSystem>& protection_systems) {
  int protection_systems_flags = 0;
  for (const auto protection_system : protection_systems) {
    switch (protection_system) {
      case EncryptionParams::ProtectionSystem::kCommonSystem:
        protection_systems_flags |= COMMON_PROTECTION_SYSTEM_FLAG;
        break;
      case EncryptionParams::ProtectionSystem::kFairPlay:
        protection_systems_flags |= FAIRPLAY_PROTECTION_SYSTEM_FLAG;
        break;
      case EncryptionParams::ProtectionSystem::kMarlin:
        protection_systems_flags |= MARLIN_PROTECTION_SYSTEM_FLAG;
        break;
      case EncryptionParams::ProtectionSystem::kPlayReady:
        protection_systems_flags |= PLAYREADY_PROTECTION_SYSTEM_FLAG;
        break;
      case EncryptionParams::ProtectionSystem::kWidevine:
        protection_systems_flags |= WIDEVINE_PROTECTION_SYSTEM_FLAG;
        break;
    }
  }
  return protection_systems_flags;
}

}  // namespace

std::unique_ptr<KeySource> CreateEncryptionKeySource(
    FourCC protection_scheme,
    const EncryptionParams& encryption_params) {
  int protection_systems_flags =
      GetProtectionSystemsFlag(encryption_params.protection_systems);

  std::unique_ptr<KeySource> encryption_key_source;
  switch (encryption_params.key_provider) {
    case KeyProvider::kWidevine: {
      const WidevineEncryptionParams& widevine = encryption_params.widevine;
      if (widevine.key_server_url.empty()) {
        LOG(ERROR) << "'key_server_url' should not be empty.";
        return nullptr;
      }
      if (widevine.content_id.empty()) {
        LOG(ERROR) << "'content_id' should not be empty.";
        return nullptr;
      }
      std::unique_ptr<WidevineKeySource> widevine_key_source(
          new WidevineKeySource(widevine.key_server_url,
                                protection_systems_flags, protection_scheme));
      if (!widevine.signer.signer_name.empty()) {
        std::unique_ptr<RequestSigner> request_signer(
            CreateSigner(widevine.signer));
        if (!request_signer)
          return nullptr;
        widevine_key_source->set_signer(std::move(request_signer));
      }
      widevine_key_source->set_group_id(widevine.group_id);
      widevine_key_source->set_enable_entitlement_license(
          widevine.enable_entitlement_license);

      Status status =
          widevine_key_source->FetchKeys(widevine.content_id, widevine.policy);
      if (!status.ok()) {
        LOG(ERROR) << "Widevine encryption key source failed to fetch keys: "
                   << status.ToString();
        return nullptr;
      }
      encryption_key_source = std::move(widevine_key_source);
      break;
    }
    case KeyProvider::kRawKey: {
      encryption_key_source =
          RawKeySource::Create(encryption_params.raw_key,
                               protection_systems_flags, protection_scheme);
      break;
    }
    case KeyProvider::kPlayReady: {
      const PlayReadyEncryptionParams& playready = encryption_params.playready;
      if (!playready.key_server_url.empty() ||
          !playready.program_identifier.empty()) {
        if (playready.key_server_url.empty() ||
            playready.program_identifier.empty()) {
          LOG(ERROR) << "Either PlayReady key_server_url or program_identifier "
                        "is not set.";
          return nullptr;
        }
        std::unique_ptr<PlayReadyKeySource> playready_key_source;
        // private_key_password is allowed to be empty for unencrypted key.
        if (!playready.client_cert_file.empty() ||
            !playready.client_cert_private_key_file.empty()) {
          if (playready.client_cert_file.empty() ||
              playready.client_cert_private_key_file.empty()) {
            LOG(ERROR) << "Either PlayReady client_cert_file or "
                          "client_cert_private_key_file is not set.";
            return nullptr;
          }
          playready_key_source.reset(new PlayReadyKeySource(
              playready.key_server_url, playready.client_cert_file,
              playready.client_cert_private_key_file,
              playready.client_cert_private_key_password,
              protection_systems_flags, protection_scheme));
        } else {
          playready_key_source.reset(new PlayReadyKeySource(
              playready.key_server_url, protection_systems_flags,
              protection_scheme));
        }
        if (!playready.ca_file.empty()) {
          playready_key_source->SetCaFile(playready.ca_file);
        }
        Status status = playready_key_source->FetchKeysWithProgramIdentifier(
            playready.program_identifier);
        if (!status.ok()) {
          LOG(ERROR) << "PlayReady encryption key source failed to fetch keys: "
                     << status.ToString();
          return nullptr;
        }
        encryption_key_source = std::move(playready_key_source);
      } else {
        LOG(ERROR) << "Error creating PlayReady key source.";
        return nullptr;
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
          new WidevineKeySource(
              widevine.key_server_url,
              WIDEVINE_PROTECTION_SYSTEM_FLAG /* value does not matter here */,
              FOURCC_NULL /* value does not matter here */));
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
      decryption_key_source = RawKeySource::Create(
          decryption_params.raw_key,
          COMMON_PROTECTION_SYSTEM_FLAG /* value does not matter here */,
          FOURCC_NULL /* value does not matter here */);
      break;
    }
    case KeyProvider::kNone:
    case KeyProvider::kPlayReady:
      break;
  }
  return decryption_key_source;
}

MpdOptions GetMpdOptions(bool on_demand_profile, const MpdParams& mpd_params) {
  MpdOptions mpd_options;
  mpd_options.dash_profile =
      on_demand_profile ? DashProfile::kOnDemand : DashProfile::kLive;
  mpd_options.mpd_type =
      (on_demand_profile || mpd_params.generate_static_live_mpd)
          ? MpdType::kStatic
          : MpdType::kDynamic;
  mpd_options.mpd_params = mpd_params;
  return mpd_options;
}

}  // namespace media
}  // namespace shaka
