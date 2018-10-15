// Copyright 2018 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/http_file.h"

#include "packager/base/files/file_util.h"
#include "packager/base/logging.h"

namespace shaka {

    HttpFile::HttpFile(const char* file_name, const char* mode)
            : File(file_name), file_mode_(mode) {}

    HttpFile::~HttpFile() {}

    bool HttpFile::Open() {
      base::FilePath file_path(base::FilePath::FromUTF8Unsafe(file_name()));
      LOG(INFO) << "Opening " << file_path.AsUTF8Unsafe();
      return true;
    }

    bool HttpFile::Close() {
      base::FilePath file_path(base::FilePath::FromUTF8Unsafe(file_name()));
      LOG(INFO) << "Closing " << file_path.AsUTF8Unsafe();
      delete this;
      return true;
    }

    int64_t HttpFile::Read(void* buffer, uint64_t length) {
      LOG(ERROR) << "HttpFile does not support Read().";
      return -1;
    }

    int64_t HttpFile::Write(const void* buffer, uint64_t length) {

      base::FilePath file_path(base::FilePath::FromUTF8Unsafe(file_name()));
      LOG(INFO) << "Writing to " << file_path.AsUTF8Unsafe() << ", length=" << length;

      return 42;
    }

    int64_t HttpFile::Size() {
      LOG(INFO) << "HttpFile does not support Size().";
      return -1;
    }

    bool HttpFile::Flush() {
      // Do nothing on Flush.
      return true;
    }

    bool HttpFile::Seek(uint64_t position) {
      VLOG(1) << "HttpFile does not support Seek().";
      return false;
    }

    bool HttpFile::Tell(uint64_t* position) {
      VLOG(1) << "HttpFile does not support Tell().";
      return false;
    }

}  // namespace shaka
