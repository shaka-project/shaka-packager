// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/kill.h"

#include <io.h>
#include <windows.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_iterator.h"
#include "base/win/object_watcher.h"

namespace base {

namespace {

// Exit codes with special meanings on Windows.
const DWORD kNormalTerminationExitCode = 0;
const DWORD kDebuggerInactiveExitCode = 0xC0000354;
const DWORD kKeyboardInterruptExitCode = 0xC000013A;
const DWORD kDebuggerTerminatedExitCode = 0x40010004;

// This exit code is used by the Windows task manager when it kills a
// process.  It's value is obviously not that unique, and it's
// surprising to me that the task manager uses this value, but it
// seems to be common practice on Windows to test for it as an
// indication that the task manager has killed something if the
// process goes away.
const DWORD kProcessKilledExitCode = 1;

// Maximum amount of time (in milliseconds) to wait for the process to exit.
static const int kWaitInterval = 2000;

class TimerExpiredTask : public win::ObjectWatcher::Delegate {
 public:
  explicit TimerExpiredTask(ProcessHandle process);
  ~TimerExpiredTask();

  void TimedOut();

  // MessageLoop::Watcher -----------------------------------------------------
  virtual void OnObjectSignaled(HANDLE object);

 private:
  void KillProcess();

  // The process that we are watching.
  ProcessHandle process_;

  win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(TimerExpiredTask);
};

TimerExpiredTask::TimerExpiredTask(ProcessHandle process) : process_(process) {
  watcher_.StartWatching(process_, this);
}

TimerExpiredTask::~TimerExpiredTask() {
  TimedOut();
  DCHECK(!process_) << "Make sure to close the handle.";
}

void TimerExpiredTask::TimedOut() {
  if (process_)
    KillProcess();
}

void TimerExpiredTask::OnObjectSignaled(HANDLE object) {
  CloseHandle(process_);
  process_ = NULL;
}

void TimerExpiredTask::KillProcess() {
  // Stop watching the process handle since we're killing it.
  watcher_.StopWatching();

  // OK, time to get frisky.  We don't actually care when the process
  // terminates.  We just care that it eventually terminates, and that's what
  // TerminateProcess should do for us. Don't check for the result code since
  // it fails quite often. This should be investigated eventually.
  base::KillProcess(process_, kProcessKilledExitCode, false);

  // Now, just cleanup as if the process exited normally.
  OnObjectSignaled(process_);
}

}  // namespace

bool KillProcess(ProcessHandle process, int exit_code, bool wait) {
  bool result = (TerminateProcess(process, exit_code) != FALSE);
  if (result && wait) {
    // The process may not end immediately due to pending I/O
    if (WAIT_OBJECT_0 != WaitForSingleObject(process, 60 * 1000))
      DLOG_GETLASTERROR(ERROR) << "Error waiting for process exit";
  } else if (!result) {
    DLOG_GETLASTERROR(ERROR) << "Unable to terminate process";
  }
  return result;
}

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code.
// Returns true if this is successful, false otherwise.
bool KillProcessById(ProcessId process_id, int exit_code, bool wait) {
  HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE,
                               FALSE,  // Don't inherit handle
                               process_id);
  if (!process) {
    DLOG_GETLASTERROR(ERROR) << "Unable to open process " << process_id;
    return false;
  }
  bool ret = KillProcess(process, exit_code, wait);
  CloseHandle(process);
  return ret;
}

