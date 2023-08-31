// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MACROS_H_
#define PACKAGER_MACROS_H_

#include <type_traits>

/// A macro to disable copying and assignment. Usage:
/// class Foo {
///  private:
///   DISALLOW_COPY_AND_ASSIGN(Foo);
/// }
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete;

/// A macro to disable all implicit constructors (copy, assignment, and default
/// constructor). Usage:
/// class Foo {
///  private:
///   DISALLOW_IMPLICIT_CONSTRUCTORS(Foo);
/// }
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName);

/// A macro to declare that you intentionally did not use a parameter.  Useful
/// when implementing abstract interfaces.
#define UNUSED(x) (void)(x)

/// A macro to declare that you intentionally did not implement a method.
/// You can use the insertion operator to add specific logs to this.
#define NOTIMPLEMENTED() LOG(ERROR) << "NOTIMPLEMENTED: "

/// AES block size in bytes, regardless of key size.
#define AES_BLOCK_SIZE 16

#define DVLOG_IF(verboselevel, condition) \
  static_cast<void>(0), !(condition) ? (void)0 : VLOG(verboselevel)

#endif  // PACKAGER_MACROS_H_