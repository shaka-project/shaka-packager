// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/file_util.h"

#include <inttypes.h>

#if defined(OS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <thread>

#include "absl/strings/str_format.h"

namespace shaka {
namespace {
// Create a temp file name using process id, thread id and current time.
std::string TempFileName() {
#if defined(OS_WIN)
  const uint32_t process_id = static_cast<uint32_t>(GetCurrentProcessId());
#else
  const uint32_t process_id = static_cast<uint32_t>(getpid());
#endif
  const size_t thread_id =
      std::hash<std::thread::id>{}(std::this_thread::get_id());

  // We may need two or more temporary files in the same thread. There might be
  // name collision if they are requested around the same time, e.g. called
  // consecutively. Use a thread_local instance to avoid that.
  static thread_local uint32_t instance_id = 0;
  ++instance_id;

  return absl::StrFormat("packager-tempfile-%x-%zx-%x", process_id, thread_id,
                         instance_id);
}
}  // namespace

bool TempFilePath(const std::string& temp_dir, std::string* temp_file_path) {
  auto temp_dir_path = std::filesystem::u8path(temp_dir);
  *temp_file_path = (temp_dir_path / TempFileName()).string();
  return true;
}

}  // namespace shaka
