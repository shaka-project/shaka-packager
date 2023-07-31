//  Copyright 2023 Google LLC. All rights reserved.
//
//  Use of this source code is governed by a BSD-style
//  license that can be found in the LICENSE file or at
//  https://developers.google.com/open-source/licenses/bsd

#ifndef SHAKA_PACKAGER_CLOCK_H
#define SHAKA_PACKAGER_CLOCK_H

#include <chrono>

namespace shaka {

class Clock {
 public:
  using time_point = std::chrono::system_clock::time_point;

  virtual ~Clock() = default;

  virtual time_point now() noexcept;
};

}  // namespace shaka

#endif  // SHAKA_PACKAGER_CLOCK_H
