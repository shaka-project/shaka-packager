// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MACROS_H_
#define PACKAGER_MACROS_H_

#include <type_traits>

#include <absl/log/globals.h>
#include <absl/log/log.h>

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

#define VLOG(verboselevel) \
  LOG(LEVEL(static_cast<absl::LogSeverity>(-verboselevel)))

#define VLOG_IS_ON(verboselevel) \
  (static_cast<int>(absl::MinLogLevel()) <= -verboselevel)

#ifndef NDEBUG
#define DVLOG(verboselevel) VLOG(verboselevel)
#else
// We need this expression to work with << after it, so this is a simple way to
// turn DVLOG into a no-op in release builds.
#define DVLOG(verboselevel) if (false) VLOG(verboselevel)
#endif

#if defined(SHARED_LIBRARY_BUILD)
#if defined(_WIN32)

#if defined(SHAKA_IMPLEMENTATION)
#define SHAKA_EXPORT __declspec(dllexport)
#else
#define SHAKA_EXPORT __declspec(dllimport)
#endif  // defined(SHAKA_IMPLEMENTATION)

#else  // defined(_WIN32)

#if defined(SHAKA_IMPLEMENTATION)
#define SHAKA_EXPORT __attribute__((visibility("default")))
#else
#define SHAKA_EXPORT
#endif

#endif  // defined(_WIN32)

#else  // defined(SHARED_LIBRARY_BUILD)
#define SHAKA_EXPORT
#endif  // defined(SHARED_LIBRARY_BUILD)

#endif  // PACKAGER_MACROS_H_
