// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_native_library.h"
#if defined(OS_WIN)
#include "base/files/file_path.h"
#endif

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Tests whether or not a function pointer retrieved via ScopedNativeLibrary
// is available only in a scope.
TEST(ScopedNativeLibrary, Basic) {
#if defined(OS_WIN)
  // Get the pointer to DirectDrawCreate() from "ddraw.dll" and verify it
  // is valid only in this scope.
  // FreeLibrary() doesn't actually unload a DLL until its reference count
  // becomes zero, i.e. this function pointer is still valid if the DLL used
  // in this test is also used by another part of this executable.
  // So, this test uses "ddraw.dll", which is not used by Chrome at all but
  // installed on all versions of Windows.
  FARPROC test_function;
  {
    FilePath path(GetNativeLibraryName(L"ddraw"));
    ScopedNativeLibrary library(path);
    test_function = reinterpret_cast<FARPROC>(
        library.GetFunctionPointer("DirectDrawCreate"));
    EXPECT_EQ(0, IsBadCodePtr(test_function));
  }
  EXPECT_NE(0, IsBadCodePtr(test_function));
#endif
}

}  // namespace base
