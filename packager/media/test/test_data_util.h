// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PACKAGER_MEDIA_TEST_TEST_DATA_UTIL_H_
#define PACKAGER_MEDIA_TEST_TEST_DATA_UTIL_H_

#include <stdint.h>

#include <filesystem>
#include <string>

#include "glog/logging.h"

namespace shaka {
namespace media {

// Returns a file path for a file in the media/test/data directory.
std::filesystem::path GetTestDataFilePath(const std::string& name) {
  std::filesystem::path header_path(__FILE__);
  return header_path.parent_path() / "data" / name;
}

// Returns a file path for a file in the media/app/test/testdata directory.
std::filesystem::path GetAppTestDataFilePath(const std::string& name) {
  std::filesystem::path header_path(__FILE__);
  return header_path.parent_path().parent_path() / "app" / "test" / "testdata"
                                                 / name;
}

// Reads a test file from media/test/data directory and returns its content.
std::vector<uint8_t> ReadTestDataFile(const std::string& name) {
  std::filesystem::path path = GetTestDataFilePath(name);

  std::vector<uint8_t> data;
  data.resize(std::filesystem::file_size(path));

  FILE* f = fopen(path.string().c_str(), "rb");
  if (!f) {
    LOG(FATAL) << "Failed to read test data from " << path;
    return std::vector<uint8_t>();
  }

  size_t size = fread(data.data(), 1, data.size(), f);
  data.resize(size);
  fclose(f);

  return data;
}

}  // namespace media
}  // namespace shaka

#endif  // PACKAGER_MEDIA_TEST_TEST_DATA_UTIL_H_
