// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/scoped_sending_event.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Sets the flag within scope, resets when leaving scope.
TEST(ScopedSendingEventTest, SetHandlingSendEvent) {
  id<CrAppProtocol> app = NSApp;
  EXPECT_FALSE([app isHandlingSendEvent]);
  {
    base::mac::ScopedSendingEvent is_handling_send_event;
    EXPECT_TRUE([app isHandlingSendEvent]);
  }
  EXPECT_FALSE([app isHandlingSendEvent]);
}

// Nested call restores previous value rather than resetting flag.
TEST(ScopedSendingEventTest, NestedSetHandlingSendEvent) {
  id<CrAppProtocol> app = NSApp;
  EXPECT_FALSE([app isHandlingSendEvent]);
  {
    base::mac::ScopedSendingEvent is_handling_send_event;
    EXPECT_TRUE([app isHandlingSendEvent]);
    {
      base::mac::ScopedSendingEvent nested_is_handling_send_event;
      EXPECT_TRUE([app isHandlingSendEvent]);
    }
    EXPECT_TRUE([app isHandlingSendEvent]);
  }
  EXPECT_FALSE([app isHandlingSendEvent]);
}

}  // namespace
