// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/settings.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "tools/gn/filesystem_utils.h"

Settings::Settings(const BuildSettings* build_settings,
                   const Toolchain* toolchain,
                   const std::string& output_subdir_name)
    : build_settings_(build_settings),
      toolchain_(toolchain),
      import_manager_(),
      base_config_(this),
      greedy_target_generation_(false) {
  DCHECK(output_subdir_name.find('/') == std::string::npos);
  if (output_subdir_name.empty()) {
    toolchain_output_dir_ = build_settings->build_dir();
  } else {
    // We guarantee this ends in a slash.
    toolchain_output_subdir_.value().append(output_subdir_name);
    toolchain_output_subdir_.value().push_back('/');

    toolchain_output_dir_ = SourceDir(build_settings->build_dir().value() +
                                      toolchain_output_subdir_.value());
  }
  // The output dir will be null in some tests and when invoked to parsed
  // one-off data without doing generation.
  if (!toolchain_output_dir_.is_null())
    toolchain_gen_dir_ = SourceDir(toolchain_output_dir_.value() + "gen/");


#if defined(OS_WIN)
  target_os_ = WIN;
#elif defined(OS_MACOSX)
  target_os_ = MAC;
#elif defined(OS_LINUX)
  target_os_ = LINUX;
#else
  #error implement me
#endif
}

Settings::~Settings() {
}


