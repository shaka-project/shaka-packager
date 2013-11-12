// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license tha can be
// found in the LICENSE file.
//
// Implements LocalFile.

#ifndef PACKAGER_FILE_LOCAL_FILE_H_
#define PACKAGER_FILE_LOCAL_FILE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "media/file/file.h"

namespace media {

class LocalFile : public File {
 public:
  LocalFile(const char* name, const char* mode);

  // File implementations.
  virtual bool Close() OVERRIDE;
  virtual int64 Read(void* buffer, uint64 length) OVERRIDE;
  virtual int64 Write(const void* buffer, uint64 length) OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool Flush() OVERRIDE;
  virtual bool Eof() OVERRIDE;

 protected:
  virtual bool Open() OVERRIDE;

 private:
  std::string file_mode_;
  FILE* internal_file_;

  DISALLOW_COPY_AND_ASSIGN(LocalFile);
};

}  // namespace media

#endif  // PACKAGER_FILE_LOCAL_FILE_H_

