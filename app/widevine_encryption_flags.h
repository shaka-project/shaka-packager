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

DEFINE_bool(enable_widevine_encryption,
            false,
            "Enable encryption with Widevine license server/proxy.");
DEFINE_string(server_url, "", "License server url.");
DEFINE_string(content_id, "", "Content Id.");
DEFINE_string(track_type, "SD", "Track type: HD, SD or AUDIO.");
DEFINE_string(signer, "", "The name of the signer.");
DEFINE_string(signing_key_path,
              "",
              "Stores PKCS#1 RSA private key for request signing.");

static bool IsNotEmptyWithWidevineEncryption(const char* flag_name,
                                             const std::string& flag_value) {
  return FLAGS_enable_widevine_encryption ? !flag_value.empty() : true;
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
static bool dummy_rsa_key_file_validator =
    google::RegisterFlagValidator(&FLAGS_signing_key_path,
                                  &IsNotEmptyWithWidevineEncryption);

#endif  // APP_WIDEVINE_ENCRYPTION_FLAGS_H_
