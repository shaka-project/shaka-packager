// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <gtest/gtest.h>

#include "packager/base/at_exit.h"
#include "packager/base/command_line.h"
#include "packager/base/files/file_path.h"
#include "packager/base/logging.h"
#include "packager/base/path_service.h"

int main(int argc, char **argv) {
  base::AtExitManager exit;

  // Needed to enable VLOG/DVLOG through --vmodule or --v.
  base::CommandLine::Init(argc, argv);

  // Set up logging.
  logging::LoggingSettings log_settings;
  log_settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  CHECK(logging::InitLogging(log_settings));

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
