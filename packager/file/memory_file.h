// Copyright 2015 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef MEDIA_FILE_MEDIA_FILE_H_
#define MEDIA_FILE_MEDIA_FILE_H_

#include <cstdint>
#include <string>
#include <vector>

#include <packager/file.h>
#include <packager/macros/classes.h>

namespace shaka {

/// Implements a File that is stored in memory.  This should be only used for
/// testing, since this does not support larger files.
class MemoryFile : public File {
 public:
  MemoryFile(const std::string& file_name, const std::string& mode);

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

  /// Deletes all memory file data created.  This assumes that there are no
  /// MemoryFile objects alive.  Any alive objects will be in an undefined
  /// state.
  static void DeleteAll();
  /// Deletes the memory file data with the given file_name.  Any objects open
  /// with that file name will be in an undefined state.
  static void Delete(const std::string& file_name);

 protected:
  ~MemoryFile() override;
  bool Open() override;

 private:
  std::string mode_;
  std::vector<uint8_t>* file_;
  uint64_t position_;

  DISALLOW_COPY_AND_ASSIGN(MemoryFile);
};

}  // namespace shaka

#endif  // MEDIA_FILE_MEDIA_FILE_H_
