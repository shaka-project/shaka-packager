// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_PUBLIC_EXPORT_H_
#define PACKAGER_PUBLIC_EXPORT_H_

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

#endif  // PACKAGER_PUBLIC_EXPORT_H_
