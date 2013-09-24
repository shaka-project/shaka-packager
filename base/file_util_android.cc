// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

namespace file_util {

bool GetShmemTempDir(base::FilePath* path, bool executable) {
  return PathService::Get(base::DIR_CACHE, path);
}

}  // namespace file_util
