// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_STANDARD_OUT_H_
#define TOOLS_GN_STANDARD_OUT_H_

#include <string>

enum TextDecoration {
  DECORATION_NONE = 0,
  DECORATION_BOLD,
  DECORATION_RED,
  DECORATION_GREEN,
  DECORATION_BLUE,
  DECORATION_YELLOW
};

void OutputString(const std::string& output,
                  TextDecoration dec = DECORATION_NONE);

#endif  // TOOLS_GN_STANDARD_OUT_H_
