// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/process/internal_linux.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"

namespace base {

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

// Read /proc/<pid>/status and returns the value for |field|, or 0 on failure.
// Only works for fields in the form of "Field: value kB".
size_t ReadProcStatusAndGetFieldAsSizeT(pid_t pid, const std::string& field) {
  FilePath stat_file = internal::GetProcPidDir(pid).Append("status");
  std::string status;
  {
    // Synchronously reading files in /proc is safe.
    ThreadRestrictions::ScopedAllowIO allow_io;
    if (!file_util::ReadFileToString(stat_file, &status))
      return 0;
  }

  StringTokenizer tokenizer(status, ":\n");
  ParsingState state = KEY_NAME;
  StringPiece last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token_piece();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == field) {
          std::string value_str;
          tokenizer.token_piece().CopyToString(&value_str);
          std::string value_str_trimmed;
          TrimWhitespaceASCII(value_str, TRIM_ALL, &value_str_trimmed);
          std::vector<std::string> split_value_str;
          SplitString(value_str_trimmed, ' ', &split_value_str);
          if (split_value_str.size() != 2 || split_value_str[1] != "kB") {
            NOTREACHED();
            return 0;
          }
          size_t value;
          if (!StringToSizeT(split_value_str[0], &value)) {
            NOTREACHED();
            return 0;
          }
          return value;
        }
        state = KEY_NAME;
        break;
    }
  }
  NOTREACHED();
  return 0;
}

// Get the total CPU of a single process.  Return value is number of jiffies
// on success or -1 on error.
int GetProcessCPU(pid_t pid) {
  // Use /proc/<pid>/task to find all threads and parse their /stat file.
  FilePath task_path = internal::GetProcPidDir(pid).Append("task");

  DIR* dir = opendir(task_path.value().c_str());
  if (!dir) {
    DPLOG(ERROR) << "opendir(" << task_path.value() << ")";
    return -1;
  }

  int total_cpu = 0;
  while (struct dirent* ent = readdir(dir)) {
    pid_t tid = internal::ProcDirSlotToPid(ent->d_name);
    if (!tid)
      continue;

    // Synchronously reading files in /proc is safe.
    ThreadRestrictions::ScopedAllowIO allow_io;

    std::string stat;
    FilePath stat_path =
        task_path.Append(ent->d_name).Append(internal::kStatFile);
    if (file_util::ReadFileToString(stat_path, &stat)) {
      int cpu = ParseProcStatCPU(stat);
      if (cpu > 0)
        total_cpu += cpu;
    }
  }
  closedir(dir);

  return total_cpu;
}

}  // namespace

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

// On linux, we return vsize.
size_t ProcessMetrics::GetPagefileUsage() const {
  return internal::ReadProcStatsAndGetFieldAsSizeT(process_,
                                                   internal::VM_VSIZE);
}

// On linux, we return the high water mark of vsize.
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  return ReadProcStatusAndGetFieldAsSizeT(process_, "VmPeak") * 1024;
}

// On linux, we return RSS.
size_t ProcessMetrics::GetWorkingSetSize() const {
  return internal::ReadProcStatsAndGetFieldAsSizeT(process_, internal::VM_RSS) *
      getpagesize();
}

// On linux, we return the high water mark of RSS.
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  return ReadProcStatusAndGetFieldAsSizeT(process_, "VmHWM") * 1024;
}

bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  WorkingSetKBytes ws_usage;
  if (!GetWorkingSetKBytes(&ws_usage))
    return false;

  if (private_bytes)
    *private_bytes = ws_usage.priv * 1024;

  if (shared_bytes)
    *shared_bytes = ws_usage.shared * 1024;

  return true;
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
#if defined(OS_CHROMEOS)
  if (GetWorkingSetKBytesTotmaps(ws_usage))
    return true;
#endif
  return GetWorkingSetKBytesStatm(ws_usage);
}

