// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/sparse_histogram.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#endif

namespace base {

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(PLATFORM_FILE_FROM_BEGIN   == SEEK_SET &&
               PLATFORM_FILE_FROM_CURRENT == SEEK_CUR &&
               PLATFORM_FILE_FROM_END     == SEEK_END, whence_matches_system);

namespace {

#if defined(OS_BSD) || defined(OS_MACOSX) || defined(OS_NACL)
typedef struct stat stat_wrapper_t;
static int CallFstat(int fd, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return fstat(fd, sb);
}
#else
typedef struct stat64 stat_wrapper_t;
static int CallFstat(int fd, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return fstat64(fd, sb);
}
#endif

// NaCl doesn't provide the following system calls, so either simulate them or
// wrap them in order to minimize the number of #ifdef's in this file.
#if !defined(OS_NACL)
static int DoPread(PlatformFile file, char* data, int size, int64 offset) {
  return HANDLE_EINTR(pread(file, data, size, offset));
}

static int DoPwrite(PlatformFile file, const char* data, int size,
                      int64 offset) {
  return HANDLE_EINTR(pwrite(file, data, size, offset));
}

static bool IsOpenAppend(PlatformFile file) {
  return (fcntl(file, F_GETFL) & O_APPEND) != 0;
}

static int CallFtruncate(PlatformFile file, int64 length) {
  return HANDLE_EINTR(ftruncate(file, length));
}

static int CallFsync(PlatformFile file) {
  return HANDLE_EINTR(fsync(file));
}

static int CallFutimes(PlatformFile file, const struct timeval times[2]) {
#ifdef __USE_XOPEN2K8
  // futimens should be available, but futimes might not be
  // http://pubs.opengroup.org/onlinepubs/9699919799/

  timespec ts_times[2];
  ts_times[0].tv_sec  = times[0].tv_sec;
  ts_times[0].tv_nsec = times[0].tv_usec * 1000;
  ts_times[1].tv_sec  = times[1].tv_sec;
  ts_times[1].tv_nsec = times[1].tv_usec * 1000;

  return futimens(file, ts_times);
#else
  return futimes(file, times);
#endif
}
#else  // defined(OS_NACL)
// TODO(bbudge) Remove DoPread, DoPwrite when NaCl implements pread, pwrite.
static int DoPread(PlatformFile file, char* data, int size, int64 offset) {
  lseek(file, static_cast<off_t>(offset), SEEK_SET);
  return HANDLE_EINTR(read(file, data, size));
}

static int DoPwrite(PlatformFile file, const char* data, int size,
                      int64 offset) {
  lseek(file, static_cast<off_t>(offset), SEEK_SET);
  return HANDLE_EINTR(write(file, data, size));
}

static bool IsOpenAppend(PlatformFile file) {
  // NaCl doesn't implement fcntl. Since NaCl's write conforms to the POSIX
  // standard and always appends if the file is opened with O_APPEND, just
  // return false here.
  return false;
}

static int CallFtruncate(PlatformFile file, int64 length) {
  NOTIMPLEMENTED();  // NaCl doesn't implement ftruncate.
  return 0;
}

static int CallFsync(PlatformFile file) {
  NOTIMPLEMENTED();  // NaCl doesn't implement fsync.
  return 0;
}

static int CallFutimes(PlatformFile file, const struct timeval times[2]) {
  NOTIMPLEMENTED();  // NaCl doesn't implement futimes.
  return 0;
}
#endif  // defined(OS_NACL)

}  // namespace

// NaCl doesn't implement system calls to open files directly.
#if !defined(OS_NACL)
// TODO(erikkay): does it make sense to support PLATFORM_FILE_EXCLUSIVE_* here?
PlatformFile CreatePlatformFileUnsafe(const FilePath& name,
                                      int flags,
                                      bool* created,
                                      PlatformFileError* error) {
  base::ThreadRestrictions::AssertIOAllowed();

  int open_flags = 0;
  if (flags & PLATFORM_FILE_CREATE)
    open_flags = O_CREAT | O_EXCL;

  if (created)
    *created = false;

  if (flags & PLATFORM_FILE_CREATE_ALWAYS) {
    DCHECK(!open_flags);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (flags & PLATFORM_FILE_OPEN_TRUNCATED) {
    DCHECK(!open_flags);
    DCHECK(flags & PLATFORM_FILE_WRITE);
    open_flags = O_TRUNC;
  }

  if (!open_flags && !(flags & PLATFORM_FILE_OPEN) &&
      !(flags & PLATFORM_FILE_OPEN_ALWAYS)) {
    NOTREACHED();
    errno = EOPNOTSUPP;
    if (error)
      *error = PLATFORM_FILE_ERROR_FAILED;
    return kInvalidPlatformFileValue;
  }

  if (flags & PLATFORM_FILE_WRITE && flags & PLATFORM_FILE_READ) {
    open_flags |= O_RDWR;
  } else if (flags & PLATFORM_FILE_WRITE) {
    open_flags |= O_WRONLY;
  } else if (!(flags & PLATFORM_FILE_READ) &&
             !(flags & PLATFORM_FILE_WRITE_ATTRIBUTES) &&
             !(flags & PLATFORM_FILE_APPEND) &&
             !(flags & PLATFORM_FILE_OPEN_ALWAYS)) {
    NOTREACHED();
  }

  if (flags & PLATFORM_FILE_TERMINAL_DEVICE)
    open_flags |= O_NOCTTY | O_NDELAY;

  if (flags & PLATFORM_FILE_APPEND && flags & PLATFORM_FILE_READ)
    open_flags |= O_APPEND | O_RDWR;
  else if (flags & PLATFORM_FILE_APPEND)
    open_flags |= O_APPEND | O_WRONLY;

  COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_equal_zero);

  int mode = S_IRUSR | S_IWUSR;
#if defined(OS_CHROMEOS)
  mode |= S_IRGRP | S_IROTH;
#endif

  int descriptor =
      HANDLE_EINTR(open(name.value().c_str(), open_flags, mode));

  if (flags & PLATFORM_FILE_OPEN_ALWAYS) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      if (flags & PLATFORM_FILE_EXCLUSIVE_READ ||
          flags & PLATFORM_FILE_EXCLUSIVE_WRITE) {
        open_flags |= O_EXCL;   // together with O_CREAT implies O_NOFOLLOW
      }
      descriptor = HANDLE_EINTR(
          open(name.value().c_str(), open_flags, mode));
      if (created && descriptor >= 0)
        *created = true;
    }
  }

  if (created && (descriptor >= 0) &&
      (flags & (PLATFORM_FILE_CREATE_ALWAYS | PLATFORM_FILE_CREATE)))
    *created = true;

  if ((descriptor >= 0) && (flags & PLATFORM_FILE_DELETE_ON_CLOSE)) {
    unlink(name.value().c_str());
  }

  if (error) {
    if (descriptor >= 0)
      *error = PLATFORM_FILE_OK;
    else
      *error = ErrnoToPlatformFileError(errno);
  }

  return descriptor;
}

