// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/file_test_util.h>

#include <filesystem>

namespace shaka {

std::string generate_unique_temp_path() {
  // Generate a unique name for a temporary file, using standard library
  // routines, to avoid a circular dependency on any of our own code for
  // generating temporary files.  The template must end in 6 X's.
  auto temp_path_template =
      std::filesystem::temp_directory_path() / "packager-test.XXXXXX";
  std::string temp_path_template_string = temp_path_template.string();
#if defined(OS_WIN)
  // _mktemp will modify the string passed to it to reflect the generated name
  // (replacing the X characters with something else).
  _mktemp(temp_path_template_string.data());
#else
  // mkstemp will create and open the file, modify the character points to
  // reflect the generated name (replacing the X characters with something
  // else), and return an open file descriptor.  Then we close it and use the
  // generated name.
  int fd = mkstemp(temp_path_template_string.data());
  close(fd);
#endif
  return temp_path_template_string;
}

void delete_file(const std::string& path) {
  std::error_code ec;
  std::filesystem::remove(std::filesystem::u8path(path), ec);
  // Ignore errors.
}

TempFile::TempFile() : path_(generate_unique_temp_path()) {}

TempFile::~TempFile() {
  std::error_code ec;
  std::filesystem::remove(std::filesystem::u8path(path_), ec);
  // Ignore errors.
}

}  // namespace shaka
