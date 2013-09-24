// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <windows.h>

#include "base/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace base {

typedef HMODULE (WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

NativeLibrary LoadNativeLibraryHelper(const FilePath& library_path,
                                      LoadLibraryFunction load_library_api) {
  // LoadLibrary() opens the file off disk.
  base::ThreadRestrictions::AssertIOAllowed();

  // Switch the current directory to the library directory as the library
  // may have dependencies on DLLs in this directory.
  bool restore_directory = false;
  FilePath current_directory;
  if (file_util::GetCurrentDirectory(&current_directory)) {
    FilePath plugin_path = library_path.DirName();
    if (!plugin_path.empty()) {
      file_util::SetCurrentDirectory(plugin_path);
      restore_directory = true;
    }
  }

  HMODULE module = (*load_library_api)(library_path.value().c_str());
  if (restore_directory)
    file_util::SetCurrentDirectory(current_directory);

  return module;
}

// static
NativeLibrary LoadNativeLibrary(const FilePath& library_path,
                                std::string* error) {
  return LoadNativeLibraryHelper(library_path, LoadLibraryW);
}

NativeLibrary LoadNativeLibraryDynamically(const FilePath& library_path) {
  typedef HMODULE (WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

  LoadLibraryFunction load_library;
  load_library = reinterpret_cast<LoadLibraryFunction>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW"));

  return LoadNativeLibraryHelper(library_path, load_library);
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  FreeLibrary(library);
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          const char* name) {
  return GetProcAddress(library, name);
}

// static
string16 GetNativeLibraryName(const string16& name) {
  return name + ASCIIToUTF16(".dll");
}

}  // namespace base
