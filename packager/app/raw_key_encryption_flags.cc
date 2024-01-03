// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for raw key encryption/decryption.

#include <packager/app/validate_flag.h>
#include <packager/utils/absl_flag_hexbytes.h>

ABSL_FLAG(bool,
          enable_fixed_key_encryption,
          false,
          "Same as --enable_raw_key_encryption. Will be deprecated.");
ABSL_FLAG(bool,
          enable_fixed_key_decryption,
          false,
          "Same as --enable_raw_key_decryption. Will be deprecated.");
ABSL_FLAG(bool,
          enable_raw_key_encryption,
          false,
          "Enable encryption with raw key (key provided in command line).");
ABSL_FLAG(bool,
          enable_raw_key_decryption,
          false,
          "Enable decryption with raw key (key provided in command line).");
ABSL_FLAG(shaka::HexBytes,
          key_id,
          {},
          "Key id in hex string format. Will be deprecated. Use --keys.");
ABSL_FLAG(shaka::HexBytes,
          key,
          {},
          "Key in hex string format. Will be deprecated. Use --keys.");
ABSL_FLAG(std::string,
          keys,
          "",
          "A list of key information in the form of label=<drm "
          "label>:key_id=<32-digit key id in hex>:key=<32-digit key in "
          "hex>,label=...");
ABSL_FLAG(shaka::HexBytes,
          iv,
          {},
          "IV in hex string format. If not specified, a random IV will be "
          "generated. This flag should only be used for testing.");
ABSL_FLAG(shaka::HexBytes,
          pssh,
          {},
          "One or more PSSH boxes in hex string format.  If not specified, "
          "will generate a v1 common PSSH box as specified in "
          "https://goo.gl/s8RIhr.");

namespace shaka {

bool ValidateRawKeyCryptoFlags() {
  bool success = true;

  if (absl::GetFlag(FLAGS_enable_fixed_key_encryption))
    absl::SetFlag(&FLAGS_enable_raw_key_encryption, true);
  if (absl::GetFlag(FLAGS_enable_fixed_key_decryption))
    absl::SetFlag(&FLAGS_enable_raw_key_decryption, true);
  if (absl::GetFlag(FLAGS_enable_fixed_key_encryption) ||
      absl::GetFlag(FLAGS_enable_fixed_key_decryption)) {
    PrintWarning(
        "--enable_fixed_key_encryption and --enable_fixed_key_decryption are "
        "going to be deprecated. Please switch to --enable_raw_key_encryption "
        "and --enable_raw_key_decryption as soon as possible.");
  }

  const bool raw_key_crypto = absl::GetFlag(FLAGS_enable_raw_key_encryption) ||
                              absl::GetFlag(FLAGS_enable_raw_key_decryption);
  const char raw_key_crypto_label[] = "--enable_raw_key_encryption/decryption";
  // --key_id and --key are associated with --enable_raw_key_encryption and
  // --enable_raw_key_decryption.
  if (absl::GetFlag(FLAGS_keys).empty()) {
    if (!ValidateFlag("key_id", absl::GetFlag(FLAGS_key_id).bytes,
                      raw_key_crypto, false, raw_key_crypto_label)) {
      success = false;
    }
    if (!ValidateFlag("key", absl::GetFlag(FLAGS_key).bytes, raw_key_crypto,
                      false, raw_key_crypto_label)) {
      success = false;
    }
    if (success && (!absl::GetFlag(FLAGS_key_id).bytes.empty() ||
                    !absl::GetFlag(FLAGS_key).bytes.empty())) {
      PrintWarning(
          "--key_id and --key are going to be deprecated. Please switch to "
          "--keys as soon as possible.");
    }
  } else {
    if (!absl::GetFlag(FLAGS_key_id).bytes.empty() ||
        !absl::GetFlag(FLAGS_key).bytes.empty()) {
      PrintError("--key_id or --key cannot be used together with --keys.");
      success = false;
    }
  }
  if (!ValidateFlag("iv", absl::GetFlag(FLAGS_iv).bytes,
                    absl::GetFlag(FLAGS_enable_raw_key_encryption), true,
                    "--enable_raw_key_encryption")) {
    success = false;
  }
  if (!absl::GetFlag(FLAGS_iv).bytes.empty()) {
    if (absl::GetFlag(FLAGS_iv).bytes.size() != 8 &&
        absl::GetFlag(FLAGS_iv).bytes.size() != 16) {
      PrintError(
          "--iv should be either 8 bytes (16 hex digits) or 16 bytes (32 hex "
          "digits).");
      success = false;
    }
  }

  // --pssh is associated with --enable_raw_key_encryption.
  if (!ValidateFlag("pssh", absl::GetFlag(FLAGS_pssh).bytes,
                    absl::GetFlag(FLAGS_enable_raw_key_encryption), true,
                    "--enable_raw_key_encryption")) {
    success = false;
  }
  return success;
}

}  // namespace shaka
