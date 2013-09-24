// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cross platform methods for FilePathWatcher. See the various platform
// specific implementation files, too.

#include "base/files/file_path_watcher.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace base {

FilePathWatcher::~FilePathWatcher() {
  impl_->Cancel();
}

// static
void FilePathWatcher::CancelWatch(
    const scoped_refptr<PlatformDelegate>& delegate) {
  delegate->CancelOnMessageLoopThread();
}

FilePathWatcher::PlatformDelegate::PlatformDelegate(): cancelled_(false) {
}

FilePathWatcher::PlatformDelegate::~PlatformDelegate() {
  DCHECK(is_cancelled());
}

bool FilePathWatcher::Watch(const FilePath& path,
                            bool recursive,
                            const Callback& callback) {
  DCHECK(path.IsAbsolute());
  return impl_->Watch(path, recursive, callback);
}

}  // namespace base
