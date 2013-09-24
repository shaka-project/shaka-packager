// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"

#include <windows.h>

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"

namespace base {

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // We try to limit privileges granted to the handle. If you need this
  // for test code, consider using OpenPrivilegedProcessHandle instead of
  // adding more privileges here.
  ProcessHandle result = OpenProcess(PROCESS_TERMINATE |
                                     PROCESS_QUERY_INFORMATION |
                                     SYNCHRONIZE,
                                     FALSE, pid);

  if (result == NULL)
    return false;

  *handle = result;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(PROCESS_DUP_HANDLE |
                                     PROCESS_TERMINATE |
                                     PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_READ |
                                     SYNCHRONIZE,
                                     FALSE, pid);

  if (result == NULL)
    return false;

  *handle = result;
  return true;
}

bool OpenProcessHandleWithAccess(ProcessId pid,
                                 uint32 access_flags,
                                 ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(access_flags, FALSE, pid);

  if (result == NULL)
    return false;

  *handle = result;
  return true;
}

void CloseProcessHandle(ProcessHandle process) {
  CloseHandle(process);
}

ProcessId GetProcId(ProcessHandle process) {
  // This returns 0 if we have insufficient rights to query the process handle.
  return GetProcessId(process);
}

bool GetProcessIntegrityLevel(ProcessHandle process, IntegrityLevel *level) {
  if (!level)
    return false;

  if (win::GetVersion() < base::win::VERSION_VISTA)
    return false;

  HANDLE process_token;
  if (!OpenProcessToken(process, TOKEN_QUERY | TOKEN_QUERY_SOURCE,
      &process_token))
    return false;

  win::ScopedHandle scoped_process_token(process_token);

  DWORD token_info_length = 0;
  if (GetTokenInformation(process_token, TokenIntegrityLevel, NULL, 0,
                          &token_info_length) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    return false;

  scoped_ptr<char[]> token_label_bytes(new char[token_info_length]);
  if (!token_label_bytes.get())
    return false;

  TOKEN_MANDATORY_LABEL* token_label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(token_label_bytes.get());
  if (!token_label)
    return false;

  if (!GetTokenInformation(process_token, TokenIntegrityLevel, token_label,
                           token_info_length, &token_info_length))
    return false;

  DWORD integrity_level = *GetSidSubAuthority(token_label->Label.Sid,
      (DWORD)(UCHAR)(*GetSidSubAuthorityCount(token_label->Label.Sid)-1));

  if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID) {
    *level = LOW_INTEGRITY;
  } else if (integrity_level >= SECURITY_MANDATORY_MEDIUM_RID &&
      integrity_level < SECURITY_MANDATORY_HIGH_RID) {
    *level = MEDIUM_INTEGRITY;
  } else if (integrity_level >= SECURITY_MANDATORY_HIGH_RID) {
    *level = HIGH_INTEGRITY;
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

}  // namespace base
