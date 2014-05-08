// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Defines command line flags for fixed key encryption.

#ifndef APP_FIXED_KEY_ENCRYPTION_FLAGS_H_
#define APP_FIXED_KEY_ENCRYPTION_FLAGS_H_

#include <gflags/gflags.h>

DECLARE_bool(enable_fixed_key_encryption);
DECLARE_string(key_id);
DECLARE_string(key);
DECLARE_string(pssh);

#endif  // APP_FIXED_KEY_ENCRYPTION_FLAGS_H_