double ProcessMetrics::GetCPUUsage() {
  struct timeval now;
  int retval = gettimeofday(&now, NULL);
  if (retval)
    return 0;
  int64 time = TimeValToMicroseconds(now);

  if (last_time_ == 0) {
    // First call, just set the last values.
    last_time_ = time;
    last_cpu_ = GetProcessCPU(process_);
    return 0;
  }

  int64 time_delta = time - last_time_;
  DCHECK_NE(time_delta, 0);
  if (time_delta == 0)
    return 0;

  int cpu = GetProcessCPU(process_);

  // We have the number of jiffies in the time period.  Convert to percentage.
  // Note this means we will go *over* 100 in the case where multiple threads
  // are together adding to more than one CPU's worth.
  TimeDelta cpu_time = internal::ClockTicksToTimeDelta(cpu);
  TimeDelta last_cpu_time = internal::ClockTicksToTimeDelta(last_cpu_);
  int percentage = 100 * (cpu_time - last_cpu_time).InSecondsF() /
      TimeDelta::FromMicroseconds(time_delta).InSecondsF();

  last_time_ = time;
  last_cpu_ = cpu;

  return percentage;
}

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  // Synchronously reading files in /proc is safe.
  ThreadRestrictions::ScopedAllowIO allow_io;

  std::string proc_io_contents;
  FilePath io_file = internal::GetProcPidDir(process_).Append("io");
  if (!file_util::ReadFileToString(io_file, &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  StringPiece last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token_piece();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).ReadOperationCount));
        } else if (last_key_name == "syscw") {
          StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).WriteOperationCount));
        } else if (last_key_name == "rchar") {
          StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).ReadTransferCount));
        } else if (last_key_name == "wchar") {
          StringToInt64(tokenizer.token_piece(),
              reinterpret_cast<int64*>(&(*io_counters).WriteTransferCount));
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
}

ProcessMetrics::ProcessMetrics(ProcessHandle process)
    : process_(process),
      last_time_(0),
      last_system_time_(0),
      last_cpu_(0) {
  processor_count_ = base::SysInfo::NumberOfProcessors();
}

#if defined(OS_CHROMEOS)
// Private, Shared and Proportional working set sizes are obtained from
// /proc/<pid>/totmaps
bool ProcessMetrics::GetWorkingSetKBytesTotmaps(WorkingSetKBytes *ws_usage)
  const {
  // The format of /proc/<pid>/totmaps is:
  //
  // Rss:                6120 kB
  // Pss:                3335 kB
  // Shared_Clean:       1008 kB
  // Shared_Dirty:       4012 kB
  // Private_Clean:         4 kB
  // Private_Dirty:      1096 kB
  // Referenced:          XXX kB
  // Anonymous:           XXX kB
  // AnonHugePages:       XXX kB
  // Swap:                XXX kB
  // Locked:              XXX kB
  const size_t kPssIndex = (1 * 3) + 1;
  const size_t kPrivate_CleanIndex = (4 * 3) + 1;
  const size_t kPrivate_DirtyIndex = (5 * 3) + 1;
  const size_t kSwapIndex = (9 * 3) + 1;

  std::string totmaps_data;
  {
    FilePath totmaps_file = internal::GetProcPidDir(process_).Append("totmaps");
    ThreadRestrictions::ScopedAllowIO allow_io;
    bool ret = file_util::ReadFileToString(totmaps_file, &totmaps_data);
    if (!ret || totmaps_data.length() == 0)
      return false;
  }

  std::vector<std::string> totmaps_fields;
  SplitStringAlongWhitespace(totmaps_data, &totmaps_fields);

  DCHECK_EQ("Pss:", totmaps_fields[kPssIndex-1]);
  DCHECK_EQ("Private_Clean:", totmaps_fields[kPrivate_CleanIndex - 1]);
  DCHECK_EQ("Private_Dirty:", totmaps_fields[kPrivate_DirtyIndex - 1]);
  DCHECK_EQ("Swap:", totmaps_fields[kSwapIndex-1]);

  int pss = 0;
  int private_clean = 0;
  int private_dirty = 0;
  int swap = 0;
  bool ret = true;
  ret &= StringToInt(totmaps_fields[kPssIndex], &pss);
  ret &= StringToInt(totmaps_fields[kPrivate_CleanIndex], &private_clean);
  ret &= StringToInt(totmaps_fields[kPrivate_DirtyIndex], &private_dirty);
  ret &= StringToInt(totmaps_fields[kSwapIndex], &swap);

  // On ChromeOS swap is to zram. We count this as private / shared, as
  // increased swap decreases available RAM to user processes, which would
  // otherwise create surprising results.
  ws_usage->priv = private_clean + private_dirty + swap;
  ws_usage->shared = pss + swap;
  ws_usage->shareable = 0;
  ws_usage->swapped = swap;
  return ret;
}
#endif

