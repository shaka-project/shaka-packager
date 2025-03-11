// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MACROS_CLASSES_H_
#define PACKAGER_MACROS_CLASSES_H_

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

#endif  // PACKAGER_MACROS_CLASSES_H_
