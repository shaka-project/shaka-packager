// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/safe_strerror_posix.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#endif  // OS_MACOSX

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#include "third_party/ashmem/ashmem.h"
#endif

namespace base {

namespace {

// Paranoia. Semaphores and shared memory segments should live in different
// namespaces, but who knows what's out there.
const char kSemaphoreSuffix[] = "-sem";

LazyInstance<Lock>::Leaky g_thread_lock_ = LAZY_INSTANCE_INITIALIZER;

}

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      inode_(0),
      mapped_size_(0),
      memory_(NULL),
      read_only_(false),
      requested_size_(0) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only)
    : mapped_file_(handle.fd),
      inode_(0),
      mapped_size_(0),
      memory_(NULL),
      read_only_(read_only),
      requested_size_(0) {
  struct stat st;
  if (fstat(handle.fd, &st) == 0) {
    // If fstat fails, then the file descriptor is invalid and we'll learn this
    // fact when Map() fails.
    inode_ = st.st_ino;
  }
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only,
                           ProcessHandle process)
    : mapped_file_(handle.fd),
      inode_(0),
      mapped_size_(0),
      memory_(NULL),
      read_only_(read_only),
      requested_size_(0) {
  // We don't handle this case yet (note the ignored parameter); let's die if
  // someone comes calling.
  NOTREACHED();
}

SharedMemory::~SharedMemory() {
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.fd >= 0;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  DCHECK_GE(handle.fd, 0);
  if (HANDLE_EINTR(close(handle.fd)) < 0)
    DPLOG(ERROR) << "close";
}

// static
size_t SharedMemory::GetHandleLimit() {
  return base::GetMaxFds();
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

#if !defined(OS_ANDROID)
// Chromium mostly only uses the unique/private shmem as specified by
// "name == L"". The exception is in the StatsTable.
// TODO(jrg): there is no way to "clean up" all unused named shmem if
// we restart from a crash.  (That isn't a new problem, but it is a problem.)
// In case we want to delete it later, it may be useful to save the value
// of mem_filename after FilePathForMemoryName().
bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK_EQ(-1, mapped_file_);
  if (options.size == 0) return false;

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FILE *fp;
  bool fix_size = true;

  FilePath path;
  if (options.name == NULL || options.name->empty()) {
    // It doesn't make sense to have a open-existing private piece of shmem
    DCHECK(!options.open_existing);
    // Q: Why not use the shm_open() etc. APIs?
    // A: Because they're limited to 4mb on OS X.  FFFFFFFUUUUUUUUUUU
    fp = file_util::CreateAndOpenTemporaryShmemFile(&path, options.executable);

    // Deleting the file prevents anyone else from mapping it in (making it
    // private), and prevents the need for cleanup (once the last fd is closed,
    // it is truly freed).
    if (fp) {
      if (unlink(path.value().c_str()))
        PLOG(WARNING) << "unlink";
    }
  } else {
    if (!FilePathForMemoryName(*options.name, &path))
      return false;

    // Make sure that the file is opened without any permission
    // to other users on the system.
    const mode_t kOwnerOnly = S_IRUSR | S_IWUSR;

    // First, try to create the file.
    int fd = HANDLE_EINTR(
        open(path.value().c_str(), O_RDWR | O_CREAT | O_EXCL, kOwnerOnly));
    if (fd == -1 && options.open_existing) {
      // If this doesn't work, try and open an existing file in append mode.
      // Opening an existing file in a world writable directory has two main
      // security implications:
      // - Attackers could plant a file under their control, so ownership of
      //   the file is checked below.
      // - Attackers could plant a symbolic link so that an unexpected file
      //   is opened, so O_NOFOLLOW is passed to open().
      fd = HANDLE_EINTR(
          open(path.value().c_str(), O_RDWR | O_APPEND | O_NOFOLLOW));

      // Check that the current user owns the file.
      // If uid != euid, then a more complex permission model is used and this
      // API is not appropriate.
      const uid_t real_uid = getuid();
      const uid_t effective_uid = geteuid();
      struct stat sb;
      if (fd >= 0 &&
          (fstat(fd, &sb) != 0 || sb.st_uid != real_uid ||
           sb.st_uid != effective_uid)) {
        LOG(ERROR) <<
            "Invalid owner when opening existing shared memory file.";
        HANDLE_EINTR(close(fd));
        return false;
      }

      // An existing file was opened, so its size should not be fixed.
      fix_size = false;
    }
    fp = NULL;
    if (fd >= 0) {
      // "a+" is always appropriate: if it's a new file, a+ is similar to w+.
      fp = fdopen(fd, "a+");
    }
  }
  if (fp && fix_size) {
    // Get current size.
    struct stat stat;
    if (fstat(fileno(fp), &stat) != 0) {
      file_util::CloseFile(fp);
      return false;
    }
    const size_t current_size = stat.st_size;
    if (current_size != options.size) {
      if (HANDLE_EINTR(ftruncate(fileno(fp), options.size)) != 0) {
        file_util::CloseFile(fp);
        return false;
      }
    }
    requested_size_ = options.size;
  }
  if (fp == NULL) {
#if !defined(OS_MACOSX)
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
    FilePath dir = path.DirName();
    if (access(dir.value().c_str(), W_OK | X_OK) < 0) {
      PLOG(ERROR) << "Unable to access(W_OK|X_OK) " << dir.value();
      if (dir.value() == "/dev/shm") {
        LOG(FATAL) << "This is frequently caused by incorrect permissions on "
                   << "/dev/shm.  Try 'sudo chmod 1777 /dev/shm' to fix.";
      }
    }
#else
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
#endif
    return false;
  }

  return PrepareMapFile(fp);
}

