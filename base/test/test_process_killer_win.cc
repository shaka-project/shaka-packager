// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_process_killer_win.h"

#include <windows.h>
#include <winternl.h>

#include <algorithm>

#include "base/logging.h"
#include "base/process/kill.h"
#include "base/process/process_iterator.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_handle.h"

namespace {

typedef LONG WINAPI
NtQueryInformationProcess(
  IN HANDLE ProcessHandle,
  IN PROCESSINFOCLASS ProcessInformationClass,
  OUT PVOID ProcessInformation,
  IN ULONG ProcessInformationLength,
  OUT PULONG ReturnLength OPTIONAL
);

// Get the function pointer to NtQueryInformationProcess in NTDLL.DLL
static bool GetQIP(NtQueryInformationProcess** qip_func_ptr) {
  static NtQueryInformationProcess* qip_func =
      reinterpret_cast<NtQueryInformationProcess*>(
          GetProcAddress(GetModuleHandle(L"ntdll.dll"),
          "NtQueryInformationProcess"));
  DCHECK(qip_func) << "Could not get pointer to NtQueryInformationProcess.";
  *qip_func_ptr = qip_func;
  return qip_func != NULL;
}

// Get the command line of a process
bool GetCommandLineForProcess(uint32 process_id, string16* cmd_line) {
  DCHECK(process_id != 0);
  DCHECK(cmd_line);

  // Open the process
  base::win::ScopedHandle process_handle(::OpenProcess(
      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
      false,
      process_id));
  if (!process_handle) {
    DLOG(ERROR) << "Failed to open process " << process_id << ", last error = "
                << GetLastError();
  }

  // Obtain Process Environment Block
  NtQueryInformationProcess* qip_func = NULL;
  if (process_handle) {
    GetQIP(&qip_func);
  }

  // Read the address of the process params from the peb.
  DWORD process_params_address = 0;
  if (qip_func) {
    PROCESS_BASIC_INFORMATION info = { 0 };
    // NtQueryInformationProcess returns an NTSTATUS for whom negative values
    // are negative. Just check for that instead of pulling in DDK macros.
    if ((qip_func(process_handle.Get(),
                  ProcessBasicInformation,
                  &info,
                  sizeof(info),
                  NULL)) < 0) {
      DLOG(ERROR) << "Failed to invoke NtQueryProcessInformation, last error = "
                  << GetLastError();
    } else {
      BYTE* peb = reinterpret_cast<BYTE*>(info.PebBaseAddress);

      // The process command line parameters are (or were once) located at
      // the base address of the PEB + 0x10 for 32 bit processes. 64 bit
      // processes have a different PEB struct as per
      // http://msdn.microsoft.com/en-us/library/aa813706(VS.85).aspx.
      // TODO(robertshield): See about doing something about this.
      SIZE_T bytes_read = 0;
      if (!::ReadProcessMemory(process_handle.Get(),
                               peb + 0x10,
                               &process_params_address,
                               sizeof(process_params_address),
                               &bytes_read)) {
        DLOG(ERROR) << "Failed to read process params address, last error = "
                    << GetLastError();
      }
    }
  }

  // Copy all the process parameters into a buffer.
  bool success = false;
  string16 buffer;
  if (process_params_address) {
    SIZE_T bytes_read;
    RTL_USER_PROCESS_PARAMETERS params = { 0 };
    if (!::ReadProcessMemory(process_handle.Get(),
                             reinterpret_cast<void*>(process_params_address),
                             &params,
                             sizeof(params),
                             &bytes_read)) {
      DLOG(ERROR) << "Failed to read RTL_USER_PROCESS_PARAMETERS, "
                  << "last error = " << GetLastError();
    } else {
      // Read the command line parameter
      const int max_cmd_line_len = std::min(
          static_cast<int>(params.CommandLine.MaximumLength),
          4096);
      buffer.resize(max_cmd_line_len + 1);
      if (!::ReadProcessMemory(process_handle.Get(),
                               params.CommandLine.Buffer,
                               &buffer[0],
                               max_cmd_line_len,
                               &bytes_read)) {
        DLOG(ERROR) << "Failed to copy process command line, "
                    << "last error = " << GetLastError();
      } else {
        *cmd_line = buffer;
        success = true;
      }
    }
  }

  return success;
}

// Used to filter processes by process ID.
class ArgumentFilter : public base::ProcessFilter {
 public:
  explicit ArgumentFilter(const string16& argument)
      : argument_to_find_(argument) {}

  // Returns true to indicate set-inclusion and false otherwise.  This method
  // should not have side-effects and should be idempotent.
  virtual bool Includes(const base::ProcessEntry& entry) const {
    bool found = false;
    string16 command_line;
    if (GetCommandLineForProcess(entry.pid(), &command_line)) {
      string16::const_iterator it =
          std::search(command_line.begin(),
                      command_line.end(),
                      argument_to_find_.begin(),
                      argument_to_find_.end(),
          base::CaseInsensitiveCompareASCII<wchar_t>());
      found = (it != command_line.end());
    }
    return found;
  }

 protected:
  string16 argument_to_find_;
};

}  // namespace

namespace base {

bool KillAllNamedProcessesWithArgument(const string16& process_name,
                                       const string16& argument) {
  ArgumentFilter argument_filter(argument);
  return base::KillProcesses(process_name, 0, &argument_filter);
}

}  // namespace base