FILE* FdopenPlatformFile(PlatformFile file, const char* mode) {
  return fdopen(file, mode);
}
#endif  // !defined(OS_NACL)

bool ClosePlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return !HANDLE_EINTR(close(file));
}

int64 SeekPlatformFile(PlatformFile file,
                       PlatformFileWhence whence,
                       int64 offset) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || offset < 0)
    return -1;

  return lseek(file, static_cast<off_t>(offset), static_cast<int>(whence));
}

int ReadPlatformFile(PlatformFile file, int64 offset, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_read = 0;
  int rv;
  do {
    rv = DoPread(file, data + bytes_read,
                 size - bytes_read, offset + bytes_read);
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

int ReadPlatformFileAtCurrentPos(PlatformFile file, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_read = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(read(file, data, size));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

int ReadPlatformFileNoBestEffort(PlatformFile file, int64 offset,
                                 char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0)
    return -1;

  return DoPread(file, data, size, offset);
}

int ReadPlatformFileCurPosNoBestEffort(PlatformFile file,
                                       char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  return HANDLE_EINTR(read(file, data, size));
}

int WritePlatformFile(PlatformFile file, int64 offset,
                      const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (IsOpenAppend(file))
    return WritePlatformFileAtCurrentPos(file, data, size);

  if (file < 0 || size < 0)
    return -1;

  int bytes_written = 0;
  int rv;
  do {
    rv = DoPwrite(file, data + bytes_written,
                  size - bytes_written, offset + bytes_written);
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

int WritePlatformFileAtCurrentPos(PlatformFile file,
                                  const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_written = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(write(file, data, size));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

int WritePlatformFileCurPosNoBestEffort(PlatformFile file,
                                        const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  return HANDLE_EINTR(write(file, data, size));
}

bool TruncatePlatformFile(PlatformFile file, int64 length) {
  base::ThreadRestrictions::AssertIOAllowed();
  return ((file >= 0) && !CallFtruncate(file, length));
}

bool FlushPlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return !CallFsync(file);
}

bool TouchPlatformFile(PlatformFile file, const base::Time& last_access_time,
                       const base::Time& last_modified_time) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0)
    return false;

  timeval times[2];
  times[0] = last_access_time.ToTimeVal();
  times[1] = last_modified_time.ToTimeVal();

  return !CallFutimes(file, times);
}

bool GetPlatformFileInfo(PlatformFile file, PlatformFileInfo* info) {
  if (!info)
    return false;

  stat_wrapper_t file_info;
  if (CallFstat(file, &file_info))
    return false;

  info->is_directory = S_ISDIR(file_info.st_mode);
  info->is_symbolic_link = S_ISLNK(file_info.st_mode);
  info->size = file_info.st_size;

#if defined(OS_LINUX)
  const time_t last_modified_sec = file_info.st_mtim.tv_sec;
  const int64 last_modified_nsec = file_info.st_mtim.tv_nsec;
  const time_t last_accessed_sec = file_info.st_atim.tv_sec;
  const int64 last_accessed_nsec = file_info.st_atim.tv_nsec;
  const time_t creation_time_sec = file_info.st_ctim.tv_sec;
  const int64 creation_time_nsec = file_info.st_ctim.tv_nsec;
#elif defined(OS_ANDROID)
  const time_t last_modified_sec = file_info.st_mtime;
  const int64 last_modified_nsec = file_info.st_mtime_nsec;
  const time_t last_accessed_sec = file_info.st_atime;
  const int64 last_accessed_nsec = file_info.st_atime_nsec;
  const time_t creation_time_sec = file_info.st_ctime;
  const int64 creation_time_nsec = file_info.st_ctime_nsec;
#elif defined(OS_MACOSX) || defined(OS_IOS) || defined(OS_BSD)
  const time_t last_modified_sec = file_info.st_mtimespec.tv_sec;
  const int64 last_modified_nsec = file_info.st_mtimespec.tv_nsec;
  const time_t last_accessed_sec = file_info.st_atimespec.tv_sec;
  const int64 last_accessed_nsec = file_info.st_atimespec.tv_nsec;
  const time_t creation_time_sec = file_info.st_ctimespec.tv_sec;
  const int64 creation_time_nsec = file_info.st_ctimespec.tv_nsec;
#else
  // TODO(gavinp): Investigate a good high resolution option for OS_NACL.
  const time_t last_modified_sec = file_info.st_mtime;
  const int64 last_modified_nsec = 0;
  const time_t last_accessed_sec = file_info.st_atime;
  const int64 last_accessed_nsec = 0;
  const time_t creation_time_sec = file_info.st_ctime;
  const int64 creation_time_nsec = 0;
#endif

  info->last_modified =
      base::Time::FromTimeT(last_modified_sec) +
      base::TimeDelta::FromMicroseconds(last_modified_nsec /
                                        base::Time::kNanosecondsPerMicrosecond);
  info->last_accessed =
      base::Time::FromTimeT(last_accessed_sec) +
      base::TimeDelta::FromMicroseconds(last_accessed_nsec /
                                        base::Time::kNanosecondsPerMicrosecond);
  info->creation_time =
      base::Time::FromTimeT(creation_time_sec) +
      base::TimeDelta::FromMicroseconds(creation_time_nsec /
                                        base::Time::kNanosecondsPerMicrosecond);
  return true;
}

PlatformFileError ErrnoToPlatformFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return PLATFORM_FILE_ERROR_ACCESS_DENIED;
#if !defined(OS_NACL)  // ETXTBSY not defined by NaCl.
    case ETXTBSY:
      return PLATFORM_FILE_ERROR_IN_USE;
#endif
    case EEXIST:
      return PLATFORM_FILE_ERROR_EXISTS;
    case ENOENT:
      return PLATFORM_FILE_ERROR_NOT_FOUND;
    case EMFILE:
      return PLATFORM_FILE_ERROR_TOO_MANY_OPENED;
    case ENOMEM:
      return PLATFORM_FILE_ERROR_NO_MEMORY;
    case ENOSPC:
      return PLATFORM_FILE_ERROR_NO_SPACE;
    case ENOTDIR:
      return PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    default:
#if !defined(OS_NACL)  // NaCl build has no metrics code.
      UMA_HISTOGRAM_SPARSE_SLOWLY("PlatformFile.UnknownErrors.Posix",
                                  saved_errno);
#endif
      return PLATFORM_FILE_ERROR_FAILED;
  }
}

}  // namespace base
