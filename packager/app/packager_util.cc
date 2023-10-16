// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/app/packager_util.h>

#include <absl/log/log.h>

#include <packager/file.h>
#include <packager/media/base/media_handler.h>
#include <packager/media/base/muxer_options.h>
#include <packager/media/base/playready_key_source.h>
#include <packager/media/base/raw_key_source.h>
#include <packager/media/base/request_signer.h>
#include <packager/media/base/widevine_key_source.h>
#include <packager/mpd/base/mpd_options.h>

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

}  // namespace

std::unique_ptr<KeySource> CreateEncryptionKeySource(
    FourCC protection_scheme,
    const EncryptionParams& encryption_params) {
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
                                encryption_params.protection_systems,
                                protection_scheme));
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
      encryption_key_source = RawKeySource::Create(encryption_params.raw_key);
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
        playready_key_source.reset(new PlayReadyKeySource(
            playready.key_server_url, encryption_params.protection_systems));
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
    default:
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
              ProtectionSystem::kWidevine /* value does not matter here */,
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
      decryption_key_source = RawKeySource::Create(decryption_params.raw_key);
      break;
    }
    default:
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
