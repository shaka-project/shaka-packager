// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd
//
// Flag validation help functions.

#ifndef APP_VALIDATE_FLAG_H_
#define APP_VALIDATE_FLAG_H_

#include <string>

#include "packager/base/strings/stringprintf.h"

namespace shaka {

/// Format and print error message.
/// @param error_message specifies the error message.
void PrintError(const std::string& error_message);

/// Format and print warning message.
/// @param warning_message specifies the warning message.
void PrintWarning(const std::string& warning_message);

/// Validate a flag against the given condition.
/// @param flag_name is the name of the flag.
/// @param flag_value is the value of the flag.
/// @param condition,optional determines how the flag should be validated. If
///        condition is true and optional is false, then this flag is required
//         and cannot be empty; If condition is false, then this flag should
//         not be set.
/// @param label specifies the label associated with the condition. It is used
///        to generate the error message on validation failure.
/// @return true on success, false otherwise.
template <class FlagType>
bool ValidateFlag(const char* flag_name,
                  const FlagType& flag_value,
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

}  // namespace shaka

#endif  // APP_VALIDATE_FLAG_H_
