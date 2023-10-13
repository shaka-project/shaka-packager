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

#define VLOG(verboselevel) \
  LOG(LEVEL(static_cast<absl::LogSeverity>(-verboselevel)))

#define VLOG_IS_ON(verboselevel) \
  (static_cast<int>(absl::MinLogLevel()) <= -verboselevel)

#ifndef NDEBUG
#define DVLOG(verboselevel) VLOG(verboselevel)
#else
// We need this expression to work with << after it, so this is a simple way to
// turn DVLOG into a no-op in release builds.
#define DVLOG(verboselevel) \
  if (false)                \
  VLOG(verboselevel)
#endif

#endif  // PACKAGER_MACROS_LOGGING_H_
