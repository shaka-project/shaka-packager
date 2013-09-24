// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using base::MakeAbsoluteFilePath;

namespace file_util {

namespace {

// Deny |permission| on the file |path|.
bool DenyFilePermission(const base::FilePath& path, mode_t permission) {
  struct stat stat_buf;
  if (stat(path.value().c_str(), &stat_buf) != 0)
    return false;
  stat_buf.st_mode &= ~permission;

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), stat_buf.st_mode));
  return rv == 0;
}

// Gets a blob indicating the permission information for |path|.
// |length| is the length of the blob.  Zero on failure.
// Returns the blob pointer, or NULL on failure.
void* GetPermissionInfo(const base::FilePath& path, size_t* length) {
  DCHECK(length);
  *length = 0;

  struct stat stat_buf;
  if (stat(path.value().c_str(), &stat_buf) != 0)
    return NULL;

  *length = sizeof(mode_t);
  mode_t* mode = new mode_t;
  *mode = stat_buf.st_mode & ~S_IFMT;  // Filter out file/path kind.

  return mode;
}

// Restores the permission information for |path|, given the blob retrieved
// using |GetPermissionInfo()|.
// |info| is the pointer to the blob.
// |length| is the length of the blob.
// Either |info| or |length| may be NULL/0, in which case nothing happens.
bool RestorePermissionInfo(const base::FilePath& path,
                           void* info, size_t length) {
  if (!info || (length == 0))
    return false;

  DCHECK_EQ(sizeof(mode_t), length);
  mode_t* mode = reinterpret_cast<mode_t*>(info);

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), *mode));

  delete mode;

  return rv == 0;
}

}  // namespace

bool DieFileDie(const base::FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on POSIX.
  // Just pass-through.
  return base::DeleteFile(file, recurse);
}

#if !defined(OS_LINUX) && !defined(OS_MACOSX)
bool EvictFileFromSystemCache(const base::FilePath& file) {
  // There doesn't seem to be a POSIX way to cool the disk cache.
  NOTIMPLEMENTED();
  return false;
}
#endif

std::wstring FilePathAsWString(const base::FilePath& path) {
  return UTF8ToWide(path.value());
}
base::FilePath WStringAsFilePath(const std::wstring& path) {
  return base::FilePath(WideToUTF8(path));
}

bool MakeFileUnreadable(const base::FilePath& path) {
  return DenyFilePermission(path, S_IRUSR | S_IRGRP | S_IROTH);
}

bool MakeFileUnwritable(const base::FilePath& path) {
  return DenyFilePermission(path, S_IWUSR | S_IWGRP | S_IWOTH);
}

PermissionRestorer::PermissionRestorer(const base::FilePath& path)
    : path_(path), info_(NULL), length_(0) {
  info_ = GetPermissionInfo(path_, &length_);
  DCHECK(info_ != NULL);
  DCHECK_NE(0u, length_);
}

PermissionRestorer::~PermissionRestorer() {
  if (!RestorePermissionInfo(path_, info_, length_))
    NOTREACHED();
}

}  // namespace file_util
