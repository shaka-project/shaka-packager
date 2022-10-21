// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MACROS_H_
#define PACKAGER_MACROS_H_

#include <type_traits>

#include "absl/base/macros.h"

/// A macro to disable copying and assignment. Usage:
/// class Foo {
///  private:
///   DISALLOW_COPY_AND_ASSIGN(Foo);
/// }
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete;

/// ABSL_ARRAYSIZE works just like the arraysize macro we used to use from
/// Chromium.  To ease porting, define arraysize() as ABSL_ARRAYSIZE().
#define arraysize(a) ABSL_ARRAYSIZE(a)

/// A macro to declare that you intentionally did not use a parameter.  Useful
/// when implementing abstract interfaces.
#define UNUSED(x) (void)(x)

/// A macro to declare that you intentionally did not implement a method.
/// You can use the insertion operator to add specific logs to this.
#define NOTIMPLEMENTED() LOG(ERROR) << "NOTIMPLEMENTED: "

#endif  // PACKAGER_MACROS_H_
