// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#include "packager/app/widevine_encryption_flags.h"

#include "packager/base/logging.h"
#include "packager/base/strings/string_number_conversions.h"

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

namespace {

static bool VerifyEncryptionAndDecryptionParams(const char* flag_name,
                                                const std::string& flag_value) {
  DCHECK(flag_name);

  const std::string flag_name_str = flag_name;
  bool is_common_param = (flag_name_str == "key_server_url") ||
                         (flag_name_str == "signer");
  if (FLAGS_enable_widevine_encryption) {
    if (flag_value.empty()) {
      fprintf(stderr,
              "ERROR: %s required if enable_widevine_encryption is true\n",
             flag_name);
      return false;
    }
  } else if (FLAGS_enable_widevine_decryption) {
    if (is_common_param) {
      if (flag_value.empty()) {
        fprintf(stderr,
                "ERROR: %s required if --enable_widevine_encryption or "
                "--enable_widevine_decryption is true\n",
                flag_name);
        return false;
      }
    } else {
      if (!flag_value.empty()) {
        fprintf(stderr, "ERROR: %s should only be specified if "
               "--enable_widevine_decryption is true\n", flag_name);
        return false;
      }
    }
  } else {
    if (!flag_value.empty()) {
      fprintf(stderr, "ERROR: %s should only be specified if %s"
              " is true\n", flag_name, is_common_param ?
             "--enable_widevine_encryption or --enable_widevine_decryption" :
             "--enable_widevine_encryption");
      return false;
    }
  }
  return true;
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
                                  &VerifyEncryptionAndDecryptionParams);
bool dummy_content_id_validator =
    google::RegisterFlagValidator(&FLAGS_content_id,
                                  &VerifyEncryptionAndDecryptionParams);
bool dummy_track_type_validator =
    google::RegisterFlagValidator(&FLAGS_max_sd_pixels, &IsPositive);
bool dummy_signer_validator =
    google::RegisterFlagValidator(&FLAGS_signer,
                                  &VerifyEncryptionAndDecryptionParams);
bool dummy_aes_iv_validator =
    google::RegisterFlagValidator(&FLAGS_aes_signing_iv,
                                  &VerifyAesRsaKey);
bool dummy_rsa_key_file_validator =
    google::RegisterFlagValidator(&FLAGS_rsa_signing_key_path,
                                  &VerifyAesRsaKey);

}  // anonymous namespace
