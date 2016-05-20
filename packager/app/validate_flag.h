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

namespace shaka {

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
bool ValidateFlag(const char* flag_name,
                  const std::string& flag_value,
                  bool condition,
                  bool optional,
                  const char* label);

/// Format and print error message.
/// @param error_message specifies the error message.
void PrintError(const std::string& error_message);

}  // namespace shaka

#endif  // APP_VALIDATE_FLAG_H_
