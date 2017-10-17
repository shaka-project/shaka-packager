// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for raw key encryption/decryption.

#include "packager/app/raw_key_encryption_flags.h"

#include "packager/app/validate_flag.h"

DEFINE_bool(enable_fixed_key_encryption,
            false,
            "Same as --enable_raw_key_encryption. Will be deprecated.");
DEFINE_bool(enable_fixed_key_decryption,
            false,
            "Same as --enable_raw_key_decryption. Will be deprecated.");
DEFINE_bool(enable_raw_key_encryption,
            false,
            "Enable encryption with raw key (key provided in command line).");
DEFINE_bool(enable_raw_key_decryption,
            false,
            "Enable decryption with raw key (key provided in command line).");
DEFINE_hex_bytes(
    key_id,
    "",
    "Key id in hex string format. Will be deprecated. Use --keys.");
DEFINE_hex_bytes(key,
                 "",
                 "Key in hex string format. Will be deprecated. Use --keys.");
DEFINE_string(keys,
              "",
              "A list of key information in the form of label=<drm "
              "label>:key_id=<32-digit key id in hex>:key=<32-digit key in "
              "hex>,label=...");
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

bool ValidateRawKeyCryptoFlags() {
  bool success = true;

  if (FLAGS_enable_fixed_key_encryption)
    FLAGS_enable_raw_key_encryption = true;
  if (FLAGS_enable_fixed_key_decryption)
    FLAGS_enable_raw_key_decryption = true;
  if (FLAGS_enable_fixed_key_encryption || FLAGS_enable_fixed_key_decryption) {
    PrintWarning(
        "--enable_fixed_key_encryption and --enable_fixed_key_decryption are "
        "going to be deprecated. Please switch to --enable_raw_key_encryption "
        "and --enable_raw_key_decryption as soon as possible.");
  }

  const bool raw_key_crypto =
      FLAGS_enable_raw_key_encryption || FLAGS_enable_raw_key_decryption;
  const char raw_key_crypto_label[] = "--enable_raw_key_encryption/decryption";
  // --key_id and --key are associated with --enable_raw_key_encryption and
  // --enable_raw_key_decryption.
  if (FLAGS_keys.empty()) {
    if (!ValidateFlag("key_id", FLAGS_key_id_bytes, raw_key_crypto, false,
                      raw_key_crypto_label)) {
      success = false;
    }
    if (!ValidateFlag("key", FLAGS_key_bytes, raw_key_crypto, false,
                      raw_key_crypto_label)) {
      success = false;
    }
    if (success && (!FLAGS_key_id_bytes.empty() || !FLAGS_key_bytes.empty())) {
      PrintWarning(
          "--key_id and --key are going to be deprecated. Please switch to "
          "--keys as soon as possible.");
    }
  } else {
    if (!FLAGS_key_id_bytes.empty() || !FLAGS_key_bytes.empty()) {
      PrintError("--key_id or --key cannot be used together with --keys.");
      success = false;
    }
  }
  if (!ValidateFlag("iv", FLAGS_iv_bytes, FLAGS_enable_raw_key_encryption, true,
                    "--enable_raw_key_encryption")) {
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

  // --pssh is associated with --enable_raw_key_encryption.
  if (!ValidateFlag("pssh", FLAGS_pssh_bytes, FLAGS_enable_raw_key_encryption,
                    true, "--enable_raw_key_encryption")) {
    success = false;
  }
  return success;
}

}  // namespace shaka
