// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/group_target_generator.h"

GroupTargetGenerator::GroupTargetGenerator(Target* target,
                                           Scope* scope,
                                           const Token& function_token,
                                           Err* err)
    : TargetGenerator(target, scope, function_token, err) {
}

GroupTargetGenerator::~GroupTargetGenerator() {
}

void GroupTargetGenerator::DoRun() {
  target_->set_output_type(Target::GROUP);
  // Groups only have the default types filled in by the target generator
  // base class.
}
