// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TEST_TEST_DATA_UTIL_H_
#define MEDIA_TEST_TEST_DATA_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"

namespace edash_packager {
namespace media {

// Returns a file path for a file in the media/test/data directory.
base::FilePath GetTestDataFilePath(const std::string& name);

// Reads a test file from media/test/data directory and returns its content.
std::vector<uint8_t> ReadTestDataFile(const std::string& name);

}  // namespace media
}  // namespace edash_packager

#endif  // MEDIA_TEST_TEST_DATA_UTIL_H_
