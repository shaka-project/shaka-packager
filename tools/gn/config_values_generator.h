// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_CONFIG_VALUES_GENERATOR_H_
#define TOOLS_GN_CONFIG_VALUES_GENERATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "tools/gn/source_dir.h"

class ConfigValues;
class Err;
class Scope;
class Token;

class ConfigValuesGenerator {
 public:
  ConfigValuesGenerator(ConfigValues* dest_values,
                        Scope* scope,
                        const Token& function_token,
                        const SourceDir& input_dir,
                        Err* err);
  ~ConfigValuesGenerator();

  // Sets the error passed to the constructor on failure.
  void Run();

 private:
  void FillIncludes();

  ConfigValues* config_values_;
  Scope* scope_;
  const Token& function_token_;
  const SourceDir input_dir_;
  Err* err_;

  DISALLOW_COPY_AND_ASSIGN(ConfigValuesGenerator);
};

#endif  // TOOLS_GN_CONFIG_VALUES_GENERATOR_H_
