// Copyright 2014 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FILE_LOCAL_FILE_H_
#define PACKAGER_FILE_LOCAL_FILE_H_

#include <cstdint>
#include <string>

#include <packager/file.h>
#include <packager/macros/classes.h>

namespace shaka {

/// Implement LocalFile which deals with local storage.
class LocalFile : public File {
 public:
  /// @param file_name C string containing the name of the file to be accessed.
  /// @param mode C string containing a file access mode, refer to fopen for
  ///        the available modes.
  LocalFile(const char* file_name, const char* mode);

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

  /// Delete a local file.
  /// @param file_name is the path of the file to be deleted.
  /// @return true if successful, or false otherwise.
  static bool Delete(const char* file_name);

 protected:
  ~LocalFile() override;

  bool Open() override;

 private:
  std::string file_mode_;
  FILE* internal_file_;

  DISALLOW_COPY_AND_ASSIGN(LocalFile);
};

}  // namespace shaka

#endif  // PACKAGER_FILE_LOCAL_FILE_H_
