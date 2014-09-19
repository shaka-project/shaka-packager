// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/test/test_data_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"

namespace edash_packager {
namespace media {

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("media"))
                  .Append(FILE_PATH_LITERAL("test"))
                  .Append(FILE_PATH_LITERAL("data"))
                  .AppendASCII(name);
  return file_path;
}

std::vector<uint8> ReadTestDataFile(const std::string& name) {
  std::string buffer;
  CHECK(base::ReadFileToString(GetTestDataFilePath(name), &buffer));
  return std::vector<uint8>(buffer.begin(), buffer.end());
}

}  // namespace media
}  // namespace edash_packager
