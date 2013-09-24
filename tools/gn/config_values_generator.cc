// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/config_values_generator.h"

#include "tools/gn/config_values.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"

namespace {

void GetStringList(
    Scope* scope,
    const char* var_name,
    ConfigValues* config_values,
    void (ConfigValues::* swapper_inner)(std::vector<std::string>*),
    Err* err) {
  const Value* value = scope->GetValue(var_name, true);
  if (!value)
    return;  // No value, empty input and succeed.

  std::vector<std::string> result;
  ExtractListOfStringValues(*value, &result, err);
  (config_values->*swapper_inner)(&result);
}

}  // namespace

ConfigValuesGenerator::ConfigValuesGenerator(ConfigValues* dest_values,
                                             Scope* scope,
                                             const Token& function_token,
                                             const SourceDir& input_dir,
                                             Err* err)
    : config_values_(dest_values),
      scope_(scope),
      function_token_(function_token),
      input_dir_(input_dir),
      err_(err) {
}

ConfigValuesGenerator::~ConfigValuesGenerator() {
}

void ConfigValuesGenerator::Run() {
  FillIncludes();

#define FILL_CONFIG_VALUE(name) \
    GetStringList(scope_, #name, config_values_, \
                  &ConfigValues::swap_in_##name, err_);

  FILL_CONFIG_VALUE(defines)
  FILL_CONFIG_VALUE(cflags)
  FILL_CONFIG_VALUE(cflags_c)
  FILL_CONFIG_VALUE(cflags_cc)
  FILL_CONFIG_VALUE(cflags_objc)
  FILL_CONFIG_VALUE(cflags_objcc)
  FILL_CONFIG_VALUE(ldflags)

#undef FILL_CONFIG_VALUE
}

void ConfigValuesGenerator::FillIncludes() {
  const Value* value = scope_->GetValue("includes", true);
  if (!value)
    return;  // No value, empty input and succeed.

  std::vector<SourceDir> includes;
  if (!ExtractListOfRelativeDirs(*value, input_dir_, &includes, err_))
    return;
  config_values_->swap_in_includes(&includes);
}
