// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"

void TestDeclarations() {
  scoped_ptr<int> a(NULL), b(new int), c(NULL);
  scoped_ptr_malloc<int> d(NULL);
}

void TestNew() {
  scoped_ptr<int>* a = new scoped_ptr<int>(NULL),
                   *b = new scoped_ptr<int>(new int),
                   *c = new scoped_ptr<int>(NULL);
}

class TestInitializers {
 public:
  TestInitializers() : a(NULL) {}
  TestInitializers(bool) : a(NULL), b(NULL), e(NULL) {}
  TestInitializers(double)
      : a(NULL), b(new int), c(), f(static_cast<int*>(malloc(sizeof(int)))) {}

 private:
  scoped_ptr<int> a;
  scoped_ptr<int> b;
  scoped_ptr<int> c;
  scoped_ptr_malloc<int> d;
  scoped_ptr_malloc<int> e;
  scoped_ptr_malloc<int> f;
};

scoped_ptr<int> TestTemporaries(scoped_ptr<int> a, scoped_ptr<int> b) {
  scoped_ptr<int> c =
      TestTemporaries(scoped_ptr<int>(NULL), scoped_ptr<int>(new int));
  return scoped_ptr<int>(NULL);
}

