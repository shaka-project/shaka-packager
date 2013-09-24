// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tool to log the execution of the process (Chrome). Writes logs containing
// time and address of the callback being called for the first time.
//
// To speed up the logging, buffering logs is implemented. Every thread have its
// own buffer and log file so the contention between threads is minimal. As a
// side-effect, functions called might be mentioned in many thread logs.
//
// Special thread is created in the process to periodically flushes logs for all
// threads for the case the thread has stopped before flushing its logs.
//
// Use this profiler with linux_use_tcmalloc=0.
//
// Note for the ChromeOS Chrome. Remove renderer process from the sandbox (add
// --no-sandbox option to running Chrome in /sbin/session_manager_setup.sh).
// Otherwise renderer will not be able to write logs (and will assert on that).
//
// Also note that the instrumentation code is self-activated. It begins to
// record the log data when it is called first, including the run-time startup.
// Have it in mind when modifying it, in particular do not use global objects
// with constructors as they are called during startup (too late for us).

#include <fcntl.h>
#include <fstream>
#include <pthread.h>
#include <stdarg.h>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"

namespace cygprofile {

extern "C" {

// Note that these are linked internally by the compiler. Don't call
// them directly!
void __cyg_profile_func_enter(void* this_fn, void* call_site)
    __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void* this_fn, void* call_site)
    __attribute__((no_instrument_function));

}

// Single log entry layout.
struct CygLogEntry {
  time_t seconds;
  long int usec;
  pid_t pid;
  pthread_t tid;
  const void* this_fn;
  CygLogEntry(time_t seconds, long int usec,
              pid_t pid, pthread_t tid, const void* this_fn)
      : seconds(seconds), usec(usec),
        pid(pid), tid(tid), this_fn(this_fn) {}
};

// Common data for the process. Singleton.
class CygCommon {
 public:
  static CygCommon* GetInstance();
  std::string header() const { return header_line_; }
 private:
  CygCommon();
  std::string header_line_;
  friend struct DefaultSingletonTraits<CygCommon>;

  DISALLOW_COPY_AND_ASSIGN(CygCommon);
};

// Returns light-weight process ID.  On linux, this is a system-wide
// unique thread id.
static pid_t GetLwp() {
  return syscall(__NR_gettid);
}

// A per-thread structure representing the log itself.
class CygTlsLog {
 public:
  CygTlsLog()
      : in_use_(false), lwp_(GetLwp()), pthread_self_(pthread_self()) { }

  // Enter a log entity.
  void LogEnter(void* this_fn);

  // Add newly created CygTlsLog object to the list of all such objects.
  // Needed for the timer callback: it will enumerate each object and flush.
  static void AddNewLog(CygTlsLog* newlog);

  // Starts a thread in this process that periodically flushes all the threads.
  // Must be called once per process.
  static void StartFlushLogThread();

 private:
  static const int kBufMaxSize;
  static const char kLogFilenameFmt[];
  static const char kLogFileNamePrefix[];

  // Flush the log to file. Create file if needed.
  // Must be called with locked log_mutex_.
  void FlushLog();

  // Fork hooks. Needed to keep data in consistent state during fork().
  static void AtForkPrepare();
  static void AtForkParent();
  static void AtForkChild();

  // Thread callback to flush all logs periodically.
  static void* FlushLogThread(void*);

  std::string log_filename_;
  std::vector<CygLogEntry> buf_;

  // A lock that guards buf_ usage between per-thread instrumentation
  // routine and timer flush callback. So the contention could happen
  // only during the flush, every 30 secs.
  base::Lock log_mutex_;

  // Current thread is inside the instrumentation routine.
  bool in_use_;

  // Keeps track of all functions that have been logged on this thread
  // so we do not record dublicates.
  std::hash_set<void*> functions_called_;

  // Thread identifier as Linux kernel shows it. For debugging purposes.
  // LWP (light-weight process) is a unique ID of the thread in the system,
  // unlike pthread_self() which is the same for fork()-ed threads.
  pid_t lwp_;
  pthread_t pthread_self_;

  DISALLOW_COPY_AND_ASSIGN(CygTlsLog);
};

// Storage for logs for all threads in the process.
// Using std::list may be better, but it fails when used before main().
struct AllLogs {
  std::vector<CygTlsLog*> logs;
  base::Lock mutex;
};

base::LazyInstance<AllLogs>::Leaky all_logs_ = LAZY_INSTANCE_INITIALIZER;

// Per-thread pointer to the current log object.
static __thread CygTlsLog* tls_current_log = NULL;

