// Copyright 2022 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_FLAG_SAVER_H_
#define PACKAGER_FLAG_SAVER_H_

#include <absl/flags/flag.h>

namespace shaka {

/// A replacement for gflags' FlagSaver, which is used in testing.
/// A FlagSaver is an RAII object to save and restore the values of
/// command-line flags during a test.  Unlike the gflags version, flags to be
/// saved and restored must be listed explicitly.
template <typename T>
class FlagSaver {
 public:
  FlagSaver(absl::Flag<T>* flag)
      : flag_(flag), original_value_(absl::GetFlag(*flag)) {}

  ~FlagSaver() { absl::SetFlag(flag_, original_value_); }

 private:
  absl::Flag<T>* flag_;  // unowned
  T original_value_;
};

}  // namespace shaka

#endif  // PACKAGER_FLAG_SAVER_H_
