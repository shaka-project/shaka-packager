// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license tha can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  // Needed to enable VLOG/DVLOG through --vmodule or --v.
  CommandLine::Init(argc, argv);
  CHECK(logging::InitLogging(logging::LoggingSettings()));

  base::AtExitManager exit;
  return RUN_ALL_TESTS();
}
