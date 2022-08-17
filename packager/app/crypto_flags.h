// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines common command line flags for encryption and decryption, which
// applies to all key sources, i.e. raw key, Widevine and PlayReady.

#ifndef PACKAGER_APP_CRYPTO_FLAGS_H_
#define PACKAGER_APP_CRYPTO_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_string(protection_scheme);
DECLARE_int32(crypt_byte_block);
DECLARE_int32(skip_byte_block);
DECLARE_bool(vp9_subsample_encryption);
DECLARE_string(playready_extra_header_data);

#endif  // PACKAGER_APP_CRYPTO_FLAGS_H_
