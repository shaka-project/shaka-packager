// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <packager/utils/clock.h>

namespace shaka {

Clock::time_point Clock::now() noexcept {
  return std::chrono::system_clock::now();
}

}  // namespace shaka
