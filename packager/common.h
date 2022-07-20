// Copyright 2022 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_COMMON_H_
#define PACKAGER_COMMON_H_

#include <type_traits>

#include "absl/base/macros.h"

namespace shaka {

/// A mix-in to disable copying and assignment.
/// Usage: class Foo : private DisallowCopyAndAssign
class DisallowCopyAndAssign {
 public:
  DisallowCopyAndAssign(const DisallowCopyAndAssign&) = delete;
  DisallowCopyAndAssign& operator=(const DisallowCopyAndAssign&) = delete;

 protected:
  DisallowCopyAndAssign() = default;
  ~DisallowCopyAndAssign() = default;
};

/// ABSL_ARRAYSIZE works just like the arraysize macro we used to use from
/// Chromium.  To ease porting, define arraysize() as ABSL_ARRAYSIZE().
#define arraysize(a) ABSL_ARRAYSIZE(a)

/// A macro to declare that you intentionally did not use a parameter.  Useful
/// when implementing abstract interfaces.
#define UNUSED(x) (void)(x)

}  // namespace shaka

#endif  // PACKAGER_COMMON_H_
