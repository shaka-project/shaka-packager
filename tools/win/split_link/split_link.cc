// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <shlwapi.h>

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#ifndef SPLIT_LINK_SCRIPT_PATH
#error SPLIT_LINK_SCRIPT_PATH must be defined as the path to "split_link.py".
#endif

#ifndef PYTHON_PATH
#error PYTHON_PATH must be defined to be the path to the python binary.
#endif

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define WPYTHON_PATH WIDEN(PYTHON_PATH)
#define WSPLIT_LINK_SCRIPT_PATH WIDEN(SPLIT_LINK_SCRIPT_PATH)

using namespace std;

// Don't use stderr for errors because VS has large buffers on them, leading
// to confusing error output.
static void Fatal(const wchar_t* msg) {
  wprintf(L"split_link fatal error: %s\n", msg);
  exit(1);
}

static wstring ErrorMessageToString(DWORD err) {
  wchar_t* msg_buf = NULL;
  DWORD rc = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM,
                           NULL,
                           err,
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                           reinterpret_cast<LPTSTR>(&msg_buf),
                           0,
                           NULL);
  if (!rc)
    return L"unknown error";
  wstring ret(msg_buf);
  LocalFree(msg_buf);
  return ret;
}

static void ArgvQuote(const std::wstring& argument,
                      std::wstring* command_line) {
  // Don't quote unless we actually need to.
  if (!argument.empty() &&
      argument.find_first_of(L" \t\n\v\"") == argument.npos) {
    command_line->append(argument);
  } else {
    command_line->push_back(L'"');
    for (std::wstring::const_iterator it = argument.begin();; ++it) {
      int num_backslashes = 0;
      while (it != argument.end() && *it == L'\\') {
        ++it;
        ++num_backslashes;
      }
      if (it == argument.end()) {
        // Escape all backslashes, but let the terminating double quotation
        // mark we add below be interpreted as a metacharacter.
        command_line->append(num_backslashes * 2, L'\\');
        break;
      } else if (*it == L'"') {
        // Escape all backslashes and the following double quotation mark.
        command_line->append(num_backslashes * 2 + 1, L'\\');
        command_line->push_back(*it);
      } else {
        // Backslashes aren't special here.
        command_line->append(num_backslashes, L'\\');
        command_line->push_back(*it);
      }
    }
    command_line->push_back(L'"');
  }
}

// Does the opposite of CommandLineToArgvW. Suitable for CreateProcess, but
// not for cmd.exe. |args| should include the program name as argv[0].
// See http://blogs.msdn.com/b/twistylittlepassagesallalike/archive/2011/04/23/everyone-quotes-arguments-the-wrong-way.aspx
static wstring BuildCommandLine(const vector<wstring>& args) {
  std::wstring result;
  for (size_t i = 0; i < args.size(); ++i) {
    ArgvQuote(args[i], &result);
    if (i < args.size() - 1) {
      result += L" ";
    }
  }
  return result;
}

static void RunLinker(const vector<wstring>& prefix, const wchar_t* msg) {
  if (msg) {
    wprintf(L"split_link failed (%s), trying to fallback to standard link.\n",
            msg);
    wprintf(L"Original command line: %s\n", GetCommandLine());
    fflush(stdout);
  }

  STARTUPINFO startup_info = { sizeof(STARTUPINFO) };
  PROCESS_INFORMATION process_info;
  DWORD exit_code;

  GetStartupInfo(&startup_info);

  if (getenv("SPLIT_LINK_DEBUG")) {
    wprintf(L"  original command line '%s'\n", GetCommandLine());
    fflush(stdout);
  }

  int num_args;
  LPWSTR* args = CommandLineToArgvW(GetCommandLine(), &num_args);
  if (!args)
    Fatal(L"Couldn't parse command line.");
  vector<wstring> argv;
  argv.insert(argv.end(), prefix.begin(), prefix.end());
  for (int i = 1; i < num_args; ++i)  // Skip old argv[0].
    argv.push_back(args[i]);
  LocalFree(args);

  wstring cmd = BuildCommandLine(argv);

  if (getenv("SPLIT_LINK_DEBUG")) {
    wprintf(L"  running '%s'\n", cmd.c_str());
    fflush(stdout);
  }
  if (!CreateProcess(NULL,
                     reinterpret_cast<LPWSTR>(const_cast<wchar_t *>(
                             cmd.c_str())),
                     NULL,
                     NULL,
                     TRUE,
                     0,
                     NULL,
                     NULL,
                     &startup_info, &process_info)) {
    wstring error = ErrorMessageToString(GetLastError());
    Fatal(error.c_str());
  }
  CloseHandle(process_info.hThread);
  WaitForSingleObject(process_info.hProcess, INFINITE);
  GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hProcess);
  exit(exit_code);
}

