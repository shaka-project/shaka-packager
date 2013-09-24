// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "media/base/decoder_buffer.h"

namespace media {

base::FilePath GetTestDataFilePath(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("media"))
      .Append(FILE_PATH_LITERAL("test")).Append(FILE_PATH_LITERAL("data"))
      .AppendASCII(name);
  return file_path;
}

scoped_refptr<DecoderBuffer> ReadTestDataFile(const std::string& name) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));

  file_path = file_path.Append(FILE_PATH_LITERAL("media"))
      .Append(FILE_PATH_LITERAL("test")).Append(FILE_PATH_LITERAL("data"))
      .AppendASCII(name);

  int64 tmp = 0;
  CHECK(file_util::GetFileSize(file_path, &tmp))
      << "Failed to get file size for '" << name << "'";

  int file_size = static_cast<int>(tmp);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(file_size));
  CHECK_EQ(file_size,
           file_util::ReadFile(
               file_path, reinterpret_cast<char*>(buffer->writable_data()),
               file_size)) << "Failed to read '" << name << "'";

  return buffer;
}

}  // namespace media
