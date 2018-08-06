// Copyright 2016 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/file/file_util.h"

#include <inttypes.h>

#include "packager/base/files/file_path.h"
#include "packager/base/files/file_util.h"
#include "packager/base/process/process_handle.h"
#include "packager/base/strings/stringprintf.h"
#include "packager/base/threading/platform_thread.h"
#include "packager/base/time/time.h"

namespace shaka {
namespace {
// Create a temp file name using process id, thread id and current time.
std::string TempFileName() {
  const int32_t process_id = static_cast<int32_t>(base::GetCurrentProcId());
  const int32_t thread_id =
      static_cast<int32_t>(base::PlatformThread::CurrentId());

  // We may need two or more temporary files in the same thread. There might be
  // name collision if they are requested around the same time, e.g. called
  // consecutively. Use a thread_local instance to avoid that.
  static thread_local int32_t instance_id = 0;
  ++instance_id;

  const int64_t current_time = base::Time::Now().ToInternalValue();
  return base::StringPrintf("packager-tempfile-%x-%x-%x-%" PRIx64, process_id,
                            thread_id, instance_id, current_time);
}
}  // namespace

bool TempFilePath(const std::string& temp_dir, std::string* temp_file_path) {
  if (temp_dir.empty()) {
    base::FilePath file_path;
    if (!base::CreateTemporaryFile(&file_path)) {
      LOG(ERROR) << "Failed to create temporary file.";
      return false;
    }
    *temp_file_path = file_path.AsUTF8Unsafe();
  } else {
    *temp_file_path =
        base::FilePath::FromUTF8Unsafe(temp_dir)
            .Append(base::FilePath::FromUTF8Unsafe(TempFileName()))
            .AsUTF8Unsafe();
  }
  return true;
}

}  // namespace shaka
