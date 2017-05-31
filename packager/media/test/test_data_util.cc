// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "packager/media/test/test_data_util.h"

#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"
#include "packager/base/path_service.h"

namespace shaka {
namespace media {

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("packager"))
                  .Append(FILE_PATH_LITERAL("media"))
                  .Append(FILE_PATH_LITERAL("test"))
                  .Append(FILE_PATH_LITERAL("data"))
                  .AppendASCII(name);
  return file_path;
}

base::FilePath GetAppTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("packager"))
                  .Append(FILE_PATH_LITERAL("app"))
                  .Append(FILE_PATH_LITERAL("test"))
                  .Append(FILE_PATH_LITERAL("testdata"))
                  .AppendASCII(name);
  return file_path;
}

std::vector<uint8_t> ReadTestDataFile(const std::string& name) {
  std::string buffer;
  CHECK(base::ReadFileToString(GetTestDataFilePath(name), &buffer));
  return std::vector<uint8_t>(buffer.begin(), buffer.end());
}

}  // namespace media
}  // namespace shaka
