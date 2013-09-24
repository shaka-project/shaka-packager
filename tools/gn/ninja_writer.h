// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_WRITER_H_
#define TOOLS_GN_NINJA_WRITER_H_

#include "base/basictypes.h"

class BuildSettings;

class NinjaWriter {
 public:
  // On failure will print an error and will return false.
  static bool RunAndWriteFiles(const BuildSettings* build_settings);

 private:
  NinjaWriter(const BuildSettings* build_settings);
  ~NinjaWriter();

  bool WriteRootBuildfiles();

  const BuildSettings* build_settings_;

  DISALLOW_COPY_AND_ASSIGN(NinjaWriter);
};

#endif  // TOOLS_GN_NINJA_WRITER_H_
