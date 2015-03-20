// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_THREADED_IO_FILE_H_
#define PACKAGER_FILE_THREADED_IO_FILE_H_

#include "packager/base/memory/scoped_ptr.h"
#include "packager/base/synchronization/lock.h"
#include "packager/media/file/file.h"
#include "packager/media/file/file_closer.h"
#include "packager/media/file/io_cache.h"

namespace edash_packager {
namespace media {

class ClosureThread;

/// Declaration of class which implements a thread-safe circular buffer.
class ThreadedIoFile : public File {
 public:
  enum Mode {
    kInputMode,
    kOutputMode
  };

  ThreadedIoFile(scoped_ptr<File, FileCloser> internal_file,
                 Mode mode,
                 uint64_t io_cache_size,
                 uint64_t io_block_size);

  /// @name File implementation overrides.
  /// @{
  virtual bool Close() OVERRIDE;
  virtual int64_t Read(void* buffer, uint64_t length) OVERRIDE;
  virtual int64_t Write(const void* buffer, uint64_t length) OVERRIDE;
  virtual int64_t Size() OVERRIDE;
  virtual bool Flush() OVERRIDE;
  /// @}

 protected:
  virtual ~ThreadedIoFile();

  virtual bool Open() OVERRIDE;

  void RunInInputMode();
  void RunInOutputMode();

 private:
  scoped_ptr<File, FileCloser> internal_file_;
  const Mode mode_;
  IoCache cache_;
  std::vector<uint8_t> io_buffer_;
  scoped_ptr<ClosureThread> thread_;
  uint64_t size_;
  bool eof_;
  int64_t internal_file_error_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedIoFile);
};

}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_FILE_THREADED_IO_FILE_H
