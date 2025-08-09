// Copyright 2017 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <cstdint>

#include <packager/file.h>

namespace shaka {

/// Implements CallbackFile, which delegates read/write calls to the callback
/// functions set through the file name.
class CallbackFile : public File {
 public:
  /// @param file_name is the callback file name, which should have callback
  ///        address encoded. Note that the file type prefix should be stripped
  ///        off already.
  /// @param mode C string containing a file access mode, refer to fopen for
  ///        the available modes.
  CallbackFile(const char* file_name, const char* mode);

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
  ~CallbackFile() override;

  bool Open() override;

 private:
  CallbackFile(const CallbackFile&) = delete;
  CallbackFile& operator=(const CallbackFile&) = delete;

  const BufferCallbackParams* callback_params_ = nullptr;
  std::string name_;
  std::string file_mode_;
};

}  // namespace shaka
