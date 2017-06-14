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
DEFINE_hex_bytes(key_id, "", "Key id in hex string format.");
DEFINE_hex_bytes(key, "", "Key in hex string format.");
DEFINE_hex_bytes(
    iv,
    "",
    "IV in hex string format. If not specified, a random IV will be "
    "generated. This flag should only be used for testing.");
DEFINE_hex_bytes(
    pssh,
    "",
    "One or more PSSH boxes in hex string format.  If not specified, "
    "will generate a v1 common PSSH box as specified in "
    "https://goo.gl/s8RIhr.");

namespace shaka {

bool ValidateFixedCryptoFlags() {
  bool success = true;

  const bool fixed_crypto =
      FLAGS_enable_fixed_key_encryption || FLAGS_enable_fixed_key_decryption;
  const char fixed_crypto_label[] = "--enable_fixed_key_encryption/decryption";
  // --key_id and --key are associated with --enable_fixed_key_encryption and
  // --enable_fixed_key_decryption.
  if (!ValidateFlag("key_id", FLAGS_key_id_bytes, fixed_crypto, false,
                    fixed_crypto_label)) {
    success = false;
  }
  if (!ValidateFlag("key", FLAGS_key_bytes, fixed_crypto, false,
                    fixed_crypto_label)) {
    success = false;
  }
  if (!ValidateFlag("iv", FLAGS_iv_bytes, FLAGS_enable_fixed_key_encryption,
                    true, "--enable_fixed_key_encryption")) {
    success = false;
  }
  if (!FLAGS_iv_bytes.empty()) {
    if (FLAGS_iv_bytes.size() != 8 && FLAGS_iv_bytes.size() != 16) {
      PrintError(
          "--iv should be either 8 bytes (16 hex digits) or 16 bytes (32 hex "
          "digits).");
      success = false;
    }
  }

  // --pssh is associated with --enable_fix_key_encryption.
  if (!ValidateFlag("pssh", FLAGS_pssh_bytes, FLAGS_enable_fixed_key_encryption,
                    true, "--enable_fixed_key_encryption")) {
    success = false;
  }
  return success;
}

}  // namespace shaka
