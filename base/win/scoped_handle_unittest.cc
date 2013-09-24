// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

void CreateHandle(int value, HANDLE* result) {
  *result = reinterpret_cast<HANDLE>(value);
}

TEST(ScopedHandleTest, Receive) {
  base::win::ScopedHandle handle;
  int value = 51;

  {
    // This is not really the expected use case, but it is a very explicit test.
    base::win::ScopedHandle::Receiver a = handle.Receive();
    HANDLE* pointer = a;
    *pointer = reinterpret_cast<HANDLE>(value);
  }

  EXPECT_EQ(handle.Get(), reinterpret_cast<HANDLE>(value));
  HANDLE to_discard = handle.Take();

  // The standard use case:
  value = 183;
  CreateHandle(value, handle.Receive());
  EXPECT_EQ(handle.Get(), reinterpret_cast<HANDLE>(value));
  to_discard = handle.Take();
}
