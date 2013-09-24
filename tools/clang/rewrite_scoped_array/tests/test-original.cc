// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

template <class T> class scoped_array {
 private:
  T* data_;
};

class TestClass {
 private:
  scoped_array<int> test_field_;
};

scoped_array<int> TestFunction(scoped_array<int> x, scoped_array<int>) {
  scoped_array<scoped_array<int>(scoped_array<int> test, scoped_array<int>)> y;
  scoped_array<int>(*function_pointer)(scoped_array<int> test,
                                       scoped_array<int>);
  scoped_array<int> test_variable;
}