// Private and Shared working set sizes are obtained from /proc/<pid>/statm.
bool ProcessMetrics::GetWorkingSetKBytesStatm(WorkingSetKBytes* ws_usage)
    const {
  // Use statm instead of smaps because smaps is:
  // a) Large and slow to parse.
  // b) Unavailable in the SUID sandbox.

  // First we need to get the page size, since everything is measured in pages.
  // For details, see: man 5 proc.
  const int page_size_kb = getpagesize() / 1024;
  if (page_size_kb <= 0)
    return false;

  std::string statm;
  {
    FilePath statm_file = internal::GetProcPidDir(process_).Append("statm");
    // Synchronously reading files in /proc is safe.
    ThreadRestrictions::ScopedAllowIO allow_io;
    bool ret = file_util::ReadFileToString(statm_file, &statm);
    if (!ret || statm.length() == 0)
      return false;
  }

  std::vector<std::string> statm_vec;
  SplitString(statm, ' ', &statm_vec);
  if (statm_vec.size() != 7)
    return false;  // Not the format we expect.

  int statm_rss, statm_shared;
  bool ret = true;
  ret &= StringToInt(statm_vec[1], &statm_rss);
  ret &= StringToInt(statm_vec[2], &statm_shared);

  ws_usage->priv = (statm_rss - statm_shared) * page_size_kb;
  ws_usage->shared = statm_shared * page_size_kb;

  // Sharable is not calculated, as it does not provide interesting data.
  ws_usage->shareable = 0;

#if defined(OS_CHROMEOS)
  // Can't get swapped memory from statm.
  ws_usage->swapped = 0;
#endif

  return ret;
}

size_t GetSystemCommitCharge() {
  SystemMemoryInfoKB meminfo;
  if (!GetSystemMemoryInfo(&meminfo))
    return 0;
  return meminfo.total - meminfo.free - meminfo.buffers - meminfo.cached;
}

// Exposed for testing.
int ParseProcStatCPU(const std::string& input) {
  std::vector<std::string> proc_stats;
  if (!internal::ParseProcStats(input, &proc_stats))
    return -1;

  if (proc_stats.size() <= internal::VM_STIME)
    return -1;
  int utime = GetProcStatsFieldAsInt(proc_stats, internal::VM_UTIME);
  int stime = GetProcStatsFieldAsInt(proc_stats, internal::VM_STIME);
  return utime + stime;
}

namespace {

// The format of /proc/meminfo is:
//
// MemTotal:      8235324 kB
// MemFree:       1628304 kB
// Buffers:        429596 kB
// Cached:        4728232 kB
// ...
const size_t kMemTotalIndex = 1;
const size_t kMemFreeIndex = 4;
const size_t kMemBuffersIndex = 7;
const size_t kMemCachedIndex = 10;
const size_t kMemActiveAnonIndex = 22;
const size_t kMemInactiveAnonIndex = 25;
const size_t kMemActiveFileIndex = 28;
const size_t kMemInactiveFileIndex = 31;

}  // namespace

