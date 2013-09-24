// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/standard_out.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <stdio.h>
#endif

namespace {

bool initialized = false;

#if defined(OS_WIN)
HANDLE hstdout;
WORD default_attributes;

bool is_console = false;
#endif

void EnsureInitialized() {
  if (initialized)
    return;
  initialized = true;

#if defined(OS_WIN)
  hstdout = ::GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO info;
  is_console = !!::GetConsoleScreenBufferInfo(hstdout, &info);
  default_attributes = info.wAttributes;
#endif
}

}  // namespace

#if defined(OS_WIN)

void OutputString(const std::string& output, TextDecoration dec) {
  EnsureInitialized();
  if (is_console) {
    switch (dec) {
      case DECORATION_NONE:
        break;
      case DECORATION_BOLD:
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_INTENSITY);
        break;
      case DECORATION_RED:
        ::SetConsoleTextAttribute(hstdout,
                                  FOREGROUND_RED | FOREGROUND_INTENSITY);
        break;
      case DECORATION_GREEN:
        // Keep green non-bold.
        ::SetConsoleTextAttribute(hstdout, FOREGROUND_GREEN);
        break;
      case DECORATION_BLUE:
        ::SetConsoleTextAttribute(hstdout,
                                  FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        break;
      case DECORATION_YELLOW:
        ::SetConsoleTextAttribute(hstdout,
                                  FOREGROUND_RED | FOREGROUND_GREEN);
        break;
    }
  }

  DWORD written = 0;
  ::WriteFile(hstdout, output.c_str(), output.size(), &written, NULL);

  if (is_console)
    ::SetConsoleTextAttribute(hstdout, default_attributes);
}

#else

void OutputString(const std::string& output, TextDecoration dec) {
  printf("%s", output.c_str());
}

#endif
