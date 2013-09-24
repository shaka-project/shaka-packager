// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

template <class T> class scoped_array {
 private:
  T* data_;
};

class TestClass {
 private:
  scoped_ptr<int[]> test_field_;
};

scoped_ptr<int[]> TestFunction(scoped_ptr<int[]> x, scoped_ptr<int[]>) {
  scoped_ptr<scoped_array<int>(scoped_array<int> test, scoped_array<int>)[]> y;
  scoped_ptr<int[]>(*function_pointer)(scoped_ptr<int[]> test,
                                       scoped_ptr<int[]>);
  scoped_ptr<int[]> test_variable;
}

