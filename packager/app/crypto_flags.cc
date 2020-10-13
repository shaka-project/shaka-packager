// Copyright 2017 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/app/crypto_flags.h"

#include <stdio.h>

DEFINE_string(protection_scheme,
              "cenc",
              "Specify a protection scheme, 'cenc' or 'cbc1' or pattern-based "
              "protection schemes 'cens' or 'cbcs'.");
DEFINE_int32(
    crypt_byte_block,
    1,
    "Specify the count of the encrypted blocks in the protection pattern, "
    "where block is of size 16-bytes. There are three common "
    "patterns (crypt_byte_block:skip_byte_block): 1:9 (default), 5:5, 10:0. "
    "Apply to video streams with 'cbcs' and 'cens' protection schemes only; "
    "ignored otherwise.");
DEFINE_int32(
    skip_byte_block,
    9,
    "Specify the count of the unencrypted blocks in the protection pattern. "
    "Apply to video streams with 'cbcs' and 'cens' protection schemes only; "
    "ignored otherwise.");
DEFINE_bool(vp9_subsample_encryption, true, "Enable VP9 subsample encryption.");
DEFINE_string(playready_extra_header_data,
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

DEFINE_validator(crypt_byte_block, &ValueNotGreaterThanTen);
DEFINE_validator(skip_byte_block, &ValueNotGreaterThanTen);
DEFINE_validator(playready_extra_header_data, &ValueIsXml);
