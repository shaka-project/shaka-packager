// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"

#include "base/callback.h"
#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void Increment(int* value) {
  (*value)++;
}

TEST(BindHelpersTest, TestScopedClosureRunnerExitScope) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(BindHelpersTest, TestScopedClosureRunnerRelease) {
  int run_count = 0;
  base::Closure c;
  {
    base::ScopedClosureRunner runner(base::Bind(&Increment, &run_count));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  c.Run();
  EXPECT_EQ(1, run_count);
}

}  // namespace
