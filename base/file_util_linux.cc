// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include "base/files/file_path.h"

#include <errno.h>
#include <sys/vfs.h>

namespace file_util {

bool GetFileSystemType(const base::FilePath& path, FileSystemType* type) {
  struct statfs statfs_buf;
  if (statfs(path.value().c_str(), &statfs_buf) < 0) {
    if (errno == ENOENT)
      return false;
    *type = FILE_SYSTEM_UNKNOWN;
    return true;
  }

  // While you would think the possible values of f_type would be available
  // in a header somewhere, it appears that is not the case.  These values
  // are copied from the statfs man page.
  switch (statfs_buf.f_type) {
    case 0:
      *type = FILE_SYSTEM_0;
      break;
    case 0xEF53:  // ext2, ext3.
    case 0x4D44:  // dos
    case 0x5346544E:  // NFTS
    case 0x52654973:  // reiser
    case 0x58465342:  // XFS
    case 0x9123683E:  // btrfs
    case 0x3153464A:  // JFS
      *type = FILE_SYSTEM_ORDINARY;
      break;
    case 0x6969:  // NFS
      *type = FILE_SYSTEM_NFS;
      break;
    case 0xFF534D42:  // CIFS
    case 0x517B:  // SMB
      *type = FILE_SYSTEM_SMB;
      break;
    case 0x73757245:  // Coda
      *type = FILE_SYSTEM_CODA;
      break;
    case 0x858458f6:  // ramfs
    case 0x01021994:  // tmpfs
      *type = FILE_SYSTEM_MEMORY;
      break;
    case 0x27e0eb: // CGROUP
      *type = FILE_SYSTEM_CGROUP;
      break;
    default:
      *type = FILE_SYSTEM_OTHER;
  }
  return true;
}

}  // namespace
