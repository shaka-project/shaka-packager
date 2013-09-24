// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/hi_res_timer_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

#if defined(OS_WIN)
// http://crbug.com/114048
TEST(HiResTimerManagerTest, DISABLED_ToggleOnOff) {
  base::MessageLoop loop;
  scoped_ptr<base::PowerMonitorSource> power_monitor_source(
      new base::PowerMonitorDeviceSource());
  scoped_ptr<base::PowerMonitor> power_monitor(
      new base::PowerMonitor(power_monitor_source.Pass()));
  HighResolutionTimerManager manager;

  // At this point, we don't know if the high resolution timers are on or off,
  // it depends on what system the tests are running on (for example, if this
  // test is running on a laptop/battery, then the PowerMonitor would have
  // already set the PowerState to battery power; but if we're running on a
  // desktop, then the PowerState will be non-battery power).  Simulate a power
  // level change to get to a deterministic state.
  manager.OnPowerStateChange(/* on_battery */ false);

  // Loop a few times to test power toggling.
  for (int loop = 2; loop >= 0; --loop) {
    // The manager has the high resolution clock enabled now.
    EXPECT_TRUE(manager.hi_res_clock_available());
    // But the Time class has it off, because it hasn't been activated.
    EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

    // Activate the high resolution timer.
    base::Time::ActivateHighResolutionTimer(true);
    EXPECT_TRUE(base::Time::IsHighResolutionTimerInUse());

    // Simulate a on-battery power event.
    manager.OnPowerStateChange(/* on_battery */ true);
    EXPECT_FALSE(manager.hi_res_clock_available());
    EXPECT_FALSE(base::Time::IsHighResolutionTimerInUse());

    // Simulate a off-battery power event.
    manager.OnPowerStateChange(/* on_battery */ false);
    EXPECT_TRUE(manager.hi_res_clock_available());
    EXPECT_TRUE(base::Time::IsHighResolutionTimerInUse());

    // De-activate the high resolution timer.
    base::Time::ActivateHighResolutionTimer(false);
  }
}
#endif  // defined(OS_WIN)

}  // namespace base
