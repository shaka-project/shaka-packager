// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for fixed key encryption.

#include "packager/app/fixed_key_encryption_flags.h"

DEFINE_bool(enable_fixed_key_encryption,
            false,
            "Enable encryption with fixed key.");
DEFINE_bool(enable_fixed_key_decryption,
            false,
            "Enable decryption with fixed key.");
DEFINE_string(key_id, "", "Key id in hex string format.");
DEFINE_string(key, "", "Key in hex string format.");
DEFINE_string(pssh, "", "PSSH in hex string format.");

static bool IsNotEmptyWithFixedKeyEncryption(const char* flag_name,
                                             const std::string& flag_value) {
  if (FLAGS_enable_fixed_key_encryption && flag_value.empty())
    return false;
  std::string flag_name_str(flag_name);
  if (FLAGS_enable_fixed_key_decryption && (flag_name_str != "pssh") &&
      flag_value.empty())
    return false;
  return true;
}

static bool dummy_key_id_validator =
    google::RegisterFlagValidator(&FLAGS_key_id,
                                  &IsNotEmptyWithFixedKeyEncryption);
static bool dummy_key_validator =
    google::RegisterFlagValidator(&FLAGS_key,
                                  &IsNotEmptyWithFixedKeyEncryption);
static bool dummy_pssh_validator =
    google::RegisterFlagValidator(&FLAGS_pssh,
                                  &IsNotEmptyWithFixedKeyEncryption);
