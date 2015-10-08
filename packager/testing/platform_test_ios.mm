// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform_test.h"

#import <Foundation/Foundation.h>

#include "coverage_util_ios.h"

PlatformTest::PlatformTest()
    : pool_([[NSAutoreleasePool alloc] init]) {
}

PlatformTest::~PlatformTest() {
  [pool_ release];
  coverage_util::FlushCoverageDataIfNecessary();
}
