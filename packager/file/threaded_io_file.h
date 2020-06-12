// Copyright 2015 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_THREADED_IO_FILE_H_
#define PACKAGER_FILE_THREADED_IO_FILE_H_

#include <atomic>
#include <memory>
#include "packager/base/synchronization/waitable_event.h"
#include "packager/file/file.h"
#include "packager/file/file_closer.h"
#include "packager/file/io_cache.h"

namespace shaka {

/// Declaration of class which implements a thread-safe circular buffer.
class ThreadedIoFile : public File {
 public:
  enum Mode { kInputMode, kOutputMode };

  ThreadedIoFile(std::unique_ptr<File, FileCloser> internal_file,
                 Mode mode,
                 uint64_t io_cache_size,
                 uint64_t io_block_size);

  /// @name File implementation overrides.
  /// @{
  bool Close() override;
  int64_t Read(void* buffer, uint64_t length) override;
  int64_t Write(const void* buffer, uint64_t length) override;
  int64_t Size() override;
  bool Flush() override;
  bool Seek(uint64_t position) override;
  bool Tell(uint64_t* position) override;
  /// @}

 protected:
  ~ThreadedIoFile() override;

  bool Open() override;

 private:
  // Internal task handler implementation. Will dispatch to either
  // |RunInInputMode| or |RunInOutputMode| depending on |mode_|.
  void TaskHandler();
  void RunInInputMode();
  void RunInOutputMode();

  std::unique_ptr<File, FileCloser> internal_file_;
  const Mode mode_;
  IoCache cache_;
  std::vector<uint8_t> io_buffer_;
  uint64_t position_;
  uint64_t size_;
  std::atomic<bool> eof_;
  bool flushing_;
  base::WaitableEvent flush_complete_event_;
  std::atomic<int32_t> internal_file_error_;
  // Signalled when thread task exits.
  base::WaitableEvent task_exit_event_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedIoFile);
};

}  // namespace shaka

#endif  // PACKAGER_FILE_THREADED_IO_FILE_H
