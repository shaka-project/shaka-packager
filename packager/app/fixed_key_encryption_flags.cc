// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for fixed key encryption/decryption.

#include "packager/app/fixed_key_encryption_flags.h"

#include "packager/app/validate_flag.h"

DEFINE_bool(enable_fixed_key_encryption,
            false,
            "Enable encryption with fixed key.");
DEFINE_bool(enable_fixed_key_decryption,
            false,
            "Enable decryption with fixed key.");
DEFINE_string(key_id, "", "Key id in hex string format.");
DEFINE_string(key, "", "Key in hex string format.");
DEFINE_string(pssh, "", "PSSH in hex string format.");

namespace edash_packager {

bool ValidateFixedCryptoFlags() {
  bool success = true;

  const bool fixed_crypto =
      FLAGS_enable_fixed_key_encryption || FLAGS_enable_fixed_key_decryption;
  const char fixed_crypto_label[] = "--enable_fixed_key_encryption/decryption";
  // --key_id and --key are associated with --enable_fixed_key_encryption and
  // --enable_fixed_key_decryption.
  if (!ValidateFlag(
          "key_id", FLAGS_key_id, fixed_crypto, false, fixed_crypto_label)) {
    success = false;
  }
  if (!ValidateFlag(
          "key", FLAGS_key, fixed_crypto, false, fixed_crypto_label)) {
    success = false;
  }

  // --pssh is associated with --enable_fix_key_encryption.
  if (!ValidateFlag("pssh",
                    FLAGS_pssh,
                    FLAGS_enable_fixed_key_encryption,
                    false,
                    "--enable_fixed_key_encryption")) {
    success = false;
  }
  return success;
}

}  // namespace edash_packager
