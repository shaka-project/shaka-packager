// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/copy_target_generator.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

CopyTargetGenerator::CopyTargetGenerator(Target* target,
                                         Scope* scope,
                                         const Token& function_token,
                                         Err* err)
    : TargetGenerator(target, scope, function_token, err) {
}

CopyTargetGenerator::~CopyTargetGenerator() {
}

void CopyTargetGenerator::DoRun() {
  target_->set_output_type(Target::COPY_FILES);

  FillSources();
  FillDestDir();

  SetToolchainDependency();
}

void CopyTargetGenerator::FillDestDir() {
  // Destdir is required for all targets that use it.
  const Value* value = scope_->GetValue("destdir", true);
  if (!value) {
    *err_ = Err(function_token_, "This target type requires a \"destdir\".");
    return;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;

  if (!EnsureStringIsInOutputDir(
          GetBuildSettings()->build_dir(),
          value->string_value(), *value, err_))
    return;
  target_->set_destdir(SourceDir(value->string_value()));
}

