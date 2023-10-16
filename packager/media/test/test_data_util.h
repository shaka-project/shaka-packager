// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_TEST_TEST_DATA_UTIL_H_
#define PACKAGER_MEDIA_TEST_TEST_DATA_UTIL_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace shaka {
namespace media {

// Returns a file path for a file in the media/test/data directory.
std::filesystem::path GetTestDataFilePath(const std::string& name);

// Returns a file path for a file in the media/app/test/testdata directory.
std::filesystem::path GetAppTestDataFilePath(const std::string& name);

// Reads a test file from media/test/data directory and returns its content.
std::vector<uint8_t> ReadTestDataFile(const std::string& name);

}  // namespace media

// Get the content of |file_path|. Returns empty string on error.
std::string GetPathContent(std::filesystem::path& file_path);

}  // namespace shaka

#endif  // PACKAGER_MEDIA_TEST_TEST_DATA_UTIL_H_
