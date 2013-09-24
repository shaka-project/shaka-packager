// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains functions for launching subprocesses.

#ifndef BASE_PROCESS_LAUNCH_H_
#define BASE_PROCESS_LAUNCH_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/process/process_handle.h"

#if defined(OS_POSIX)
#include "base/posix/file_descriptor_shuffle.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

class CommandLine;

namespace base {

typedef std::vector<std::pair<std::string, std::string> > EnvironmentVector;
typedef std::vector<std::pair<int, int> > FileHandleMappingVector;

// Options for launching a subprocess that are passed to LaunchProcess().
// The default constructor constructs the object with default options.
struct LaunchOptions {
  LaunchOptions()
      : wait(false),
#if defined(OS_WIN)
        start_hidden(false),
        inherit_handles(false),
        as_user(NULL),
        empty_desktop_name(false),
        job_handle(NULL),
        stdin_handle(NULL),
        stdout_handle(NULL),
        stderr_handle(NULL),
        force_breakaway_from_job_(false)
#else
        environ(NULL),
        fds_to_remap(NULL),
        maximize_rlimits(NULL),
        new_process_group(false)
#if defined(OS_LINUX)
        , clone_flags(0)
#endif  // OS_LINUX
#if defined(OS_CHROMEOS)
        , ctrl_terminal_fd(-1)
#endif  // OS_CHROMEOS
#endif  // !defined(OS_WIN)
        {}

  // If true, wait for the process to complete.
  bool wait;

#if defined(OS_WIN)
  bool start_hidden;

  // If true, the new process inherits handles from the parent. In production
  // code this flag should be used only when running short-lived, trusted
  // binaries, because open handles from other libraries and subsystems will
  // leak to the child process, causing errors such as open socket hangs.
  bool inherit_handles;

  // If non-NULL, runs as if the user represented by the token had launched it.
  // Whether the application is visible on the interactive desktop depends on
  // the token belonging to an interactive logon session.
  //
  // To avoid hard to diagnose problems, when specified this loads the
  // environment variables associated with the user and if this operation fails
  // the entire call fails as well.
  UserTokenHandle as_user;

  // If true, use an empty string for the desktop name.
  bool empty_desktop_name;

  // If non-NULL, launches the application in that job object. The process will
  // be terminated immediately and LaunchProcess() will fail if assignment to
  // the job object fails.
  HANDLE job_handle;

  // Handles for the redirection of stdin, stdout and stderr. The handles must
  // be inheritable. Caller should either set all three of them or none (i.e.
  // there is no way to redirect stderr without redirecting stdin). The
  // |inherit_handles| flag must be set to true when redirecting stdio stream.
  HANDLE stdin_handle;
  HANDLE stdout_handle;
  HANDLE stderr_handle;

  // If set to true, ensures that the child process is launched with the
  // CREATE_BREAKAWAY_FROM_JOB flag which allows it to breakout of the parent
  // job if any.
  bool force_breakaway_from_job_;
#else
  // If non-NULL, set/unset environment variables.
  // See documentation of AlterEnvironment().
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const EnvironmentVector* environ;

  // If non-NULL, remap file descriptors according to the mapping of
  // src fd->dest fd to propagate FDs into the child process.
  // This pointer is owned by the caller and must live through the
  // call to LaunchProcess().
  const FileHandleMappingVector* fds_to_remap;

  // Each element is an RLIMIT_* constant that should be raised to its
  // rlim_max.  This pointer is owned by the caller and must live through
  // the call to LaunchProcess().
  const std::set<int>* maximize_rlimits;

