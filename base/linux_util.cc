// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/linux_util.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"

namespace {

// Not needed for OS_CHROMEOS.
#if defined(OS_LINUX)
enum LinuxDistroState {
  STATE_DID_NOT_CHECK  = 0,
  STATE_CHECK_STARTED  = 1,
  STATE_CHECK_FINISHED = 2,
};

// Helper class for GetLinuxDistro().
class LinuxDistroHelper {
 public:
  // Retrieves the Singleton.
  static LinuxDistroHelper* GetInstance() {
    return Singleton<LinuxDistroHelper>::get();
  }

  // The simple state machine goes from:
  // STATE_DID_NOT_CHECK -> STATE_CHECK_STARTED -> STATE_CHECK_FINISHED.
  LinuxDistroHelper() : state_(STATE_DID_NOT_CHECK) {}
  ~LinuxDistroHelper() {}

  // Retrieve the current state, if we're in STATE_DID_NOT_CHECK,
  // we automatically move to STATE_CHECK_STARTED so nobody else will
  // do the check.
  LinuxDistroState State() {
    base::AutoLock scoped_lock(lock_);
    if (STATE_DID_NOT_CHECK == state_) {
      state_ = STATE_CHECK_STARTED;
      return STATE_DID_NOT_CHECK;
    }
    return state_;
  }

  // Indicate the check finished, move to STATE_CHECK_FINISHED.
  void CheckFinished() {
    base::AutoLock scoped_lock(lock_);
    DCHECK_EQ(STATE_CHECK_STARTED, state_);
    state_ = STATE_CHECK_FINISHED;
  }

 private:
  base::Lock lock_;
  LinuxDistroState state_;
};
#endif  // if defined(OS_LINUX)

// expected prefix of the target of the /proc/self/fd/%d link for a socket
const char kSocketLinkPrefix[] = "socket:[";

// Parse a symlink in /proc/pid/fd/$x and return the inode number of the
// socket.
//   inode_out: (output) set to the inode number on success
//   path: e.g. /proc/1234/fd/5 (must be a UNIX domain socket descriptor)
//   log: if true, log messages about failure details
bool ProcPathGetInode(ino_t* inode_out, const char* path, bool log = false) {
  DCHECK(inode_out);
  DCHECK(path);

  char buf[256];
  const ssize_t n = readlink(path, buf, sizeof(buf) - 1);
  if (n == -1) {
    if (log) {
      DLOG(WARNING) << "Failed to read the inode number for a socket from /proc"
                      "(" << errno << ")";
    }
    return false;
  }
  buf[n] = 0;

  if (memcmp(kSocketLinkPrefix, buf, sizeof(kSocketLinkPrefix) - 1)) {
    if (log) {
      DLOG(WARNING) << "The descriptor passed from the crashing process wasn't "
                      " a UNIX domain socket.";
    }
    return false;
  }

  char* endptr;
  const unsigned long long int inode_ul =
      strtoull(buf + sizeof(kSocketLinkPrefix) - 1, &endptr, 10);
  if (*endptr != ']')
    return false;

  if (inode_ul == ULLONG_MAX) {
    if (log) {
      DLOG(WARNING) << "Failed to parse a socket's inode number: the number "
                       "was too large. Please report this bug: " << buf;
    }
    return false;
  }

  *inode_out = inode_ul;
  return true;
}

}  // namespace

