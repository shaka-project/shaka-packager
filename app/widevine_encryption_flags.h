// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for widevine_encryption.

#ifndef APP_WIDEVINE_ENCRYPTION_FLAGS_H_
#define APP_WIDEVINE_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

#include "base/strings/string_number_conversions.h"

DEFINE_bool(enable_widevine_encryption,
            false,
            "Enable encryption with Widevine license server/proxy. User should "
            "provide either AES signing key (--aes_signing_key, "
            "--aes_signing_iv) or RSA signing key (--rsa_signing_key_path).");
DEFINE_string(server_url, "", "License server url.");
DEFINE_string(content_id, "", "Content Id.");
DEFINE_string(track_type, "SD", "Track type: HD, SD or AUDIO.");
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

static bool IsNotEmptyWithWidevineEncryption(const char* flag_name,
                                             const std::string& flag_value) {
  return FLAGS_enable_widevine_encryption ? !flag_value.empty() : true;
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

static bool dummy_server_url_validator =
    google::RegisterFlagValidator(&FLAGS_server_url,
                                  &IsNotEmptyWithWidevineEncryption);
static bool dummy_content_id_validator =
    google::RegisterFlagValidator(&FLAGS_content_id,
                                  &IsNotEmptyWithWidevineEncryption);
static bool dummy_track_type_validator =
    google::RegisterFlagValidator(&FLAGS_track_type,
                                  &IsNotEmptyWithWidevineEncryption);
static bool dummy_signer_validator =
    google::RegisterFlagValidator(&FLAGS_signer,
                                  &IsNotEmptyWithWidevineEncryption);
static bool dummy_aes_iv_validator =
    google::RegisterFlagValidator(&FLAGS_aes_signing_iv,
                                  &VerifyAesRsaKey);
static bool dummy_rsa_key_file_validator =
    google::RegisterFlagValidator(&FLAGS_rsa_signing_key_path,
                                  &VerifyAesRsaKey);
#endif  // APP_WIDEVINE_ENCRYPTION_FLAGS_H_
