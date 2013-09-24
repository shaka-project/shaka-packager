// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/script_target_generator.h"

#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"

ScriptTargetGenerator::ScriptTargetGenerator(Target* target,
                                             Scope* scope,
                                             const Token& function_token,
                                             Err* err)
    : TargetGenerator(target, scope, function_token, err) {
}

ScriptTargetGenerator::~ScriptTargetGenerator() {
}

void ScriptTargetGenerator::DoRun() {
  target_->set_output_type(Target::CUSTOM);

  FillSources();
  FillScript();
  FillScriptArgs();
  FillOutputs();

  // Script outputs don't depend on the current toolchain so we can skip adding
  // that dependency.
}

void ScriptTargetGenerator::FillScript() {
  // If this gets called, the target type requires a script, so error out
  // if it doesn't have one.
  // TODO(brettw) hook up a constant in variables.h
  const Value* value = scope_->GetValue("script", true);
  if (!value) {
    *err_ = Err(function_token_, "This target type requires a \"script\".");
    return;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;

  target_->script_values().set_script(
      input_directory_.ResolveRelativeFile(value->string_value()));
}

void ScriptTargetGenerator::FillScriptArgs() {
  const Value* value = scope_->GetValue("args", true);
  if (!value)
    return;

  std::vector<std::string> args;
  if (!ExtractListOfStringValues(*value, &args, err_))
    return;
  target_->script_values().swap_in_args(&args);
}

void ScriptTargetGenerator::FillOutputs() {
  // TODO(brettw) hook up a constant in variables.h
  const Value* value = scope_->GetValue("outputs", true);
  if (!value)
    return;

  Target::FileList outputs;
  if (!ExtractListOfRelativeFiles(*value, input_directory_, &outputs, err_))
    return;

  // Validate that outputs are in the output dir.
  CHECK(outputs.size() == value->list_value().size());
  for (size_t i = 0; i < outputs.size(); i++) {
    if (!EnsureStringIsInOutputDir(
            GetBuildSettings()->build_dir(),
            outputs[i].value(), value->list_value()[i], err_))
      return;
  }
  target_->script_values().swap_in_outputs(&outputs);
}

