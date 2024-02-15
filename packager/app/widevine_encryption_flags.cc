// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#include <packager/app/widevine_encryption_flags.h>

#include <string_view>

#include <absl/flags/flag.h>
#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/match.h>

#include <packager/app/validate_flag.h>

ABSL_FLAG(bool,
          enable_widevine_encryption,
          false,
          "Enable encryption with Widevine key server. User should provide "
          "either AES signing key (--aes_signing_key, --aes_signing_iv) or "
          "RSA signing key (--rsa_signing_key_path).");
ABSL_FLAG(bool,
          enable_widevine_decryption,
          false,
          "Enable decryption with Widevine license server/proxy. User should "
          "provide either AES signing key (--aes_signing_key, "
          "--aes_signing_iv) or RSA signing key (--rsa_signing_key_path).");
ABSL_FLAG(std::string,
          key_server_url,
          "",
          "Key server url. Required for encryption and "
          "decryption");
ABSL_FLAG(shaka::HexBytes, content_id, {}, "Content Id (hex).");
ABSL_FLAG(std::string,
          policy,
          "",
          "The name of a stored policy, which specifies DRM content "
          "rights.");
ABSL_FLAG(int32_t,
          max_sd_pixels,
          768 * 576,
          "The video track is considered SD if its max pixels per frame is "
          "no higher than max_sd_pixels. Default: 442368 (768 x 576).");
ABSL_FLAG(int32_t,
          max_hd_pixels,
          1920 * 1080,
          "The video track is considered HD if its max pixels per frame is "
          "higher than max_sd_pixels, but no higher than max_hd_pixels. "
          "Default: 2073600 (1920 x 1080).");
ABSL_FLAG(int32_t,
          max_uhd1_pixels,
          4096 * 2160,
          "The video track is considered UHD1 if its max pixels per frame "
          "is higher than max_hd_pixels, but no higher than max_uhd1_pixels."
          " Otherwise it is UHD2. Default: 8847360 (4096 x 2160).");
ABSL_FLAG(std::string, signer, "", "The name of the signer.");
ABSL_FLAG(shaka::HexBytes,
          aes_signing_key,
          {},
          "AES signing key in hex string. --aes_signing_iv is required. "
          "Exclusive with --rsa_signing_key_path.");
ABSL_FLAG(shaka::HexBytes, aes_signing_iv, {}, "AES signing iv in hex string.");
ABSL_FLAG(std::string,
          rsa_signing_key_path,
          "",
          "Stores PKCS#1 RSA private key for request signing. Exclusive "
          "with --aes_signing_key.");
ABSL_FLAG(int32_t,
          crypto_period_duration,
          0,
          "Crypto period duration in seconds. If it is non-zero, key "
          "rotation is enabled.");
ABSL_FLAG(shaka::HexBytes,
          group_id,
          {},
          "Identifier for a group of licenses (hex).");
ABSL_FLAG(bool,
          enable_entitlement_license,
          false,
          "Enable entitlement license when using Widevine key server.");

namespace shaka {
namespace {
const bool kOptional = true;
}  // namespace

bool ValidateWidevineCryptoFlags() {
  bool success = true;

  const bool widevine_crypto =
      absl::GetFlag(FLAGS_enable_widevine_encryption) ||
      absl::GetFlag(FLAGS_enable_widevine_decryption);
  const char widevine_crypto_label[] =
      "--enable_widevine_encryption/decryption";
  // key_server_url and signer (optional) are associated with
  // enable_widevine_encryption and enable_widevine_descryption.
  if (!ValidateFlag("key_server_url", absl::GetFlag(FLAGS_key_server_url),
                    widevine_crypto, !kOptional, widevine_crypto_label)) {
    success = false;
  }
  if (!ValidateFlag("signer", absl::GetFlag(FLAGS_signer), widevine_crypto,
                    kOptional, widevine_crypto_label)) {
    success = false;
  }
  if (widevine_crypto && absl::GetFlag(FLAGS_signer).empty() &&
      absl::StartsWith(
          absl::AsciiStrToLower(absl::GetFlag(FLAGS_key_server_url)), "http")) {
    LOG(WARNING) << "--signer is likely required with "
                    "--enable_widevine_encryption/decryption.";
  }

  const char widevine_encryption_label[] = "--enable_widevine_encryption";
  // content_id and policy (optional) are associated with
  // enable_widevine_encryption.
  if (!ValidateFlag("content_id", absl::GetFlag(FLAGS_content_id).bytes,
                    absl::GetFlag(FLAGS_enable_widevine_encryption), !kOptional,
                    widevine_encryption_label)) {
    success = false;
  }
  if (!ValidateFlag("policy", absl::GetFlag(FLAGS_policy),
                    absl::GetFlag(FLAGS_enable_widevine_encryption), kOptional,
                    widevine_encryption_label)) {
    success = false;
  }

  if (absl::GetFlag(FLAGS_max_sd_pixels) <= 0) {
    PrintError("--max_sd_pixels must be positive.");
    success = false;
  }
  if (absl::GetFlag(FLAGS_max_hd_pixels) <= 0) {
    PrintError("--max_hd_pixels must be positive.");
    success = false;
  }
  if (absl::GetFlag(FLAGS_max_uhd1_pixels) <= 0) {
    PrintError("--max_uhd1_pixels must be positive.");
    success = false;
  }
  if (absl::GetFlag(FLAGS_max_hd_pixels) <=
      absl::GetFlag(FLAGS_max_sd_pixels)) {
    PrintError("--max_hd_pixels must be greater than --max_sd_pixels.");
    success = false;
  }
  if (absl::GetFlag(FLAGS_max_uhd1_pixels) <=
      absl::GetFlag(FLAGS_max_hd_pixels)) {
    PrintError("--max_uhd1_pixels must be greater than --max_hd_pixels.");
    success = false;
  }

  const bool aes = !absl::GetFlag(FLAGS_aes_signing_key).bytes.empty() ||
                   !absl::GetFlag(FLAGS_aes_signing_iv).bytes.empty();
  if (aes && (absl::GetFlag(FLAGS_aes_signing_key).bytes.empty() ||
              absl::GetFlag(FLAGS_aes_signing_iv).bytes.empty())) {
    PrintError("--aes_signing_key/iv is required if using aes signing.");
    success = false;
  }

  const bool rsa = !absl::GetFlag(FLAGS_rsa_signing_key_path).empty();

  if (absl::GetFlag(FLAGS_signer).empty() && (aes || rsa)) {
    PrintError("--signer is required if using aes/rsa signing.");
    success = false;
  }
  if (!absl::GetFlag(FLAGS_signer).empty() && !aes && !rsa) {
    PrintError(
        "--aes_signing_key/iv or --rsa_signing_key_path is required with "
        "--signer.");
    success = false;
  }
  if (aes && rsa) {
    PrintError(
        "Only one of --aes_signing_key/iv and --rsa_signing_key_path should be "
        "specified.");
    success = false;
  }

  if (absl::GetFlag(FLAGS_crypto_period_duration) < 0) {
    PrintError("--crypto_period_duration should not be negative.");
    success = false;
  }
  return success;
}

}  // namespace shaka