  // If true, start the process in a new process group, instead of
  // inheriting the parent's process group.  The pgid of the child process
  // will be the same as its pid.
  bool new_process_group;

#if defined(OS_LINUX)
  // If non-zero, start the process using clone(), using flags as provided.
  int clone_flags;
#endif  // defined(OS_LINUX)

#if defined(OS_CHROMEOS)
  // If non-negative, the specified file descriptor will be set as the launched
  // process' controlling terminal.
  int ctrl_terminal_fd;
#endif  // defined(OS_CHROMEOS)

#endif  // !defined(OS_WIN)
};

// Launch a process via the command line |cmdline|.
// See the documentation of LaunchOptions for details on |options|.
//
// Returns true upon success.
//
// Upon success, if |process_handle| is non-NULL, it will be filled in with the
// handle of the launched process.  NOTE: In this case, the caller is
// responsible for closing the handle so that it doesn't leak!
// Otherwise, the process handle will be implicitly closed.
//
// Unix-specific notes:
// - All file descriptors open in the parent process will be closed in the
//   child process except for any preserved by options::fds_to_remap, and
//   stdin, stdout, and stderr. If not remapped by options::fds_to_remap,
//   stdin is reopened as /dev/null, and the child is allowed to inherit its
//   parent's stdout and stderr.
// - If the first argument on the command line does not contain a slash,
//   PATH will be searched.  (See man execvp.)
BASE_EXPORT bool LaunchProcess(const CommandLine& cmdline,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

#if defined(OS_WIN)
// Windows-specific LaunchProcess that takes the command line as a
// string.  Useful for situations where you need to control the
// command line arguments directly, but prefer the CommandLine version
// if launching Chrome itself.
//
// The first command line argument should be the path to the process,
// and don't forget to quote it.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
BASE_EXPORT bool LaunchProcess(const string16& cmdline,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

#elif defined(OS_POSIX)
// A POSIX-specific version of LaunchProcess that takes an argv array
// instead of a CommandLine.  Useful for situations where you need to
// control the command line arguments directly, but prefer the
// CommandLine version if launching Chrome itself.
BASE_EXPORT bool LaunchProcess(const std::vector<std::string>& argv,
                               const LaunchOptions& options,
                               ProcessHandle* process_handle);

// AlterEnvironment returns a modified environment vector, constructed from the
// given environment and the list of changes given in |changes|. Each key in
// the environment is matched against the first element of the pairs. In the
// event of a match, the value is replaced by the second of the pair, unless
// the second is empty, in which case the key-value is removed.
//
// The returned array is allocated using new[] and must be freed by the caller.
BASE_EXPORT char** AlterEnvironment(const EnvironmentVector& changes,
                                    const char* const* const env);

// Close all file descriptors, except those which are a destination in the
// given multimap. Only call this function in a child process where you know
// that there aren't any other threads.
BASE_EXPORT void CloseSuperfluousFds(const InjectiveMultimap& saved_map);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Set JOBOBJECT_EXTENDED_LIMIT_INFORMATION to JobObject |job_object|.
// As its limit_info.BasicLimitInformation.LimitFlags has
// JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
// When the provide JobObject |job_object| is closed, the binded process will
// be terminated.
BASE_EXPORT bool SetJobObjectAsKillOnJobClose(HANDLE job_object);

// Output multi-process printf, cout, cerr, etc to the cmd.exe console that ran
// chrome. This is not thread-safe: only call from main thread.
BASE_EXPORT void RouteStdioToConsole();
#endif  // defined(OS_WIN)

// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. Redirects stderr to /dev/null. Returns true
// on success (application launched and exited cleanly, with exit code
// indicating success).
BASE_EXPORT bool GetAppOutput(const CommandLine& cl, std::string* output);

#if defined(OS_POSIX)
// A POSIX-specific version of GetAppOutput that takes an argv array
// instead of a CommandLine.  Useful for situations where you need to
// control the command line arguments directly.
BASE_EXPORT bool GetAppOutput(const std::vector<std::string>& argv,
                              std::string* output);

// A restricted version of |GetAppOutput()| which (a) clears the environment,
// and (b) stores at most |max_output| bytes; also, it doesn't search the path
// for the command.
BASE_EXPORT bool GetAppOutputRestricted(const CommandLine& cl,
                                        std::string* output, size_t max_output);

// A version of |GetAppOutput()| which also returns the exit code of the
// executed command. Returns true if the application runs and exits cleanly. If
// this is the case the exit code of the application is available in
// |*exit_code|.
BASE_EXPORT bool GetAppOutputWithExitCode(const CommandLine& cl,
                                          std::string* output, int* exit_code);
#endif  // defined(OS_POSIX)

// If supported on the platform, and the user has sufficent rights, increase
// the current process's scheduling priority to a high priority.
BASE_EXPORT void RaiseProcessToHighPriority();

#if defined(OS_MACOSX)
// Restore the default exception handler, setting it to Apple Crash Reporter
// (ReportCrash).  When forking and execing a new process, the child will
// inherit the parent's exception ports, which may be set to the Breakpad
// instance running inside the parent.  The parent's Breakpad instance should
// not handle the child's exceptions.  Calling RestoreDefaultExceptionHandler
// in the child after forking will restore the standard exception handler.
// See http://crbug.com/20371/ for more details.
void RestoreDefaultExceptionHandler();
#endif  // defined(OS_MACOSX)

}  // namespace base

#endif  // BASE_PROCESS_LAUNCH_H_
