// Copyright 2017 Google Inc. All rights reserved.
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
DECLARE_bool(vp9_subsample_encryption);

#endif  // PACKAGER_APP_CRYPTO_FLAGS_H_
