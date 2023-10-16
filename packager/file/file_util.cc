// Copyright 2016 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/file/file_util.h>

#if defined(OS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <thread>

#include <absl/strings/str_format.h>

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
  std::filesystem::path temp_dir_path;

  if (temp_dir.empty()) {
    temp_dir_path = std::filesystem::temp_directory_path();
  } else {
    temp_dir_path = std::filesystem::u8path(temp_dir);
  }

  *temp_file_path = (temp_dir_path / TempFileName()).string();
  return true;
}

std::string MakePathRelative(const std::filesystem::path& media_path,
                             const std::filesystem::path& parent_path) {
  auto relative_path = std::filesystem::relative(media_path, parent_path);
  if (relative_path.empty() || *relative_path.begin() == "..") {
    // Not related.
    relative_path = media_path;
  }

  return relative_path.lexically_normal().generic_string();
}

}  // namespace shaka
