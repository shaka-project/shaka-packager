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
DEFINE_bool(vp9_subsample_encryption, true, "Enable VP9 subsample encryption.");
