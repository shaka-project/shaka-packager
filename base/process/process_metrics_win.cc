// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <windows.h>
#include <psapi.h>

#include "base/logging.h"
#include "base/sys_info.h"

namespace base {

// System pagesize. This value remains constant on x86/64 architectures.
const int PAGESIZE_KB = 4;

ProcessMetrics::~ProcessMetrics() { }

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

size_t ProcessMetrics::GetPagefileUsage() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PagefileUsage;
  }
  return 0;
}

// Returns the peak space allocated for the pagefile, in bytes.
size_t ProcessMetrics::GetPeakPagefileUsage() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PeakPagefileUsage;
  }
  return 0;
}

// Returns the current working set size, in bytes.
size_t ProcessMetrics::GetWorkingSetSize() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.WorkingSetSize;
  }
  return 0;
}

// Returns the peak working set size, in bytes.
size_t ProcessMetrics::GetPeakWorkingSetSize() const {
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(process_, &pmc, sizeof(pmc))) {
    return pmc.PeakWorkingSetSize;
  }
  return 0;
}

bool ProcessMetrics::GetMemoryBytes(size_t* private_bytes,
                                    size_t* shared_bytes) {
  // PROCESS_MEMORY_COUNTERS_EX is not supported until XP SP2.
  // GetProcessMemoryInfo() will simply fail on prior OS. So the requested
  // information is simply not available. Hence, we will return 0 on unsupported
  // OSes. Unlike most Win32 API, we don't need to initialize the "cb" member.
  PROCESS_MEMORY_COUNTERS_EX pmcx;
  if (private_bytes &&
      GetProcessMemoryInfo(process_,
                           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmcx),
                           sizeof(pmcx))) {
    *private_bytes = pmcx.PrivateUsage;
  }

  if (shared_bytes) {
    WorkingSetKBytes ws_usage;
    if (!GetWorkingSetKBytes(&ws_usage))
      return false;

    *shared_bytes = ws_usage.shared * 1024;
  }

  return true;
}

void ProcessMetrics::GetCommittedKBytes(CommittedKBytes* usage) const {
  MEMORY_BASIC_INFORMATION mbi = {0};
  size_t committed_private = 0;
  size_t committed_mapped = 0;
  size_t committed_image = 0;
  void* base_address = NULL;
  while (VirtualQueryEx(process_, base_address, &mbi, sizeof(mbi)) ==
      sizeof(mbi)) {
    if (mbi.State == MEM_COMMIT) {
      if (mbi.Type == MEM_PRIVATE) {
        committed_private += mbi.RegionSize;
      } else if (mbi.Type == MEM_MAPPED) {
        committed_mapped += mbi.RegionSize;
      } else if (mbi.Type == MEM_IMAGE) {
        committed_image += mbi.RegionSize;
      } else {
        NOTREACHED();
      }
    }
    void* new_base = (static_cast<BYTE*>(mbi.BaseAddress)) + mbi.RegionSize;
    // Avoid infinite loop by weird MEMORY_BASIC_INFORMATION.
    // If we query 64bit processes in a 32bit process, VirtualQueryEx()
    // returns such data.
    if (new_base <= base_address) {
      usage->image = 0;
      usage->mapped = 0;
      usage->priv = 0;
      return;
    }
    base_address = new_base;
  }
  usage->image = committed_image / 1024;
  usage->mapped = committed_mapped / 1024;
  usage->priv = committed_private / 1024;
}

bool ProcessMetrics::GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const {
  size_t ws_private = 0;
  size_t ws_shareable = 0;
  size_t ws_shared = 0;

  DCHECK(ws_usage);
  memset(ws_usage, 0, sizeof(*ws_usage));

  DWORD number_of_entries = 4096;  // Just a guess.
  PSAPI_WORKING_SET_INFORMATION* buffer = NULL;
  int retries = 5;
  for (;;) {
    DWORD buffer_size = sizeof(PSAPI_WORKING_SET_INFORMATION) +
                        (number_of_entries * sizeof(PSAPI_WORKING_SET_BLOCK));

    // if we can't expand the buffer, don't leak the previous
    // contents or pass a NULL pointer to QueryWorkingSet
    PSAPI_WORKING_SET_INFORMATION* new_buffer =
        reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(
            realloc(buffer, buffer_size));
    if (!new_buffer) {
      free(buffer);
      return false;
    }
    buffer = new_buffer;

    // Call the function once to get number of items
    if (QueryWorkingSet(process_, buffer, buffer_size))
      break;  // Success

    if (GetLastError() != ERROR_BAD_LENGTH) {
      free(buffer);
      return false;
    }

    number_of_entries = static_cast<DWORD>(buffer->NumberOfEntries);

    // Maybe some entries are being added right now. Increase the buffer to
    // take that into account.
    number_of_entries = static_cast<DWORD>(number_of_entries * 1.25);

    if (--retries == 0) {
      free(buffer);  // If we're looping, eventually fail.
      return false;
    }
  }

  // On windows 2000 the function returns 1 even when the buffer is too small.
  // The number of entries that we are going to parse is the minimum between the
  // size we allocated and the real number of entries.
  number_of_entries =
      std::min(number_of_entries, static_cast<DWORD>(buffer->NumberOfEntries));
  for (unsigned int i = 0; i < number_of_entries; i++) {
    if (buffer->WorkingSetInfo[i].Shared) {
      ws_shareable++;
      if (buffer->WorkingSetInfo[i].ShareCount > 1)
        ws_shared++;
    } else {
      ws_private++;
    }
  }

  ws_usage->priv = ws_private * PAGESIZE_KB;
  ws_usage->shareable = ws_shareable * PAGESIZE_KB;
  ws_usage->shared = ws_shared * PAGESIZE_KB;
  free(buffer);
  return true;
}

