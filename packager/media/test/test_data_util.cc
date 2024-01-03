// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <packager/media/test/test_data_util.h>

#include <absl/log/log.h>

namespace shaka {
namespace media {

// Returns a file path for a file in the media/test/data directory.
std::filesystem::path GetTestDataFilePath(const std::string& name) {
  auto data_dir = std::filesystem::u8path(TEST_DATA_DIR);
  return data_dir / name;
}

// Returns a file path for a file in the media/app/test/testdata directory.
std::filesystem::path GetAppTestDataFilePath(const std::string& name) {
  auto data_dir = std::filesystem::u8path(TEST_DATA_DIR);
  auto app_data_dir = data_dir.parent_path().parent_path().parent_path() /
                      "app" / "test" / "testdata";
  return app_data_dir / name;
}

// Reads a test file from media/test/data directory and returns its content.
std::vector<uint8_t> ReadTestDataFile(const std::string& name) {
  auto path = GetTestDataFilePath(name);

  FILE* f = fopen(path.string().c_str(), "rb");
  if (!f) {
    LOG(ERROR) << "Failed to read test data from " << path;
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> data;
  data.resize(std::filesystem::file_size(path));
  size_t size = fread(data.data(), 1, data.size(), f);
  data.resize(size);
  fclose(f);

  return data;
}

}  // namespace media

// Get the content of |file_path|. Returns empty string on error.
std::string GetPathContent(std::filesystem::path& file_path) {
  std::string content;

  FILE* f = fopen(file_path.string().c_str(), "rb");
  if (!f) {
    LOG(FATAL) << "Failed to read test data from " << file_path;
    return std::string{};
  }

  content.resize(std::filesystem::file_size(file_path));
  size_t size = fread(content.data(), 1, content.size(), f);
  content.resize(size);
  fclose(f);

  return content;
}

}  // namespace shaka
