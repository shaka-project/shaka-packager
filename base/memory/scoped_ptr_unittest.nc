// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"

namespace {

class Parent {
};

class Child : public Parent {
};

class RefCountedClass : public base::RefCountedThreadSafe<RefCountedClass> {
};

}  // namespace

#if defined(NCTEST_NO_PASSAS_DOWNCAST)  // [r"invalid conversion from"]

scoped_ptr<Child> DowncastUsingPassAs(scoped_ptr<Parent> object) {
  return object.PassAs<Child>();
}

#elif defined(NCTEST_NO_REF_COUNTED_SCOPED_PTR)  // [r"size of array is negative"]

// scoped_ptr<> should not work for ref-counted objects.
void WontCompile() {
  scoped_ptr<RefCountedClass> x;
}

#elif defined(NCTEST_NO_ARRAY_WITH_SIZE)  // [r"size of array is negative"]

void WontCompile() {
  scoped_ptr<int[10]> x;
}

#elif defined(NCTEST_NO_PASS_FROM_ARRAY)  // [r"size of array is negative"]

void WontCompile() {
  scoped_ptr<int[]> a;
  scoped_ptr<int*> b;
  b = a.Pass();
}

#elif defined(NCTEST_NO_PASS_TO_ARRAY)  // [r"no match for 'operator='"]

void WontCompile() {
  scoped_ptr<int*> a;
  scoped_ptr<int[]> b;
  b = a.Pass();
}

#elif defined(NCTEST_NO_CONSTRUCT_FROM_ARRAY)  // [r"is private"]

void WontCompile() {
  scoped_ptr<int[]> a;
  scoped_ptr<int*> b(a.Pass());
}

#elif defined(NCTEST_NO_CONSTRUCT_TO_ARRAY)  // [r"no matching function for call"]

void WontCompile() {
  scoped_ptr<int*> a;
  scoped_ptr<int[]> b(a.Pass());
}

#elif defined(NCTEST_NO_CONSTRUCT_SCOPED_PTR_ARRAY_FROM_NULL)  // [r"is ambiguous"]

void WontCompile() {
  scoped_ptr<int[]> x(NULL);
}

#elif defined(NCTEST_NO_CONSTRUCT_SCOPED_PTR_ARRAY_FROM_DERIVED)  // [r"is private"]

void WontCompile() {
  scoped_ptr<Parent[]> x(new Child[1]);
}

#elif defined(NCTEST_NO_RESET_SCOPED_PTR_ARRAY_FROM_NULL)  // [r"is ambiguous"]

void WontCompile() {
  scoped_ptr<int[]> x;
  x.reset(NULL);
}

#elif defined(NCTEST_NO_RESET_SCOPED_PTR_ARRAY_FROM_DERIVED)  // [r"is private"]

void WontCompile() {
  scoped_ptr<Parent[]> x;
  x.reset(new Child[1]);
}

#elif defined(NCTEST_NO_DELETER_REFERENCE)  // [r"fails to be a struct or class type"]

struct Deleter {
  void operator()(int*) {}
};

// Current implementation doesn't support Deleter Reference types. Enabling
// support would require changes to the behavior of the constructors to match
// including the use of SFINAE to discard the type-converting constructor
// as per C++11 20.7.1.2.1.19.
void WontCompile() {
  Deleter d;
  int n;
  scoped_ptr<int*, Deleter&> a(&n, d);
}

#endif
