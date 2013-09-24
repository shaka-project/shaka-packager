// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SelfAssign : public base::RefCounted<SelfAssign> {
  friend class base::RefCounted<SelfAssign>;

  ~SelfAssign() {}
};

class CheckDerivedMemberAccess : public scoped_refptr<SelfAssign> {
 public:
  CheckDerivedMemberAccess() {
    // This shouldn't compile if we don't have access to the member variable.
    SelfAssign** pptr = &ptr_;
    EXPECT_EQ(*pptr, ptr_);
  }
};

class ScopedRefPtrToSelf : public base::RefCounted<ScopedRefPtrToSelf> {
 public:
  ScopedRefPtrToSelf() : self_ptr_(this) {}

  static bool was_destroyed() { return was_destroyed_; }

  void SelfDestruct() { self_ptr_ = NULL; }

 private:
  friend class base::RefCounted<ScopedRefPtrToSelf>;
  ~ScopedRefPtrToSelf() { was_destroyed_ = true; }

  static bool was_destroyed_;

  scoped_refptr<ScopedRefPtrToSelf> self_ptr_;
};

bool ScopedRefPtrToSelf::was_destroyed_ = false;

}  // end namespace

TEST(RefCountedUnitTest, TestSelfAssignment) {
  SelfAssign* p = new SelfAssign;
  scoped_refptr<SelfAssign> var(p);
  var = var;
  EXPECT_EQ(var.get(), p);
}

TEST(RefCountedUnitTest, ScopedRefPtrMemberAccess) {
  CheckDerivedMemberAccess check;
}

TEST(RefCountedUnitTest, ScopedRefPtrToSelf) {
  ScopedRefPtrToSelf* check = new ScopedRefPtrToSelf();
  EXPECT_FALSE(ScopedRefPtrToSelf::was_destroyed());
  check->SelfDestruct();
  EXPECT_TRUE(ScopedRefPtrToSelf::was_destroyed());
}
