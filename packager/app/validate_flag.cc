// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Flag validation help functions.

#include "packager/app/validate_flag.h"

#include <stdio.h>

#include "packager/base/strings/stringprintf.h"

namespace shaka {

bool ValidateFlag(const char* flag_name,
                  const std::string& flag_value,
                  bool condition,
                  bool optional,
                  const char* label) {
  if (flag_value.empty()) {
    if (!optional && condition) {
      PrintError(
          base::StringPrintf("--%s is required if %s.", flag_name, label));
      return false;
    }
  } else if (!condition) {
    PrintError(base::StringPrintf(
        "--%s should be specified only if %s.", flag_name, label));
    return false;
  }
  return true;
}

void PrintError(const std::string& error_message) {
  fprintf(stderr, "ERROR: %s\n", error_message.c_str());
}

}  // namespace shaka