static void Fallback(const wchar_t* msg) {
  wchar_t original_link[1024];
  DWORD type;
  DWORD size = sizeof(original_link);
  if (SHGetValue(HKEY_CURRENT_USER,
                 L"Software\\Chromium\\split_link_installed",
                 NULL,
                 &type,
                 original_link,
                 &size) != ERROR_SUCCESS || type != REG_SZ) {
    Fatal(L"Couldn't retrieve linker location from "
          L"HKCU\\Software\\Chromium\\split_link_installed.");
  }
  if (getenv("SPLIT_LINK_DEBUG")) {
    wprintf(L"  got original linker '%s'\n", original_link);
    fflush(stdout);
  }
  vector<wstring> link_binary;
  link_binary.push_back(original_link);
  RunLinker(link_binary, msg);
}

static void Fallback() {
  Fallback(NULL);
}

static unsigned char* SlurpFile(const wchar_t* path, size_t* length) {
  HANDLE file = CreateFile(
      path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if (file == INVALID_HANDLE_VALUE)
    Fallback(L"couldn't open file");
  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(file, &file_size))
    Fallback(L"couldn't get file size");
  *length = static_cast<size_t>(file_size.QuadPart);
  unsigned char* buffer = static_cast<unsigned char*>(malloc(*length));
  DWORD bytes_read = 0;
  if (!ReadFile(file, buffer, *length, &bytes_read, NULL))
    Fallback(L"couldn't read file");
  return buffer;
}

static bool SplitLinkRequested(const wchar_t* rsp_path) {
  size_t length;
  unsigned char* data = SlurpFile(rsp_path, &length);
  bool flag_found = false;
  if (data[0] == 0xff && data[1] == 0xfe) {
    // UTF-16LE
    wstring wide(reinterpret_cast<wchar_t*>(&data[2]),
                 length / sizeof(wchar_t) - 1);
    flag_found = wide.find(L"/splitlink") != wide.npos;
  } else {
    string narrow(reinterpret_cast<char*>(data), length);
    flag_found = narrow.find("/splitlink") != narrow.npos;
  }
  free(data);
  return flag_found;
}

// If /splitlink is on the command line, delegate to split_link.py, otherwise
// fallback to standard linker.
int wmain(int argc, wchar_t** argv) {
  int rsp_file_index = -1;

  if (argc < 2)
    Fallback();

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == L'@') {
      rsp_file_index = i;
      break;
    }
  }

  if (rsp_file_index == -1)
    Fallback(L"couldn't find a response file in argv");

  if (getenv("SPLIT_LINK_DEBUG")) {
    wstring backup_copy(&argv[rsp_file_index][1]);
    backup_copy += L".copy";
    wchar_t buf[1024];
    swprintf(buf,
             sizeof(buf),
             L"copy %s %s",
             &argv[rsp_file_index][1],
             backup_copy.c_str());
    if (_wsystem(buf) == 0)
      wprintf(L"Saved original rsp as %s\n", backup_copy.c_str());
    else
      wprintf(L"'%s' failed.", buf);
  }

  if (SplitLinkRequested(&argv[rsp_file_index][1])) {
    vector<wstring> link_binary;
    link_binary.push_back(WPYTHON_PATH);
    link_binary.push_back(WSPLIT_LINK_SCRIPT_PATH);
    RunLinker(link_binary, NULL);
  }

  // Otherwise, run regular linker silently.
  Fallback();
}
