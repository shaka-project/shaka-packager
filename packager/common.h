// Copyright 2022 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_COMMON_H_
#define PACKAGER_COMMON_H_

#include <type_traits>

namespace shaka {

/// A mix-in to disable copying and assignment.
/// Usage: class Foo : private DisallowCopyAndAssign
class DisallowCopyAndAssign {
 public:
  DisallowCopyAndAssign(const DisallowCopyAndAssign &) = delete;
  DisallowCopyAndAssign& operator=(const DisallowCopyAndAssign &) = delete;

 protected:
  DisallowCopyAndAssign() = default;
  ~DisallowCopyAndAssign() = default;
};

/// A work-alike for Chromium base's arraysize macro, but built on the C++11
/// standard-library's less-easy-to-understand std::extent template.
#define arraysize(a) std::extent<decltype(a)>::value

}  // namespace shaka

#endif  // PACKAGER_COMMON_H_
