// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_MACROS_LOGGING_H_
#define PACKAGER_MACROS_LOGGING_H_

#include <absl/log/globals.h>
#include <absl/log/log.h>

/// A macro to declare that you intentionally did not implement a method.
/// You can use the insertion operator to add specific logs to this.
#define NOTIMPLEMENTED() LOG(ERROR) << "NOTIMPLEMENTED: "

#endif  // PACKAGER_MACROS_LOGGING_H_
