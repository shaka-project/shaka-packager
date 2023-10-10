// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/app/crypto_flags.h>

#include <cstdio>

#include <absl/flags/flag.h>

ABSL_FLAG(std::string,
          protection_scheme,
          "cenc",
          "Specify a protection scheme, 'cenc' or 'cbc1' or pattern-based "
          "protection schemes 'cens' or 'cbcs'.");
ABSL_FLAG(
    int32_t,
    crypt_byte_block,
    1,
    "Specify the count of the encrypted blocks in the protection pattern, "
    "where block is of size 16-bytes. There are three common "
    "patterns (crypt_byte_block:skip_byte_block): 1:9 (default), 5:5, 10:0. "
    "Apply to video streams with 'cbcs' and 'cens' protection schemes only; "
    "ignored otherwise.");
ABSL_FLAG(
    int32_t,
    skip_byte_block,
    9,
    "Specify the count of the unencrypted blocks in the protection pattern. "
    "Apply to video streams with 'cbcs' and 'cens' protection schemes only; "
    "ignored otherwise.");
ABSL_FLAG(bool,
          vp9_subsample_encryption,
          true,
          "Enable VP9 subsample encryption.");
ABSL_FLAG(std::string,
          playready_extra_header_data,
          "",
          "Extra XML data to add to PlayReady headers.");

bool ValueNotGreaterThanTen(const char* flagname, int32_t value) {
  if (value > 10) {
    fprintf(stderr, "ERROR: %s must not be greater than 10.\n", flagname);
    return false;
  }
  if (value < 0) {
    fprintf(stderr, "ERROR: %s must be non-negative.\n", flagname);
    return false;
  }
  return true;
}

bool ValueIsXml(const char* flagname, const std::string& value) {
  if (value.empty())
    return true;

  if (value[0] != '<' || value[value.size() - 1] != '>') {
    fprintf(stderr, "ERROR: %s must be valid XML.\n", flagname);
    return false;
  }
  return true;
}

namespace shaka {
bool ValidateCryptoFlags() {
  bool success = true;

  auto crypt_byte_block = absl::GetFlag(FLAGS_crypt_byte_block);
  if (!ValueNotGreaterThanTen("crypt_byte_block", crypt_byte_block)) {
    success = false;
  }

  auto skip_byte_block = absl::GetFlag(FLAGS_skip_byte_block);
  if (!ValueNotGreaterThanTen("skip_byte_block", skip_byte_block)) {
    success = false;
  }

  auto playready_extra_header_data =
      absl::GetFlag(FLAGS_playready_extra_header_data);
  if (!ValueIsXml("playready_extra_header_data", playready_extra_header_data)) {
    success = false;
  }

  return success;
}
}  // namespace shaka