namespace base {

const char kFindInodeSwitch[] = "--find-inode";

// Account for the terminating null character.
static const int kDistroSize = 128 + 1;

// We use this static string to hold the Linux distro info. If we
// crash, the crash handler code will send this in the crash dump.
char g_linux_distro[kDistroSize] =
#if defined(OS_CHROMEOS)
    "CrOS";
#elif defined(OS_ANDROID)
    "Android";
#else  // if defined(OS_LINUX)
    "Unknown";
#endif

std::string GetLinuxDistro() {
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  return g_linux_distro;
#elif defined(OS_LINUX)
  LinuxDistroHelper* distro_state_singleton = LinuxDistroHelper::GetInstance();
  LinuxDistroState state = distro_state_singleton->State();
  if (STATE_CHECK_FINISHED == state)
    return g_linux_distro;
  if (STATE_CHECK_STARTED == state)
    return "Unknown"; // Don't wait for other thread to finish.
  DCHECK_EQ(state, STATE_DID_NOT_CHECK);
  // We do this check only once per process. If it fails, there's
  // little reason to believe it will work if we attempt to run
  // lsb_release again.
  std::vector<std::string> argv;
  argv.push_back("lsb_release");
  argv.push_back("-d");
  std::string output;
  base::GetAppOutput(CommandLine(argv), &output);
  if (output.length() > 0) {
    // lsb_release -d should return: Description:<tab>Distro Info
    const char field[] = "Description:\t";
    if (output.compare(0, strlen(field), field) == 0) {
      SetLinuxDistro(output.substr(strlen(field)));
    }
  }
  distro_state_singleton->CheckFinished();
  return g_linux_distro;
#else
  NOTIMPLEMENTED();
  return "Unknown";
#endif
}

void SetLinuxDistro(const std::string& distro) {
  std::string trimmed_distro;
  TrimWhitespaceASCII(distro, TRIM_ALL, &trimmed_distro);
  base::strlcpy(g_linux_distro, trimmed_distro.c_str(), kDistroSize);
}

bool FileDescriptorGetInode(ino_t* inode_out, int fd) {
  DCHECK(inode_out);

  struct stat buf;
  if (fstat(fd, &buf) < 0)
    return false;

  if (!S_ISSOCK(buf.st_mode))
    return false;

  *inode_out = buf.st_ino;
  return true;
}

bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode) {
  DCHECK(pid_out);
  bool already_found = false;

  DIR* proc = opendir("/proc");
  if (!proc) {
    DLOG(WARNING) << "Cannot open /proc";
    return false;
  }

  std::vector<pid_t> pids;

  struct dirent* dent;
  while ((dent = readdir(proc))) {
    char* endptr;
    const unsigned long int pid_ul = strtoul(dent->d_name, &endptr, 10);
    if (pid_ul == ULONG_MAX || *endptr)
      continue;
    pids.push_back(pid_ul);
  }
  closedir(proc);

  for (std::vector<pid_t>::const_iterator
       i = pids.begin(); i != pids.end(); ++i) {
    const pid_t current_pid = *i;
    char buf[256];
    snprintf(buf, sizeof(buf), "/proc/%d/fd", current_pid);
    DIR* fd = opendir(buf);
    if (!fd)
      continue;

    while ((dent = readdir(fd))) {
      if (snprintf(buf, sizeof(buf), "/proc/%d/fd/%s", current_pid,
                   dent->d_name) >= static_cast<int>(sizeof(buf))) {
        continue;
      }

      ino_t fd_inode = static_cast<ino_t>(-1);
      if (ProcPathGetInode(&fd_inode, buf)) {
        if (fd_inode == socket_inode) {
          if (already_found) {
            closedir(fd);
            return false;
          }

          already_found = true;
          *pid_out = current_pid;
          break;
        }
      }
    }

    closedir(fd);
  }

  return already_found;
}

pid_t FindThreadIDWithSyscall(pid_t pid, const std::string& expected_data,
                              bool* syscall_supported) {
  char buf[256];
  snprintf(buf, sizeof(buf), "/proc/%d/task", pid);

  if (syscall_supported != NULL)
    *syscall_supported = false;

  DIR* task = opendir(buf);
  if (!task) {
    DLOG(WARNING) << "Cannot open " << buf;
    return -1;
  }

  std::vector<pid_t> tids;
  struct dirent* dent;
  while ((dent = readdir(task))) {
    char* endptr;
    const unsigned long int tid_ul = strtoul(dent->d_name, &endptr, 10);
    if (tid_ul == ULONG_MAX || *endptr)
      continue;
    tids.push_back(tid_ul);
  }
  closedir(task);

  scoped_ptr<char[]> syscall_data(new char[expected_data.length()]);
  for (std::vector<pid_t>::const_iterator
       i = tids.begin(); i != tids.end(); ++i) {
    const pid_t current_tid = *i;
    snprintf(buf, sizeof(buf), "/proc/%d/task/%d/syscall", pid, current_tid);
    int fd = open(buf, O_RDONLY);
    if (fd < 0)
      continue;
    if (syscall_supported != NULL)
      *syscall_supported = true;
    bool read_ret =
        file_util::ReadFromFD(fd, syscall_data.get(), expected_data.length());
    close(fd);
    if (!read_ret)
      continue;

    if (0 == strncmp(expected_data.c_str(), syscall_data.get(),
                     expected_data.length())) {
      return current_tid;
    }
  }
  return -1;
}

}  // namespace base
