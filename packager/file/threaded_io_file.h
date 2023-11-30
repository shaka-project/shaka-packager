// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_THREADED_IO_FILE_H_
#define PACKAGER_FILE_THREADED_IO_FILE_H_

#include <atomic>
#include <memory>

#include <absl/synchronization/mutex.h>

#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/file/io_cache.h>
#include <packager/macros/classes.h>

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
  void CloseForWriting() override;
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
  void WaitForSignal(absl::Mutex* mutex, bool* condition);

  std::unique_ptr<File, FileCloser> internal_file_;
  const Mode mode_;
  IoCache cache_;
  std::vector<uint8_t> io_buffer_;
  uint64_t position_;
  uint64_t size_;
  std::atomic<bool> eof_;
  std::atomic<int64_t> internal_file_error_;

  absl::Mutex flush_mutex_;
  bool flushing_ ABSL_GUARDED_BY(flush_mutex_);
  bool flush_complete_ ABSL_GUARDED_BY(flush_mutex_);

  absl::Mutex task_exited_mutex_;
  bool task_exited_ ABSL_GUARDED_BY(task_exited_mutex_);

  DISALLOW_COPY_AND_ASSIGN(ThreadedIoFile);
};

}  // namespace shaka

#endif  // PACKAGER_FILE_THREADED_IO_FILE_H
