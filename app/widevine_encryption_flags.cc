// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#include "app/widevine_encryption_flags.h"

#include "base/strings/string_number_conversions.h"

DEFINE_bool(enable_widevine_encryption,
            false,
            "Enable encryption with Widevine license server/proxy. User should "
            "provide either AES signing key (--aes_signing_key, "
            "--aes_signing_iv) or RSA signing key (--rsa_signing_key_path).");
DEFINE_string(key_server_url, "", "Key server url.");
DEFINE_string(content_id, "", "Content Id.");
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

namespace {

static bool IsNotEmptyWithWidevineEncryption(const char* flag_name,
                                             const std::string& flag_value) {
  return FLAGS_enable_widevine_encryption ? !flag_value.empty() : true;
}

static bool IsPositive(const char* flag_name, int flag_value) {
  return flag_value > 0;
}

static bool VerifyAesRsaKey(const char* flag_name,
                            const std::string& flag_value) {
  if (!FLAGS_enable_widevine_encryption)
    return true;
  const std::string flag_name_str = flag_name;
  if (flag_name_str == "aes_signing_iv") {
    if (!FLAGS_aes_signing_key.empty() && flag_value.empty()) {
      fprintf(stderr,
              "ERROR: --aes_signing_iv is required for --aes_signing_key.\n");
      return false;
    }
  } else if (flag_name_str == "rsa_signing_key_path") {
    if (FLAGS_aes_signing_key.empty() && flag_value.empty()) {
      fprintf(stderr,
              "ERROR: --aes_signing_key or --rsa_signing_key_path is "
              "required.\n");
      return false;
    }
    if (!FLAGS_aes_signing_key.empty() && !flag_value.empty()) {
      fprintf(stderr,
              "ERROR: --aes_signing_key and --rsa_signing_key_path are "
              "exclusive.\n");
      return false;
    }
  }
  return true;
}

bool dummy_key_server_url_validator =
    google::RegisterFlagValidator(&FLAGS_key_server_url,
                                  &IsNotEmptyWithWidevineEncryption);
bool dummy_content_id_validator =
    google::RegisterFlagValidator(&FLAGS_content_id,
                                  &IsNotEmptyWithWidevineEncryption);
bool dummy_track_type_validator =
    google::RegisterFlagValidator(&FLAGS_max_sd_pixels, &IsPositive);
bool dummy_signer_validator =
    google::RegisterFlagValidator(&FLAGS_signer,
                                  &IsNotEmptyWithWidevineEncryption);
bool dummy_aes_iv_validator =
    google::RegisterFlagValidator(&FLAGS_aes_signing_iv,
                                  &VerifyAesRsaKey);
bool dummy_rsa_key_file_validator =
    google::RegisterFlagValidator(&FLAGS_rsa_signing_key_path,
                                  &VerifyAesRsaKey);

}  // anonymous namespace