// Magic value of above to prevent the instrumentation. Used when CygTlsLog is
// being constructed (to prevent reentering by malloc, for example) and by
// the FlushLogThread (to prevent it being logged - see comment in its code).
CygTlsLog* const kMagicBeingConstructed = reinterpret_cast<CygTlsLog*>(1);

// Number of entries in the per-thread log buffer before we flush.
// Note, that we also flush by timer so not all thread logs may grow up to this.
const int CygTlsLog::kBufMaxSize = 3000;

#if defined(OS_ANDROID)
const char CygTlsLog::kLogFileNamePrefix[] =
    "/data/local/tmp/chrome/cyglog/";
#else
const char CygTlsLog::kLogFileNamePrefix[] = "/var/log/chrome/";
#endif

// "cyglog.PID.LWP.pthread_self.PPID"
const char CygTlsLog::kLogFilenameFmt[] = "%scyglog.%d.%d.%ld-%d";

CygCommon* CygCommon::GetInstance() {
  return Singleton<CygCommon>::get();
}

CygCommon::CygCommon() {
  // Determine our module addresses.
  std::ifstream mapsfile("/proc/self/maps");
  CHECK(mapsfile.good());
  static const int kMaxLineSize = 512;
  char line[kMaxLineSize];
  void (*this_fn)(void) =
      reinterpret_cast<void(*)()>(__cyg_profile_func_enter);
  while (mapsfile.getline(line, kMaxLineSize)) {
    const std::string str_line = line;
    size_t permindex = str_line.find("r-xp");
    if (permindex != std::string::npos) {
      int dashindex = str_line.find("-");
      int spaceindex = str_line.find(" ");
      char* p;
      void* start = reinterpret_cast<void*>
          (strtol((str_line.substr(0, dashindex)).c_str(),
                  &p, 16));
      CHECK(*p == 0);  // Could not determine start address.
      void* end = reinterpret_cast<void*>
          (strtol((str_line.substr(dashindex + 1,
                                   spaceindex - dashindex - 1)).c_str(),
                  &p, 16));
      CHECK(*p == 0);  // Could not determine end address.

      if (this_fn >= start && this_fn < end)
        header_line_ = str_line;
    }
  }
  mapsfile.close();
  header_line_.append("\nsecs\tmsecs\tpid:threadid\tfunc\n");
}

void CygTlsLog::LogEnter(void* this_fn) {
  if (in_use_)
    return;
  in_use_ = true;

  if (functions_called_.find(this_fn) ==
      functions_called_.end()) {
    functions_called_.insert(this_fn);
    base::AutoLock lock(log_mutex_);
    if (buf_.capacity() < kBufMaxSize)
      buf_.reserve(kBufMaxSize);
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);
    buf_.push_back(CygLogEntry(time(NULL), timestamp.tv_usec,
                               getpid(), pthread_self(), this_fn));
    if (buf_.size() == kBufMaxSize) {
      FlushLog();
    }
  }

  in_use_ = false;
}

void CygTlsLog::AtForkPrepare() {
  CHECK(tls_current_log);
  CHECK(tls_current_log->lwp_ == GetLwp());
  CHECK(tls_current_log->pthread_self_ == pthread_self());
  all_logs_.Get().mutex.Acquire();
}

void CygTlsLog::AtForkParent() {
  CHECK(tls_current_log);
  CHECK(tls_current_log->lwp_ == GetLwp());
  CHECK(tls_current_log->pthread_self_ == pthread_self());
  all_logs_.Get().mutex.Release();
}

void CygTlsLog::AtForkChild() {
  CHECK(tls_current_log);

  // Update the IDs of this new thread of the new process.
  // Note that the process may (and Chrome main process forks zygote this way)
  // call exec(self) after we return (to launch new shiny self). If done like
  // that, PID and LWP will remain the same, but pthread_self() changes.
  pid_t lwp = GetLwp();
  CHECK(tls_current_log->lwp_ != lwp);  // LWP is system-wide unique thread ID.
  tls_current_log->lwp_ = lwp;

  CHECK(tls_current_log->pthread_self_ == pthread_self());

  // Leave the only current thread tls object because fork() clones only the
  // current thread (the one that called fork) to the child process.
  AllLogs& all_logs = all_logs_.Get();
  all_logs.logs.clear();
  all_logs.logs.push_back(tls_current_log);
  CHECK(all_logs.logs.size() == 1);

  // Clear log filename so it will be re-calculated with the new PIDs.
  tls_current_log->log_filename_.clear();

  // Create the thread that will periodically flush all logs for this process.
  StartFlushLogThread();

  // We do not update log header line (CygCommon data) as it will be the same
  // because the new process is just a forked copy.
  all_logs.mutex.Release();
}

