// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <string>

namespace shaka {

/// Create a temp file name in directory @a temp_dir. Generate the temp file
/// in os specific temporary directory if @a temp_dir is empty.
/// @param temp_dir specifies the directory where the file should go.
/// @param temp_file_path is the result temp file path on success.
/// @returns true on success, false otherwise.
bool TempFilePath(const std::string& temp_dir, std::string* temp_file_path);

}  // namespace shaka
