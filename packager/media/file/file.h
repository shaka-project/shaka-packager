// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_FILE_H_
#define PACKAGER_FILE_FILE_H_

#include <stdint.h>

#include <string>

#include "packager/base/macros.h"

namespace edash_packager {
namespace media {

extern const char* kLocalFilePrefix;

/// Define an abstract file interface.
class File {
 public:
  /// Open the specified file.
  /// This is a file factory method, it opens a proper file automatically
  /// based on prefix, e.g. "file://" for LocalFile.
  /// @param file_name contains the name of the file to be accessed.
  /// @param mode contains file access mode. Implementation dependent.
  /// @return A File pointer on success, false otherwise.
  static File* Open(const char* file_name, const char* mode);

  /// Open the specified file in direct-access mode (no buffering).
  /// This is a file factory method, it opens a proper file automatically
  /// based on prefix, e.g. "file://" for LocalFile.
  /// @param file_name contains the name of the file to be accessed.
  /// @param mode contains file access mode. Implementation dependent.
  /// @return A File pointer on success, false otherwise.
  static File* OpenWithNoBuffering(const char* file_name, const char* mode);

  /// Delete the specified file.
  /// @param file_name contains the path of the file to be deleted.
  /// @return true if successful, false otherwise.
  static bool Delete(const char* file_name);

  /// Flush() and de-allocate resources associated with this file, and
  /// delete this File object.  THIS IS THE ONE TRUE WAY TO DEALLOCATE
  /// THIS OBJECT.
  /// @return true on success. For writable files, returning false MAY
  ///         INDICATE DATA LOSS.
  virtual bool Close() = 0;

  /// Read data and return it in buffer.
  /// @param[out] buffer points to a block of memory with a size of at least
  ///             @a length bytes.
  /// @param length indicates number of bytes to be read.
  /// @return Number of bytes read, or a value < 0 on error.
  ///         Zero on end-of-file, or if 'length' is zero.
  virtual int64_t Read(void* buffer, uint64_t length) = 0;

  /// Write block of data.
  /// @param buffer points to a block of memory with at least @a length bytes.
  /// @param length indicates number of bytes to write.
  /// @return Number of bytes written, or a value < 0 on error.
  virtual int64_t Write(const void* buffer, uint64_t length) = 0;

  /// @return Size of the file in bytes. A return value less than zero
  ///         indicates a problem getting the size.
  virtual int64_t Size() = 0;

  /// Flush the file so that recently written data will survive an
  /// application crash (but not necessarily an OS crash). For
  /// instance, in LocalFile the data is flushed into the OS but not
  /// necessarily to disk.
  /// @return true on success, false otherwise.
  virtual bool Flush() = 0;

  /// Seek to the specifield position in the file.
  /// @param position is the position to seek to.
  /// @return true on success, false otherwise.
  virtual bool Seek(uint64_t position) = 0;

  /// Get the current file position.
  /// @param position is a pointer to contain the current file position upon
  ///        successful return.
  /// @return true on succcess, false otherwise.
  virtual bool Tell(uint64_t* position) = 0;

  /// @return The file name.
  const std::string& file_name() const { return file_name_; }

  // ************************************************************
  // * Static Methods: File-on-the-filesystem status
  // ************************************************************

  /// @return The size of a file in bytes on success, a value < 0 otherwise.
  ///         The file will be opened and closed in the process.
  static int64_t GetFileSize(const char* file_name);

  /// Read the file into string.
  /// @param file_name is the file to be read. It should be a valid file.
  /// @param contents[out] points to the output string. Should not be NULL.
  /// @return true on success, false otherwise.
  static bool ReadFileToString(const char* file_name, std::string* contents);

 protected:
  explicit File(const std::string& file_name) : file_name_(file_name) {}
  /// Do *not* call the destructor directly (with the "delete" keyword)
  /// nor use scoped_ptr; instead use Close().
  virtual ~File() {}

  /// Internal open. Should not be used directly.
  virtual bool Open() = 0;

 private:
  friend class ThreadedIoFile;

  // This is a file factory method, it creates a proper file, e.g.
  // LocalFile, MemFile based on prefix.
  static File* Create(const char* file_name, const char* mode);

  static File* CreateInternalFile(const char* file_name, const char* mode);

  std::string file_name_;
  DISALLOW_COPY_AND_ASSIGN(File);
};

}  // namespace media
}  // namespace edash_packager

#endif  // PACKAGER_FILE_FILE_H_
