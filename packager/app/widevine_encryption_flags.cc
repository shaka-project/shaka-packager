// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#include "packager/app/widevine_encryption_flags.h"

#include "packager/app/validate_flag.h"
#include "packager/base/logging.h"
#include "packager/base/strings/string_util.h"

DEFINE_bool(enable_widevine_encryption,
            false,
            "Enable encryption with Widevine license server/proxy. User should "
            "provide either AES signing key (--aes_signing_key, "
            "--aes_signing_iv) or RSA signing key (--rsa_signing_key_path).");
DEFINE_bool(enable_widevine_decryption,
            false,
            "Enable decryption with Widevine license server/proxy. User should "
            "provide either AES signing key (--aes_signing_key, "
            "--aes_signing_iv) or RSA signing key (--rsa_signing_key_path).");
DEFINE_string(key_server_url, "", "Key server url. Required for encryption and "
              "decryption");
DEFINE_string(content_id, "", "Content Id (hex).");
DEFINE_string(policy,
              "",
              "The name of a stored policy, which specifies DRM content "
              "rights.");
DEFINE_int32(max_sd_pixels,
             768 * 576,
             "If the video track has more pixels per frame than max_sd_pixels, "
             "it is considered as HD, SD otherwise. Default: 768 * 576.");
DEFINE_string(signer, "", "The name of the signer.");
DEFINE_string(aes_signing_key,
              "",
              "AES signing key in hex string. --aes_signing_iv is required. "
              "Exclusive with --rsa_signing_key_path.");
DEFINE_string(aes_signing_iv,
              "",
              "AES signing iv in hex string.");
DEFINE_string(rsa_signing_key_path,
              "",
              "Stores PKCS#1 RSA private key for request signing. Exclusive "
              "with --aes_signing_key.");
DEFINE_int32(crypto_period_duration,
             0,
             "Crypto period duration in seconds. If it is non-zero, key "
             "rotation is enabled.");

namespace edash_packager {

bool ValidateWidevineCryptoFlags() {
  bool success = true;

  const bool widevine_crypto =
      FLAGS_enable_widevine_encryption || FLAGS_enable_widevine_decryption;
  const char widevine_crypto_label[] =
      "--enable_widevine_encryption/decryption";
  // key_server_url and signer (optional) are associated with
  // enable_widevine_encryption and enable_widevine_descryption.
  if (!ValidateFlag("key_server_url",
                    FLAGS_key_server_url,
                    widevine_crypto,
                    false,
                    widevine_crypto_label)) {
    success = false;
  }
  if (!ValidateFlag("signer",
                    FLAGS_signer,
                    widevine_crypto,
                    true,
                    widevine_crypto_label)) {
    success = false;
  }
  if (widevine_crypto && FLAGS_signer.empty() &&
      base::StartsWith(FLAGS_key_server_url, "http",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    LOG(WARNING) << "--signer is likely required with "
                    "--enable_widevine_encryption/decryption.";
  }

  const char widevine_encryption_label[] = "--enable_widevine_encryption";
  // content_id and policy (optional) are associated with
  // enable_widevine_encryption.
  if (!ValidateFlag("content_id",
                    FLAGS_content_id,
                    FLAGS_enable_widevine_encryption,
                    false,
                    widevine_encryption_label)) {
    success = false;
  }
  if (!ValidateFlag("policy",
                    FLAGS_policy,
                    FLAGS_enable_widevine_encryption,
                    true,
                    widevine_encryption_label)) {
    success = false;
  }

  if (FLAGS_max_sd_pixels <= 0) {
    PrintError("--max_sd_pixels must be positive.");
    success = false;
  }

  const bool aes = !FLAGS_signer.empty() && FLAGS_rsa_signing_key_path.empty();
  const char aes_label[] =
      "--signer is specified and exclusive with --rsa_signing_key_path";
  // aes_signer_key and aes_signing_iv are associated with aes signing.
  if (!ValidateFlag(
          "aes_signing_key", FLAGS_aes_signing_key, aes, true, aes_label)) {
    success = false;
  }
  if (!ValidateFlag(
          "aes_signing_iv", FLAGS_aes_signing_iv, aes, true, aes_label)) {
    success = false;
  }

  const bool rsa = !FLAGS_signer.empty() && FLAGS_aes_signing_key.empty() &&
                   FLAGS_aes_signing_iv.empty();
  const char rsa_label[] =
      "--signer is specified and exclusive with --aes_signing_key/iv";
  // rsa_signing_key_path is associated with rsa_signing.
  if (!ValidateFlag("rsa_signing_key_path",
                    FLAGS_rsa_signing_key_path,
                    rsa,
                    true,
                    rsa_label)) {
    success = false;
  }

  if (!FLAGS_signer.empty() &&
      (FLAGS_aes_signing_key.empty() || FLAGS_aes_signing_iv.empty()) &&
      FLAGS_rsa_signing_key_path.empty()) {
    PrintError(
        "--aes_signing_key/iv or --rsa_signing_key_path is required with "
        "--signer.");
    success = false;
  }

  if (FLAGS_crypto_period_duration < 0) {
    PrintError("--crypto_period_duration should not be negative.");
    success = false;
  }
  return success;
}

}  // namespace edash_packager