void CygTlsLog::StartFlushLogThread() {
  pthread_t tid;
  CHECK(!pthread_create(&tid, NULL, &CygTlsLog::FlushLogThread, NULL));
}

void CygTlsLog::AddNewLog(CygTlsLog* newlog) {
  CHECK(tls_current_log == kMagicBeingConstructed);
  AllLogs& all_logs = all_logs_.Get();
  base::AutoLock lock(all_logs.mutex);
  if (all_logs.logs.empty()) {

    // An Android app never fork, it always starts with a pre-defined number of
    // process descibed by the android manifest file. In fact, there is not
    // support for pthread_atfork at the android system libraries.  All chrome
    // for android processes will start as independent processs and each one
    // will generate its own logs that will later have to be merged as usual.
#if !defined(OS_ANDROID)
    CHECK(!pthread_atfork(CygTlsLog::AtForkPrepare,
                          CygTlsLog::AtForkParent,
                          CygTlsLog::AtForkChild));
#endif

    // The very first process starts its flush thread here. Forked processes
    // will do it in AtForkChild().
    StartFlushLogThread();
  }
  all_logs.logs.push_back(newlog);
}

// Printf-style routine to write to open file.
static void WriteLogLine(int fd, const char* fmt, ...) {
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  char msg[160];
  int len = vsnprintf(msg, sizeof(msg), fmt, arg_ptr);
  int rc = write(fd, msg, (len > sizeof(msg))? sizeof(msg): len);
  va_end(arg_ptr);
};

void CygTlsLog::FlushLog() {
  bool first_log_write = false;
  if (log_filename_.empty()) {
    first_log_write = true;
    char buf[80];
    snprintf(buf, sizeof(buf), kLogFilenameFmt,
             kLogFileNamePrefix, getpid(), lwp_, pthread_self_, getppid());
    log_filename_ = buf;
    unlink(log_filename_.c_str());
  }

  int file = open(log_filename_.c_str(), O_CREAT | O_WRONLY | O_APPEND, 00600);
  CHECK(file != -1);

  if (first_log_write)
    WriteLogLine(file, "%s", CygCommon::GetInstance()->header().c_str());

  for (int i = 0; i != buf_.size(); ++i) {
    const CygLogEntry& p = buf_[i];
    WriteLogLine(file, "%ld %ld\t%d:%ld\t%p\n",
                 p.seconds, p.usec, p.pid, p.tid, p.this_fn);
  }

  close(file);
  buf_.clear();
}

void* CygTlsLog::FlushLogThread(void*) {
  // Disable logging this thread.  Although this routine is not instrumented
  // (cygprofile.gyp provides that), the called routines are and thus will
  // call instrumentation.
  CHECK(tls_current_log == NULL);  // Must be 0 as this is a new thread.
  tls_current_log = kMagicBeingConstructed;

  // Run this loop infinitely: sleep 30 secs and the flush all thread's
  // buffers.  There is a danger that, when quitting Chrome, this thread may
  // see unallocated data and segfault. We do not care because we need logs
  // when Chrome is working.
  while (true) {
    for(int secs_to_sleep = 30; secs_to_sleep != 0;)
      secs_to_sleep = sleep(secs_to_sleep);

    AllLogs& all_logs = all_logs_.Get();
    base::AutoLock lock(all_logs.mutex);
    for (int i = 0; i != all_logs.logs.size(); ++i) {
      CygTlsLog* current_log = all_logs.logs[i];
      base::AutoLock current_lock(current_log->log_mutex_);
      if (current_log->buf_.size()) {
        current_log->FlushLog();
      } else {
        // The thread's log is still empty. Probably the thread finished prior
        // to previous timer fired - deallocate its buffer. Even if the thread
        // ever resumes, it will allocate its buffer again in
        // std::vector::push_back().
        current_log->buf_.clear();
      }
    }
  }
}

// Gcc Compiler callback, called on every function invocation providing
// addresses of caller and callee codes.
void __cyg_profile_func_enter(void* this_fn, void* callee_unused) {
  if (tls_current_log == NULL) {
    tls_current_log = kMagicBeingConstructed;
    CygTlsLog* newlog = new CygTlsLog;
    CHECK(newlog);
    CygTlsLog::AddNewLog(newlog);
    tls_current_log = newlog;
  }
  if (tls_current_log != kMagicBeingConstructed) {
    tls_current_log->LogEnter(this_fn);
  }
}

// Gcc Compiler callback, called after every function invocation providing
// addresses of caller and callee codes.
void __cyg_profile_func_exit(void* this_fn, void* call_site) {
}

}  // namespace cygprofile