TerminationStatus GetTerminationStatus(ProcessHandle handle, int* exit_code) {
  DWORD tmp_exit_code = 0;

  if (!::GetExitCodeProcess(handle, &tmp_exit_code)) {
    DLOG_GETLASTERROR(FATAL) << "GetExitCodeProcess() failed";
    if (exit_code) {
      // This really is a random number.  We haven't received any
      // information about the exit code, presumably because this
      // process doesn't have permission to get the exit code, or
      // because of some other cause for GetExitCodeProcess to fail
      // (MSDN docs don't give the possible failure error codes for
      // this function, so it could be anything).  But we don't want
      // to leave exit_code uninitialized, since that could cause
      // random interpretations of the exit code.  So we assume it
      // terminated "normally" in this case.
      *exit_code = kNormalTerminationExitCode;
    }
    // Assume the child has exited normally if we can't get the exit
    // code.
    return TERMINATION_STATUS_NORMAL_TERMINATION;
  }
  if (tmp_exit_code == STILL_ACTIVE) {
    DWORD wait_result = WaitForSingleObject(handle, 0);
    if (wait_result == WAIT_TIMEOUT) {
      if (exit_code)
        *exit_code = wait_result;
      return TERMINATION_STATUS_STILL_RUNNING;
    }

    if (wait_result == WAIT_FAILED) {
      DLOG_GETLASTERROR(ERROR) << "WaitForSingleObject() failed";
    } else {
      DCHECK_EQ(WAIT_OBJECT_0, wait_result);

      // Strange, the process used 0x103 (STILL_ACTIVE) as exit code.
      NOTREACHED();
    }

    return TERMINATION_STATUS_ABNORMAL_TERMINATION;
  }

  if (exit_code)
    *exit_code = tmp_exit_code;

  switch (tmp_exit_code) {
    case kNormalTerminationExitCode:
      return TERMINATION_STATUS_NORMAL_TERMINATION;
    case kDebuggerInactiveExitCode:  // STATUS_DEBUGGER_INACTIVE.
    case kKeyboardInterruptExitCode:  // Control-C/end session.
    case kDebuggerTerminatedExitCode:  // Debugger terminated process.
    case kProcessKilledExitCode:  // Task manager kill.
      return TERMINATION_STATUS_PROCESS_WAS_KILLED;
    default:
      // All other exit codes indicate crashes.
      return TERMINATION_STATUS_PROCESS_CRASHED;
  }
}

bool WaitForExitCode(ProcessHandle handle, int* exit_code) {
  bool success = WaitForExitCodeWithTimeout(
      handle, exit_code, base::TimeDelta::FromMilliseconds(INFINITE));
  CloseProcessHandle(handle);
  return success;
}

bool WaitForExitCodeWithTimeout(ProcessHandle handle,
                                int* exit_code,
                                base::TimeDelta timeout) {
  if (::WaitForSingleObject(handle, timeout.InMilliseconds()) != WAIT_OBJECT_0)
    return false;
  DWORD temp_code;  // Don't clobber out-parameters in case of failure.
  if (!::GetExitCodeProcess(handle, &temp_code))
    return false;

  *exit_code = temp_code;
  return true;
}

bool WaitForProcessesToExit(const FilePath::StringType& executable_name,
                            base::TimeDelta wait,
                            const ProcessFilter* filter) {
  const ProcessEntry* entry;
  bool result = true;
  DWORD start_time = GetTickCount();

  NamedProcessIterator iter(executable_name, filter);
  while ((entry = iter.NextProcessEntry())) {
    DWORD remaining_wait = std::max<int64>(
        0, wait.InMilliseconds() - (GetTickCount() - start_time));
    HANDLE process = OpenProcess(SYNCHRONIZE,
                                 FALSE,
                                 entry->th32ProcessID);
    DWORD wait_result = WaitForSingleObject(process, remaining_wait);
    CloseHandle(process);
    result = result && (wait_result == WAIT_OBJECT_0);
  }

  return result;
}

bool WaitForSingleProcess(ProcessHandle handle, base::TimeDelta wait) {
  int exit_code;
  if (!WaitForExitCodeWithTimeout(handle, &exit_code, wait))
    return false;
  return exit_code == 0;
}

bool CleanupProcesses(const FilePath::StringType& executable_name,
                      base::TimeDelta wait,
                      int exit_code,
                      const ProcessFilter* filter) {
  bool exited_cleanly = WaitForProcessesToExit(executable_name, wait, filter);
  if (!exited_cleanly)
    KillProcesses(executable_name, exit_code, filter);
  return exited_cleanly;
}

void EnsureProcessTerminated(ProcessHandle process) {
  DCHECK(process != GetCurrentProcess());

  // If already signaled, then we are done!
  if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
    CloseHandle(process);
    return;
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TimerExpiredTask::TimedOut,
                 base::Owned(new TimerExpiredTask(process))),
      base::TimeDelta::FromMilliseconds(kWaitInterval));
}

}  // namespace base
