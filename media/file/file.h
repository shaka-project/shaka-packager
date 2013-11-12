// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license tha can be
// found in the LICENSE file.
//
// Defines an abstract file interface.

#ifndef PACKAGER_FILE_FILE_H_
#define PACKAGER_FILE_FILE_H_

#include <string>

#include "base/basictypes.h"

namespace media {

extern const char* kLocalFilePrefix;

class File {
 public:
  // Open the specified file, or return NULL on error.
  // This is actually a file factory method, it opens a proper file, e.g.
  // LocalFile, MemFile automatically based on prefix.
  static File* Open(const char* name, const char* mode);

  // Flush() and de-allocate resources associated with this file, and
  // delete this File object.  THIS IS THE ONE TRUE WAY TO DEALLOCATE
  // THIS OBJECT.
  // Returns true on success.
  // For writable files, returning false MAY INDICATE DATA LOSS.
  virtual bool Close() = 0;

  // Reads data and returns it in buffer. Returns a value < 0 on error,
  // or the number of bytes read otherwise. Returns zero on end-of-file,
  // or if 'length' is zero.
  virtual int64 Read(void* buffer, uint64 length) = 0;

  // Write 'length' bytes from 'buffer', returning the number of bytes
  // that were actually written.  Return < 0 on error.
  //
  // For a file open in append mode (i.e., "a" or "a+"), Write()
  // always appends to the end of the file. For files opened in other
  // write modes (i.e., "w", or "w+"), writes occur at the
  // current file offset.
  virtual int64 Write(const void* buffer, uint64 length) = 0;

  // Return the size of the file in bytes.
  // A return value less than zero indicates a problem getting the
  // size.
  virtual int64 Size() = 0;

  // Flushes the file so that recently written data will survive an
  // application crash (but not necessarily an OS crash).  For
  // instance, in localfile the data is flushed into the OS but not
  // necessarily to disk.
  virtual bool Flush() = 0;

  // Return whether we're currently at eof.
  virtual bool Eof() = 0;

  // Return the file name.
  const std::string& file_name() const { return file_name_; }

  // ************************************************************
  // * Static Methods: File-on-the-filesystem status
  // ************************************************************

  // Returns the size of a file in bytes, and opens and closes the file
  // in the process. Returns -1 on failure.
  static int64 GetFileSize(const char* fname);

 protected:
  explicit File(const std::string& file_name) : file_name_(file_name) {}
  // Do *not* call the destructor directly (with the "delete" keyword)
  // nor use scoped_ptr; instead use Close().
  virtual ~File() {}

  // Internal open. Should not be used directly.
  virtual bool Open() = 0;

 private:
  // This is a file factory method, it creates a proper file, e.g.
  // LocalFile, MemFile based on prefix.
  static File* Create(const char* fname, const char* mode);

  std::string file_name_;
  DISALLOW_COPY_AND_ASSIGN(File);
};

}  // namespace media

#endif  // PACKAGER_FILE_FILE_H_