SystemMemoryInfoKB::SystemMemoryInfoKB()
    : total(0),
      free(0),
      buffers(0),
      cached(0),
      active_anon(0),
      inactive_anon(0),
      active_file(0),
      inactive_file(0),
      shmem(0),
      gem_objects(-1),
      gem_size(-1) {
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  // Synchronously reading files in /proc is safe.
  ThreadRestrictions::ScopedAllowIO allow_io;

  // Used memory is: total - free - buffers - caches
  FilePath meminfo_file("/proc/meminfo");
  std::string meminfo_data;
  if (!file_util::ReadFileToString(meminfo_file, &meminfo_data)) {
    DLOG(WARNING) << "Failed to open " << meminfo_file.value();
    return false;
  }
  std::vector<std::string> meminfo_fields;
  SplitStringAlongWhitespace(meminfo_data, &meminfo_fields);

  if (meminfo_fields.size() < kMemCachedIndex) {
    DLOG(WARNING) << "Failed to parse " << meminfo_file.value()
                  << ".  Only found " << meminfo_fields.size() << " fields.";
    return false;
  }

  DCHECK_EQ(meminfo_fields[kMemTotalIndex-1], "MemTotal:");
  DCHECK_EQ(meminfo_fields[kMemFreeIndex-1], "MemFree:");
  DCHECK_EQ(meminfo_fields[kMemBuffersIndex-1], "Buffers:");
  DCHECK_EQ(meminfo_fields[kMemCachedIndex-1], "Cached:");
  DCHECK_EQ(meminfo_fields[kMemActiveAnonIndex-1], "Active(anon):");
  DCHECK_EQ(meminfo_fields[kMemInactiveAnonIndex-1], "Inactive(anon):");
  DCHECK_EQ(meminfo_fields[kMemActiveFileIndex-1], "Active(file):");
  DCHECK_EQ(meminfo_fields[kMemInactiveFileIndex-1], "Inactive(file):");

  StringToInt(meminfo_fields[kMemTotalIndex], &meminfo->total);
  StringToInt(meminfo_fields[kMemFreeIndex], &meminfo->free);
  StringToInt(meminfo_fields[kMemBuffersIndex], &meminfo->buffers);
  StringToInt(meminfo_fields[kMemCachedIndex], &meminfo->cached);
  StringToInt(meminfo_fields[kMemActiveAnonIndex], &meminfo->active_anon);
  StringToInt(meminfo_fields[kMemInactiveAnonIndex],
                    &meminfo->inactive_anon);
  StringToInt(meminfo_fields[kMemActiveFileIndex], &meminfo->active_file);
  StringToInt(meminfo_fields[kMemInactiveFileIndex],
                    &meminfo->inactive_file);
#if defined(OS_CHROMEOS)
  // Chrome OS has a tweaked kernel that allows us to query Shmem, which is
  // usually video memory otherwise invisible to the OS.  Unfortunately, the
  // meminfo format varies on different hardware so we have to search for the
  // string.  It always appears after "Cached:".
  for (size_t i = kMemCachedIndex+2; i < meminfo_fields.size(); i += 3) {
    if (meminfo_fields[i] == "Shmem:") {
      StringToInt(meminfo_fields[i+1], &meminfo->shmem);
      break;
    }
  }

  // Report on Chrome OS GEM object graphics memory. /var/run/debugfs_gpu is a
  // bind mount into /sys/kernel/debug and synchronously reading the in-memory
  // files in /sys is fast.
#if defined(ARCH_CPU_ARM_FAMILY)
  FilePath geminfo_file("/var/run/debugfs_gpu/exynos_gem_objects");
#else
  FilePath geminfo_file("/var/run/debugfs_gpu/i915_gem_objects");
#endif
  std::string geminfo_data;
  meminfo->gem_objects = -1;
  meminfo->gem_size = -1;
  if (file_util::ReadFileToString(geminfo_file, &geminfo_data)) {
    int gem_objects = -1;
    long long gem_size = -1;
    int num_res = sscanf(geminfo_data.c_str(),
                         "%d objects, %lld bytes",
                         &gem_objects, &gem_size);
    if (num_res == 2) {
      meminfo->gem_objects = gem_objects;
      meminfo->gem_size = gem_size;
    }
  }

#if defined(ARCH_CPU_ARM_FAMILY)
  // Incorporate Mali graphics memory if present.
  FilePath mali_memory_file("/sys/devices/platform/mali.0/memory");
  std::string mali_memory_data;
  if (file_util::ReadFileToString(mali_memory_file, &mali_memory_data)) {
    long long mali_size = -1;
    int num_res = sscanf(mali_memory_data.c_str(), "%lld bytes", &mali_size);
    if (num_res == 1)
      meminfo->gem_size += mali_size;
  }
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif  // defined(OS_CHROMEOS)

  return true;
}

const char kProcSelfExe[] = "/proc/self/exe";

int GetNumberOfThreads(ProcessHandle process) {
  return internal::ReadProcStatsAndGetFieldAsInt(process,
                                                 internal::VM_NUMTHREADS);
}

}  // namespace base
