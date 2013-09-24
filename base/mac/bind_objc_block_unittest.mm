// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/bind_objc_block.h"

#include "base/callback.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(BindObjcBlockTest, TestScopedClosureRunnerExitScope) {
  int run_count = 0;
  int* ptr = &run_count;
  {
    base::ScopedClosureRunner runner(base::BindBlock(^{
        (*ptr)++;
    }));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(BindObjcBlockTest, TestScopedClosureRunnerRelease) {
  int run_count = 0;
  int* ptr = &run_count;
  base::Closure c;
  {
    base::ScopedClosureRunner runner(base::BindBlock(^{
        (*ptr)++;
    }));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  c.Run();
  EXPECT_EQ(1, run_count);
}

TEST(BindObjcBlockTest, TestReturnValue) {
  const int kReturnValue = 42;
  base::Callback<int(void)> c = base::BindBlock(^{return kReturnValue;});
  EXPECT_EQ(kReturnValue, c.Run());
}

TEST(BindObjcBlockTest, TestArgument) {
  const int kArgument = 42;
  base::Callback<int(int)> c = base::BindBlock(^(int a){return a + 1;});
  EXPECT_EQ(kArgument + 1, c.Run(kArgument));
}

}  // namespace