static uint64 FileTimeToUTC(const FILETIME& ftime) {
  LARGE_INTEGER li;
  li.LowPart = ftime.dwLowDateTime;
  li.HighPart = ftime.dwHighDateTime;
  return li.QuadPart;
}

double ProcessMetrics::GetCPUUsage() {
  FILETIME now;
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  GetSystemTimeAsFileTime(&now);

  if (!GetProcessTimes(process_, &creation_time, &exit_time,
                       &kernel_time, &user_time)) {
    // We don't assert here because in some cases (such as in the Task Manager)
    // we may call this function on a process that has just exited but we have
    // not yet received the notification.
    return 0;
  }
  int64 system_time = (FileTimeToUTC(kernel_time) + FileTimeToUTC(user_time)) /
                        processor_count_;
  int64 time = FileTimeToUTC(now);

  if ((last_system_time_ == 0) || (last_time_ == 0)) {
    // First call, just set the last values.
    last_system_time_ = system_time;
    last_time_ = time;
    return 0;
  }

  int64 system_time_delta = system_time - last_system_time_;
  int64 time_delta = time - last_time_;
  DCHECK_NE(0U, time_delta);
  if (time_delta == 0)
    return 0;

  // We add time_delta / 2 so the result is rounded.
  int cpu = static_cast<int>((system_time_delta * 100 + time_delta / 2) /
                             time_delta);

  last_system_time_ = system_time;
  last_time_ = time;

  return cpu;
}

bool ProcessMetrics::CalculateFreeMemory(FreeMBytes* free) const {
  const SIZE_T kTopAddress = 0x7F000000;
  const SIZE_T kMegabyte = 1024 * 1024;
  SIZE_T accumulated = 0;

  MEMORY_BASIC_INFORMATION largest = {0};
  UINT_PTR scan = 0;
  while (scan < kTopAddress) {
    MEMORY_BASIC_INFORMATION info;
    if (!::VirtualQueryEx(process_, reinterpret_cast<void*>(scan),
                          &info, sizeof(info)))
      return false;
    if (info.State == MEM_FREE) {
      accumulated += info.RegionSize;
      if (info.RegionSize > largest.RegionSize)
        largest = info;
    }
    scan += info.RegionSize;
  }
  free->largest = largest.RegionSize / kMegabyte;
  free->largest_ptr = largest.BaseAddress;
  free->total = accumulated / kMegabyte;
  return true;
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  return GetProcessIoCounters(process_, io_counters) != FALSE;
}

ProcessMetrics::ProcessMetrics(ProcessHandle process)
    : process_(process),
      processor_count_(base::SysInfo::NumberOfProcessors()),
      last_time_(0),
      last_system_time_(0) {
}

// GetPerformanceInfo is not available on WIN2K.  So we'll
// load it on-the-fly.
const wchar_t kPsapiDllName[] = L"psapi.dll";
typedef BOOL (WINAPI *GetPerformanceInfoFunction) (
    PPERFORMANCE_INFORMATION pPerformanceInformation,
    DWORD cb);

// Beware of races if called concurrently from multiple threads.
static BOOL InternalGetPerformanceInfo(
    PPERFORMANCE_INFORMATION pPerformanceInformation, DWORD cb) {
  static GetPerformanceInfoFunction GetPerformanceInfo_func = NULL;
  if (!GetPerformanceInfo_func) {
    HMODULE psapi_dll = ::GetModuleHandle(kPsapiDllName);
    if (psapi_dll)
      GetPerformanceInfo_func = reinterpret_cast<GetPerformanceInfoFunction>(
          GetProcAddress(psapi_dll, "GetPerformanceInfo"));

    if (!GetPerformanceInfo_func) {
      // The function could be loaded!
      memset(pPerformanceInformation, 0, cb);
      return FALSE;
    }
  }
  return GetPerformanceInfo_func(pPerformanceInformation, cb);
}

size_t GetSystemCommitCharge() {
  // Get the System Page Size.
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  PERFORMANCE_INFORMATION info;
  if (!InternalGetPerformanceInfo(&info, sizeof(info))) {
    DLOG(ERROR) << "Failed to fetch internal performance info.";
    return 0;
  }
  return (info.CommitTotal * system_info.dwPageSize) / 1024;
}

}  // namespace base
