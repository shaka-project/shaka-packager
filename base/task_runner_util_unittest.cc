// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_runner_util.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

int ReturnFourtyTwo() {
  return 42;
}

void StoreValue(int* destination, int value) {
  *destination = value;
}

void StoreDoubleValue(double* destination, double value) {
  *destination = value;
}

int g_foo_destruct_count = 0;
int g_foo_free_count = 0;

struct Foo {
  ~Foo() {
    ++g_foo_destruct_count;
  }
};

scoped_ptr<Foo> CreateFoo() {
  return scoped_ptr<Foo>(new Foo);
}

void ExpectFoo(scoped_ptr<Foo> foo) {
  EXPECT_TRUE(foo.get());
  scoped_ptr<Foo> local_foo(foo.Pass());
  EXPECT_TRUE(local_foo.get());
  EXPECT_FALSE(foo.get());
}

struct FreeFooFunctor {
  void operator()(Foo* foo) const {
    ++g_foo_free_count;
    delete foo;
  };
};

scoped_ptr_malloc<Foo, FreeFooFunctor> CreateScopedFoo() {
  return scoped_ptr_malloc<Foo, FreeFooFunctor>(new Foo);
}

void ExpectScopedFoo(scoped_ptr_malloc<Foo, FreeFooFunctor> foo) {
  EXPECT_TRUE(foo.get());
  scoped_ptr_malloc<Foo, FreeFooFunctor> local_foo(foo.Pass());
  EXPECT_TRUE(local_foo.get());
  EXPECT_FALSE(foo.get());
}

}  // namespace

TEST(TaskRunnerHelpersTest, PostTaskAndReplyWithResult) {
  int result = 0;

  MessageLoop message_loop;
  PostTaskAndReplyWithResult(message_loop.message_loop_proxy().get(),
                             FROM_HERE,
                             Bind(&ReturnFourtyTwo),
                             Bind(&StoreValue, &result));

  RunLoop().RunUntilIdle();

  EXPECT_EQ(42, result);
}

TEST(TaskRunnerHelpersTest, PostTaskAndReplyWithResultImplicitConvert) {
  double result = 0;

  MessageLoop message_loop;
  PostTaskAndReplyWithResult(message_loop.message_loop_proxy().get(),
                             FROM_HERE,
                             Bind(&ReturnFourtyTwo),
                             Bind(&StoreDoubleValue, &result));

  RunLoop().RunUntilIdle();

  EXPECT_DOUBLE_EQ(42.0, result);
}

TEST(TaskRunnerHelpersTest, PostTaskAndReplyWithResultPassed) {
  g_foo_destruct_count = 0;
  g_foo_free_count = 0;

  MessageLoop message_loop;
  PostTaskAndReplyWithResult(message_loop.message_loop_proxy().get(),
                             FROM_HERE,
                             Bind(&CreateFoo),
                             Bind(&ExpectFoo));

  RunLoop().RunUntilIdle();

  EXPECT_EQ(1, g_foo_destruct_count);
  EXPECT_EQ(0, g_foo_free_count);
}

TEST(TaskRunnerHelpersTest, PostTaskAndReplyWithResultPassedFreeProc) {
  g_foo_destruct_count = 0;
  g_foo_free_count = 0;

  MessageLoop message_loop;
  PostTaskAndReplyWithResult(message_loop.message_loop_proxy().get(),
                             FROM_HERE,
                             Bind(&CreateScopedFoo),
                             Bind(&ExpectScopedFoo));

  RunLoop().RunUntilIdle();

  EXPECT_EQ(1, g_foo_destruct_count);
  EXPECT_EQ(1, g_foo_free_count);
}

}  // namespace base
