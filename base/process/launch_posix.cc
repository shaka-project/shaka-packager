// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/launch.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iterator>
#include <limits>
#include <set>

#include "base/allocator/type_profiler_control.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/debug/stack_trace.h"
#include "base/file_util.h"
#include "base/files/dir_reader_posix.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/process_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_CHROMEOS)
#include <sys/ioctl.h>
#endif

#if defined(OS_FREEBSD)
#include <sys/event.h>
#include <sys/ucontext.h>
#endif

#if defined(OS_MACOSX)
#include <crt_externs.h>
#include <sys/event.h>
#else
extern char** environ;
#endif

namespace base {

namespace {

// Get the process's "environment" (i.e. the thing that setenv/getenv
// work with).
char** GetEnvironment() {
#if defined(OS_MACOSX)
  return *_NSGetEnviron();
#else
  return environ;
#endif
}

// Set the process's "environment" (i.e. the thing that setenv/getenv
// work with).
void SetEnvironment(char** env) {
#if defined(OS_MACOSX)
  *_NSGetEnviron() = env;
#else
  environ = env;
#endif
}

// Set the calling thread's signal mask to new_sigmask and return
// the previous signal mask.
sigset_t SetSignalMask(const sigset_t& new_sigmask) {
  sigset_t old_sigmask;
#if defined(OS_ANDROID)
  // POSIX says pthread_sigmask() must be used in multi-threaded processes,
  // but Android's pthread_sigmask() was broken until 4.1:
  // https://code.google.com/p/android/issues/detail?id=15337
  // http://stackoverflow.com/questions/13777109/pthread-sigmask-on-android-not-working
  RAW_CHECK(sigprocmask(SIG_SETMASK, &new_sigmask, &old_sigmask) == 0);
#else
  RAW_CHECK(pthread_sigmask(SIG_SETMASK, &new_sigmask, &old_sigmask) == 0);
#endif
  return old_sigmask;
}

#if !defined(OS_LINUX) || \
    (!defined(__i386__) && !defined(__x86_64__) && !defined(__arm__))
void ResetChildSignalHandlersToDefaults() {
  // The previous signal handlers are likely to be meaningless in the child's
  // context so we reset them to the defaults for now. http://crbug.com/44953
  // These signal handlers are set up at least in browser_main_posix.cc:
  // BrowserMainPartsPosix::PreEarlyInitialization and stack_trace_posix.cc:
  // EnableInProcessStackDumping.
  signal(SIGHUP, SIG_DFL);
  signal(SIGINT, SIG_DFL);
  signal(SIGILL, SIG_DFL);
  signal(SIGABRT, SIG_DFL);
  signal(SIGFPE, SIG_DFL);
  signal(SIGBUS, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
  signal(SIGSYS, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}

#else

// TODO(jln): remove the Linux special case once kernels are fixed.

// Internally the kernel makes sigset_t an array of long large enough to have
// one bit per signal.
typedef uint64_t kernel_sigset_t;

// This is what struct sigaction looks like to the kernel at least on X86 and
// ARM. MIPS, for instance, is very different.
struct kernel_sigaction {
  void* k_sa_handler;  // For this usage it only needs to be a generic pointer.
  unsigned long k_sa_flags;
  void* k_sa_restorer;  // For this usage it only needs to be a generic pointer.
  kernel_sigset_t k_sa_mask;
};

// glibc's sigaction() will prevent access to sa_restorer, so we need to roll
// our own.
int sys_rt_sigaction(int sig, const struct kernel_sigaction* act,
                     struct kernel_sigaction* oact) {
  return syscall(SYS_rt_sigaction, sig, act, oact, sizeof(kernel_sigset_t));
}

// This function is intended to be used in between fork() and execve() and will
// reset all signal handlers to the default.
// The motivation for going through all of them is that sa_restorer can leak
// from parents and help defeat ASLR on buggy kernels.  We reset it to NULL.
// See crbug.com/177956.
void ResetChildSignalHandlersToDefaults(void) {
  for (int signum = 1; ; ++signum) {
    struct kernel_sigaction act = {0};
    int sigaction_get_ret = sys_rt_sigaction(signum, NULL, &act);
    if (sigaction_get_ret && errno == EINVAL) {
#if !defined(NDEBUG)
      // Linux supports 32 real-time signals from 33 to 64.
      // If the number of signals in the Linux kernel changes, someone should
      // look at this code.
      const int kNumberOfSignals = 64;
      RAW_CHECK(signum == kNumberOfSignals + 1);
#endif  // !defined(NDEBUG)
      break;
    }
    // All other failures are fatal.
    if (sigaction_get_ret) {
      RAW_LOG(FATAL, "sigaction (get) failed.");
    }

    // The kernel won't allow to re-set SIGKILL or SIGSTOP.
    if (signum != SIGSTOP && signum != SIGKILL) {
      act.k_sa_handler = reinterpret_cast<void*>(SIG_DFL);
      act.k_sa_restorer = NULL;
      if (sys_rt_sigaction(signum, &act, NULL)) {
        RAW_LOG(FATAL, "sigaction (set) failed.");
      }
    }
#if !defined(NDEBUG)
    // Now ask the kernel again and check that no restorer will leak.
    if (sys_rt_sigaction(signum, NULL, &act) || act.k_sa_restorer) {
      RAW_LOG(FATAL, "Cound not fix sa_restorer.");
    }
#endif  // !defined(NDEBUG)
  }
}
#endif  // !defined(OS_LINUX) ||
        // (!defined(__i386__) && !defined(__x86_64__) && !defined(__arm__))

}  // anonymous namespace

// A class to handle auto-closing of DIR*'s.
class ScopedDIRClose {
 public:
  inline void operator()(DIR* x) const {
    if (x) {
      closedir(x);
    }
  }
};
typedef scoped_ptr_malloc<DIR, ScopedDIRClose> ScopedDIR;

#if defined(OS_LINUX)
static const char kFDDir[] = "/proc/self/fd";
#elif defined(OS_MACOSX)
static const char kFDDir[] = "/dev/fd";
#elif defined(OS_SOLARIS)
static const char kFDDir[] = "/dev/fd";
#elif defined(OS_FREEBSD)
static const char kFDDir[] = "/dev/fd";
#elif defined(OS_OPENBSD)
static const char kFDDir[] = "/dev/fd";
#elif defined(OS_ANDROID)
static const char kFDDir[] = "/proc/self/fd";
#endif

void CloseSuperfluousFds(const base::InjectiveMultimap& saved_mapping) {
  // DANGER: no calls to malloc are allowed from now on:
  // http://crbug.com/36678

  // Get the maximum number of FDs possible.
  size_t max_fds = GetMaxFds();

  DirReaderPosix fd_dir(kFDDir);
  if (!fd_dir.IsValid()) {
    // Fallback case: Try every possible fd.
    for (size_t i = 0; i < max_fds; ++i) {
      const int fd = static_cast<int>(i);
      if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
        continue;
      InjectiveMultimap::const_iterator j;
      for (j = saved_mapping.begin(); j != saved_mapping.end(); j++) {
        if (fd == j->dest)
          break;
      }
      if (j != saved_mapping.end())
        continue;

      // Since we're just trying to close anything we can find,
      // ignore any error return values of close().
      ignore_result(HANDLE_EINTR(close(fd)));
    }
    return;
  }

  const int dir_fd = fd_dir.fd();

  for ( ; fd_dir.Next(); ) {
    // Skip . and .. entries.
    if (fd_dir.name()[0] == '.')
      continue;

    char *endptr;
    errno = 0;
    const long int fd = strtol(fd_dir.name(), &endptr, 10);
    if (fd_dir.name()[0] == 0 || *endptr || fd < 0 || errno)
      continue;
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO)
      continue;
    InjectiveMultimap::const_iterator i;
    for (i = saved_mapping.begin(); i != saved_mapping.end(); i++) {
      if (fd == i->dest)
        break;
    }
    if (i != saved_mapping.end())
      continue;
    if (fd == dir_fd)
      continue;

    // When running under Valgrind, Valgrind opens several FDs for its
    // own use and will complain if we try to close them.  All of
    // these FDs are >= |max_fds|, so we can check against that here
    // before closing.  See https://bugs.kde.org/show_bug.cgi?id=191758
    if (fd < static_cast<int>(max_fds)) {
      int ret = HANDLE_EINTR(close(fd));
      DPCHECK(ret == 0);
    }
  }
}

char** AlterEnvironment(const EnvironmentVector& changes,
                        const char* const* const env) {
  unsigned count = 0;
  unsigned size = 0;

  // First assume that all of the current environment will be included.
  for (unsigned i = 0; env[i]; i++) {
    const char *const pair = env[i];
    count++;
    size += strlen(pair) + 1 /* terminating NUL */;
  }

  for (EnvironmentVector::const_iterator j = changes.begin();
       j != changes.end();
       ++j) {
    bool found = false;
    const char *pair;

    for (unsigned i = 0; env[i]; i++) {
      pair = env[i];
      const char *const equals = strchr(pair, '=');
      if (!equals)
        continue;
      const unsigned keylen = equals - pair;
      if (keylen == j->first.size() &&
          memcmp(pair, j->first.data(), keylen) == 0) {
        found = true;
        break;
      }
    }

    // if found, we'll either be deleting or replacing this element.
    if (found) {
      count--;
      size -= strlen(pair) + 1;
      if (j->second.size())
        found = false;
    }

    // if !found, then we have a new element to add.
    if (!found && !j->second.empty()) {
      count++;
      size += j->first.size() + 1 /* '=' */ + j->second.size() + 1 /* NUL */;
    }
  }

  count++;  // for the final NULL
  uint8_t *buffer = new uint8_t[sizeof(char*) * count + size];
  char **const ret = reinterpret_cast<char**>(buffer);
  unsigned k = 0;
  char *scratch = reinterpret_cast<char*>(buffer + sizeof(char*) * count);

  for (unsigned i = 0; env[i]; i++) {
    const char *const pair = env[i];
    const char *const equals = strchr(pair, '=');
    if (!equals) {
      const unsigned len = strlen(pair);
      ret[k++] = scratch;
      memcpy(scratch, pair, len + 1);
      scratch += len + 1;
      continue;
    }
    const unsigned keylen = equals - pair;
    bool handled = false;
    for (EnvironmentVector::const_iterator
         j = changes.begin(); j != changes.end(); j++) {
      if (j->first.size() == keylen &&
          memcmp(j->first.data(), pair, keylen) == 0) {
        if (!j->second.empty()) {
          ret[k++] = scratch;
          memcpy(scratch, pair, keylen + 1);
          scratch += keylen + 1;
          memcpy(scratch, j->second.c_str(), j->second.size() + 1);
          scratch += j->second.size() + 1;
        }
        handled = true;
        break;
      }
    }

    if (!handled) {
      const unsigned len = strlen(pair);
      ret[k++] = scratch;
      memcpy(scratch, pair, len + 1);
      scratch += len + 1;
    }
  }

  // Now handle new elements
  for (EnvironmentVector::const_iterator
       j = changes.begin(); j != changes.end(); j++) {
    if (j->second.empty())
      continue;

    bool found = false;
    for (unsigned i = 0; env[i]; i++) {
      const char *const pair = env[i];
      const char *const equals = strchr(pair, '=');
      if (!equals)
        continue;
      const unsigned keylen = equals - pair;
      if (keylen == j->first.size() &&
          memcmp(pair, j->first.data(), keylen) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      ret[k++] = scratch;
      memcpy(scratch, j->first.data(), j->first.size());
      scratch += j->first.size();
      *scratch++ = '=';
      memcpy(scratch, j->second.c_str(), j->second.size() + 1);
      scratch += j->second.size() + 1;
     }
  }

  ret[k] = NULL;
  return ret;
}

bool LaunchProcess(const std::vector<std::string>& argv,
                   const LaunchOptions& options,
                   ProcessHandle* process_handle) {
  size_t fd_shuffle_size = 0;
  if (options.fds_to_remap) {
    fd_shuffle_size = options.fds_to_remap->size();
  }

  InjectiveMultimap fd_shuffle1;
  InjectiveMultimap fd_shuffle2;
  fd_shuffle1.reserve(fd_shuffle_size);
  fd_shuffle2.reserve(fd_shuffle_size);

  scoped_ptr<char*[]> argv_cstr(new char*[argv.size() + 1]);
  scoped_ptr<char*[]> new_environ;
  if (options.environ)
    new_environ.reset(AlterEnvironment(*options.environ, GetEnvironment()));

  sigset_t full_sigset;
  sigfillset(&full_sigset);
  const sigset_t orig_sigmask = SetSignalMask(full_sigset);

  pid_t pid;
#if defined(OS_LINUX)
  if (options.clone_flags) {
    // Signal handling in this function assumes the creation of a new
    // process, so we check that a thread is not being created by mistake
    // and that signal handling follows the process-creation rules.
    RAW_CHECK(
        !(options.clone_flags & (CLONE_SIGHAND | CLONE_THREAD | CLONE_VM)));
    pid = syscall(__NR_clone, options.clone_flags, 0, 0, 0);
  } else
#endif
  {
    pid = fork();
  }

  // Always restore the original signal mask in the parent.
  if (pid != 0) {
    SetSignalMask(orig_sigmask);
  }

  if (pid < 0) {
    DPLOG(ERROR) << "fork";
    return false;
  } else if (pid == 0) {
    // Child process

    // DANGER: fork() rule: in the child, if you don't end up doing exec*(),
    // you call _exit() instead of exit(). This is because _exit() does not
    // call any previously-registered (in the parent) exit handlers, which
    // might do things like block waiting for threads that don't even exist
    // in the child.

    // If a child process uses the readline library, the process block forever.
    // In BSD like OSes including OS X it is safe to assign /dev/null as stdin.
    // See http://crbug.com/56596.
    int null_fd = HANDLE_EINTR(open("/dev/null", O_RDONLY));
    if (null_fd < 0) {
      RAW_LOG(ERROR, "Failed to open /dev/null");
      _exit(127);
    }

    file_util::ScopedFD null_fd_closer(&null_fd);
    int new_fd = HANDLE_EINTR(dup2(null_fd, STDIN_FILENO));
    if (new_fd != STDIN_FILENO) {
      RAW_LOG(ERROR, "Failed to dup /dev/null for stdin");
      _exit(127);
    }

    if (options.new_process_group) {
      // Instead of inheriting the process group ID of the parent, the child
      // starts off a new process group with pgid equal to its process ID.
      if (setpgid(0, 0) < 0) {
        RAW_LOG(ERROR, "setpgid failed");
        _exit(127);
      }
    }

    // Stop type-profiler.
    // The profiler should be stopped between fork and exec since it inserts
    // locks at new/delete expressions.  See http://crbug.com/36678.
    base::type_profiler::Controller::Stop();

    if (options.maximize_rlimits) {
      // Some resource limits need to be maximal in this child.
      std::set<int>::const_iterator resource;
      for (resource = options.maximize_rlimits->begin();
           resource != options.maximize_rlimits->end();
           ++resource) {
        struct rlimit limit;
        if (getrlimit(*resource, &limit) < 0) {
          RAW_LOG(WARNING, "getrlimit failed");
        } else if (limit.rlim_cur < limit.rlim_max) {
          limit.rlim_cur = limit.rlim_max;
          if (setrlimit(*resource, &limit) < 0) {
            RAW_LOG(WARNING, "setrlimit failed");
          }
        }
      }
    }

#if defined(OS_MACOSX)
    RestoreDefaultExceptionHandler();
#endif  // defined(OS_MACOSX)

    ResetChildSignalHandlersToDefaults();
    SetSignalMask(orig_sigmask);

#if 0
    // When debugging it can be helpful to check that we really aren't making
    // any hidden calls to malloc.
    void *malloc_thunk =
        reinterpret_cast<void*>(reinterpret_cast<intptr_t>(malloc) & ~4095);
    mprotect(malloc_thunk, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
    memset(reinterpret_cast<void*>(malloc), 0xff, 8);
#endif  // 0

    // DANGER: no calls to malloc are allowed from now on:
    // http://crbug.com/36678

#if defined(OS_CHROMEOS)
    if (options.ctrl_terminal_fd >= 0) {
      // Set process' controlling terminal.
      if (HANDLE_EINTR(setsid()) != -1) {
        if (HANDLE_EINTR(
                ioctl(options.ctrl_terminal_fd, TIOCSCTTY, NULL)) == -1) {
          RAW_LOG(WARNING, "ioctl(TIOCSCTTY), ctrl terminal not set");
        }
      } else {
        RAW_LOG(WARNING, "setsid failed, ctrl terminal not set");
      }
    }
#endif  // defined(OS_CHROMEOS)

    if (options.fds_to_remap) {
      for (FileHandleMappingVector::const_iterator
               it = options.fds_to_remap->begin();
           it != options.fds_to_remap->end(); ++it) {
        fd_shuffle1.push_back(InjectionArc(it->first, it->second, false));
        fd_shuffle2.push_back(InjectionArc(it->first, it->second, false));
      }
    }

    if (options.environ)
      SetEnvironment(new_environ.get());

    // fd_shuffle1 is mutated by this call because it cannot malloc.
    if (!ShuffleFileDescriptors(&fd_shuffle1))
      _exit(127);

    CloseSuperfluousFds(fd_shuffle2);

    for (size_t i = 0; i < argv.size(); i++)
      argv_cstr[i] = const_cast<char*>(argv[i].c_str());
    argv_cstr[argv.size()] = NULL;
    execvp(argv_cstr[0], argv_cstr.get());

    RAW_LOG(ERROR, "LaunchProcess: failed to execvp:");
    RAW_LOG(ERROR, argv_cstr[0]);
    _exit(127);
  } else {
    // Parent process
    if (options.wait) {
      // While this isn't strictly disk IO, waiting for another process to
      // finish is the sort of thing ThreadRestrictions is trying to prevent.
      base::ThreadRestrictions::AssertIOAllowed();
      pid_t ret = HANDLE_EINTR(waitpid(pid, 0, 0));
      DPCHECK(ret > 0);
    }

    if (process_handle)
      *process_handle = pid;
  }

  return true;
}


bool LaunchProcess(const CommandLine& cmdline,
                   const LaunchOptions& options,
                   ProcessHandle* process_handle) {
  return LaunchProcess(cmdline.argv(), options, process_handle);
}

void RaiseProcessToHighPriority() {
  // On POSIX, we don't actually do anything here.  We could try to nice() or
  // setpriority() or sched_getscheduler, but these all require extra rights.
}

// Return value used by GetAppOutputInternal to encapsulate the various exit
// scenarios from the function.
enum GetAppOutputInternalResult {
  EXECUTE_FAILURE,
  EXECUTE_SUCCESS,
  GOT_MAX_OUTPUT,
};

// Executes the application specified by |argv| and wait for it to exit. Stores
// the output (stdout) in |output|. If |do_search_path| is set, it searches the
// path for the application; in that case, |envp| must be null, and it will use
// the current environment. If |do_search_path| is false, |argv[0]| should fully
// specify the path of the application, and |envp| will be used as the
// environment. Redirects stderr to /dev/null.
// If we successfully start the application and get all requested output, we
// return GOT_MAX_OUTPUT, or if there is a problem starting or exiting
// the application we return RUN_FAILURE. Otherwise we return EXECUTE_SUCCESS.
// The GOT_MAX_OUTPUT return value exists so a caller that asks for limited
// output can treat this as a success, despite having an exit code of SIG_PIPE
// due to us closing the output pipe.
// In the case of EXECUTE_SUCCESS, the application exit code will be returned
// in |*exit_code|, which should be checked to determine if the application
// ran successfully.
static GetAppOutputInternalResult GetAppOutputInternal(
    const std::vector<std::string>& argv,
    char* const envp[],
    std::string* output,
    size_t max_output,
    bool do_search_path,
    int* exit_code) {
  // Doing a blocking wait for another command to finish counts as IO.
  base::ThreadRestrictions::AssertIOAllowed();
  // exit_code must be supplied so calling function can determine success.
  DCHECK(exit_code);
  *exit_code = EXIT_FAILURE;

  int pipe_fd[2];
  pid_t pid;
  InjectiveMultimap fd_shuffle1, fd_shuffle2;
  scoped_ptr<char*[]> argv_cstr(new char*[argv.size() + 1]);

  fd_shuffle1.reserve(3);
  fd_shuffle2.reserve(3);

  // Either |do_search_path| should be false or |envp| should be null, but not
  // both.
  DCHECK(!do_search_path ^ !envp);

  if (pipe(pipe_fd) < 0)
    return EXECUTE_FAILURE;

  switch (pid = fork()) {
    case -1:  // error
      close(pipe_fd[0]);
      close(pipe_fd[1]);
      return EXECUTE_FAILURE;
    case 0:  // child
      {
#if defined(OS_MACOSX)
        RestoreDefaultExceptionHandler();
#endif
        // DANGER: no calls to malloc are allowed from now on:
        // http://crbug.com/36678

        // Obscure fork() rule: in the child, if you don't end up doing exec*(),
        // you call _exit() instead of exit(). This is because _exit() does not
        // call any previously-registered (in the parent) exit handlers, which
        // might do things like block waiting for threads that don't even exist
        // in the child.
        int dev_null = open("/dev/null", O_WRONLY);
        if (dev_null < 0)
          _exit(127);

        // Stop type-profiler.
        // The profiler should be stopped between fork and exec since it inserts
        // locks at new/delete expressions.  See http://crbug.com/36678.
        base::type_profiler::Controller::Stop();

        fd_shuffle1.push_back(InjectionArc(pipe_fd[1], STDOUT_FILENO, true));
        fd_shuffle1.push_back(InjectionArc(dev_null, STDERR_FILENO, true));
        fd_shuffle1.push_back(InjectionArc(dev_null, STDIN_FILENO, true));
        // Adding another element here? Remeber to increase the argument to
        // reserve(), above.

        std::copy(fd_shuffle1.begin(), fd_shuffle1.end(),
                  std::back_inserter(fd_shuffle2));

        if (!ShuffleFileDescriptors(&fd_shuffle1))
          _exit(127);

        CloseSuperfluousFds(fd_shuffle2);

        for (size_t i = 0; i < argv.size(); i++)
          argv_cstr[i] = const_cast<char*>(argv[i].c_str());
        argv_cstr[argv.size()] = NULL;
        if (do_search_path)
          execvp(argv_cstr[0], argv_cstr.get());
        else
          execve(argv_cstr[0], argv_cstr.get(), envp);
        _exit(127);
      }
    default:  // parent
      {
        // Close our writing end of pipe now. Otherwise later read would not
        // be able to detect end of child's output (in theory we could still
        // write to the pipe).
        close(pipe_fd[1]);

        output->clear();
        char buffer[256];
        size_t output_buf_left = max_output;
        ssize_t bytes_read = 1;  // A lie to properly handle |max_output == 0|
                                 // case in the logic below.

        while (output_buf_left > 0) {
          bytes_read = HANDLE_EINTR(read(pipe_fd[0], buffer,
                                    std::min(output_buf_left, sizeof(buffer))));
          if (bytes_read <= 0)
            break;
          output->append(buffer, bytes_read);
          output_buf_left -= static_cast<size_t>(bytes_read);
        }
        close(pipe_fd[0]);

        // Always wait for exit code (even if we know we'll declare
        // GOT_MAX_OUTPUT).
        bool success = WaitForExitCode(pid, exit_code);

        // If we stopped because we read as much as we wanted, we return
        // GOT_MAX_OUTPUT (because the child may exit due to |SIGPIPE|).
        if (!output_buf_left && bytes_read > 0)
          return GOT_MAX_OUTPUT;
        else if (success)
          return EXECUTE_SUCCESS;
        return EXECUTE_FAILURE;
      }
  }
}

bool GetAppOutput(const CommandLine& cl, std::string* output) {
  return GetAppOutput(cl.argv(), output);
}

bool GetAppOutput(const std::vector<std::string>& argv, std::string* output) {
  // Run |execve()| with the current environment and store "unlimited" data.
  int exit_code;
  GetAppOutputInternalResult result = GetAppOutputInternal(
      argv, NULL, output, std::numeric_limits<std::size_t>::max(), true,
      &exit_code);
  return result == EXECUTE_SUCCESS && exit_code == EXIT_SUCCESS;
}

// TODO(viettrungluu): Conceivably, we should have a timeout as well, so we
// don't hang if what we're calling hangs.
bool GetAppOutputRestricted(const CommandLine& cl,
                            std::string* output, size_t max_output) {
  // Run |execve()| with the empty environment.
  char* const empty_environ = NULL;
  int exit_code;
  GetAppOutputInternalResult result = GetAppOutputInternal(
      cl.argv(), &empty_environ, output, max_output, false, &exit_code);
  return result == GOT_MAX_OUTPUT || (result == EXECUTE_SUCCESS &&
                                      exit_code == EXIT_SUCCESS);
}

bool GetAppOutputWithExitCode(const CommandLine& cl,
                              std::string* output,
                              int* exit_code) {
  // Run |execve()| with the current environment and store "unlimited" data.
  GetAppOutputInternalResult result = GetAppOutputInternal(
      cl.argv(), NULL, output, std::numeric_limits<std::size_t>::max(), true,
      exit_code);
  return result == EXECUTE_SUCCESS;
}

}  // namespace base