// Our current implementation of shmem is with mmap()ing of files.
// These files need to be deleted explicitly.
// In practice this call is only needed for unit tests.
bool SharedMemory::Delete(const std::string& name) {
  FilePath path;
  if (!FilePathForMemoryName(name, &path))
    return false;

  if (PathExists(path))
    return base::DeleteFile(path, false);

  // Doesn't exist, so success.
  return true;
}

bool SharedMemory::Open(const std::string& name, bool read_only) {
  FilePath path;
  if (!FilePathForMemoryName(name, &path))
    return false;

  read_only_ = read_only;

  const char *mode = read_only ? "r" : "r+";
  FILE *fp = file_util::OpenFile(path, mode);
  return PrepareMapFile(fp);
}

#endif  // !defined(OS_ANDROID)

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (mapped_file_ == -1)
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

#if defined(OS_ANDROID)
  // On Android, Map can be called with a size and offset of zero to use the
  // ashmem-determined size.
  if (bytes == 0) {
    DCHECK_EQ(0, offset);
    int ashmem_bytes = ashmem_get_size_region(mapped_file_);
    if (ashmem_bytes < 0)
      return false;
    bytes = ashmem_bytes;
  }
#endif

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, mapped_file_, offset);

  bool mmap_succeeded = memory_ != (void*)-1 && memory_ != NULL;
  if (mmap_succeeded) {
    mapped_size_ = bytes;
    DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
        (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
  } else {
    memory_ = NULL;
  }

  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  munmap(memory_, mapped_size_);
  memory_ = NULL;
  mapped_size_ = 0;
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return FileDescriptor(mapped_file_, false);
}

void SharedMemory::Close() {
  Unmap();

  if (mapped_file_ > 0) {
    if (HANDLE_EINTR(close(mapped_file_)) < 0)
      PLOG(ERROR) << "close";
    mapped_file_ = -1;
  }
}

void SharedMemory::Lock() {
  g_thread_lock_.Get().Acquire();
  LockOrUnlockCommon(F_LOCK);
}

void SharedMemory::Unlock() {
  LockOrUnlockCommon(F_ULOCK);
  g_thread_lock_.Get().Release();
}

#if !defined(OS_ANDROID)
bool SharedMemory::PrepareMapFile(FILE *fp) {
  DCHECK_EQ(-1, mapped_file_);
  if (fp == NULL) return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  file_util::ScopedFILE file_closer(fp);

  mapped_file_ = dup(fileno(fp));
  if (mapped_file_ == -1) {
    if (errno == EMFILE) {
      LOG(WARNING) << "Shared memory creation failed; out of file descriptors";
      return false;
    } else {
      NOTREACHED() << "Call to dup failed, errno=" << errno;
    }
  }

  struct stat st;
  if (fstat(mapped_file_, &st))
    NOTREACHED();
  inode_ = st.st_ino;

  return true;
}
#endif

// For the given shmem named |mem_name|, return a filename to mmap()
// (and possibly create).  Modifies |filename|.  Return false on
// error, or true of we are happy.
bool SharedMemory::FilePathForMemoryName(const std::string& mem_name,
                                         FilePath* path) {
  // mem_name will be used for a filename; make sure it doesn't
  // contain anything which will confuse us.
  DCHECK_EQ(std::string::npos, mem_name.find('/'));
  DCHECK_EQ(std::string::npos, mem_name.find('\0'));

  FilePath temp_dir;
  if (!file_util::GetShmemTempDir(&temp_dir, false))
    return false;

#if !defined(OS_MACOSX)
#if defined(GOOGLE_CHROME_BUILD)
  std::string name_base = std::string("com.google.Chrome");
#else
  std::string name_base = std::string("org.chromium.Chromium");
#endif
#else  // OS_MACOSX
  std::string name_base = std::string(base::mac::BaseBundleID());
#endif  // OS_MACOSX
  *path = temp_dir.AppendASCII(name_base + ".shmem." + mem_name);
  return true;
}

void SharedMemory::LockOrUnlockCommon(int function) {
  DCHECK_GE(mapped_file_, 0);
  while (lockf(mapped_file_, function, 0) < 0) {
    if (errno == EINTR) {
      continue;
    } else if (errno == ENOLCK) {
      // temporary kernel resource exaustion
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(500));
      continue;
    } else {
      NOTREACHED() << "lockf() failed."
                   << " function:" << function
                   << " fd:" << mapped_file_
                   << " errno:" << errno
                   << " msg:" << safe_strerror(errno);
    }
  }
}

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
                                        SharedMemoryHandle *new_handle,
                                        bool close_self) {
  const int new_fd = dup(mapped_file_);
  if (new_fd < 0) {
    DPLOG(ERROR) << "dup() failed.";
    return false;
  }

  new_handle->fd = new_fd;
  new_handle->auto_close = true;

  if (close_self)
    Close();

  return true;
}

}  // namespace base
