// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Flag validation help functions.

#include "packager/app/validate_flag.h"

#include <stdio.h>

namespace shaka {

void PrintError(const std::string& error_message) {
  fprintf(stderr, "ERROR: %s\n", error_message.c_str());
}

void PrintWarning(const std::string& warning_message) {
  fprintf(stderr, "WARNING: %s\n", warning_message.c_str());
}

}  // namespace shaka
