//  Copyright 2023 Google LLC. All rights reserved.
//
//  Use of this source code is governed by a BSD-style
//  license that can be found in the LICENSE file or at
//  https://developers.google.com/open-source/licenses/bsd

#ifndef SHAKA_PACKAGER_TEST_CLOCK_H
#define SHAKA_PACKAGER_TEST_CLOCK_H

#include <chrono>
#include <string>

#include <packager/utils/clock.h>

namespace shaka {

class TestClock : public Clock {
 public:
  explicit TestClock(std::string utc_time_8601);
  time_point now() noexcept override { return mock_time_; }

 private:
  time_point mock_time_;
};

}  // namespace shaka

#endif
