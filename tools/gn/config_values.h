// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_CONFIG_VALUES_H_
#define TOOLS_GN_CONFIG_VALUES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "tools/gn/source_dir.h"

// Holds the values (includes, defines, compiler flags, etc.) for a given
// config or target.
class ConfigValues {
 public:
  ConfigValues();
  ~ConfigValues();

  const std::vector<SourceDir>& includes() const { return includes_; }
  void swap_in_includes(std::vector<SourceDir>* lo) { includes_.swap(*lo); }

#define VALUES_ACCESSOR(name) \
    const std::vector<std::string>& name() const { return name##_; } \
    void swap_in_##name(std::vector<std::string>* v) { name##_.swap(*v); }

  VALUES_ACCESSOR(defines)
  VALUES_ACCESSOR(cflags)
  VALUES_ACCESSOR(cflags_c)
  VALUES_ACCESSOR(cflags_cc)
  VALUES_ACCESSOR(cflags_objc)
  VALUES_ACCESSOR(cflags_objcc)
  VALUES_ACCESSOR(ldflags)

#undef VALUES_ACCESSOR

 private:
  std::vector<SourceDir> includes_;
  std::vector<std::string> defines_;
  std::vector<std::string> cflags_;
  std::vector<std::string> cflags_c_;
  std::vector<std::string> cflags_cc_;
  std::vector<std::string> cflags_objc_;
  std::vector<std::string> cflags_objcc_;
  std::vector<std::string> ldflags_;

  DISALLOW_COPY_AND_ASSIGN(ConfigValues);
};

#endif  // TOOLS_GN_CONFIG_VALUES_H_
