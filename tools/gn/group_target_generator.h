// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_GROUP_TARGET_GENERATOR_H_
#define TOOLS_GN_GROUP_TARGET_GENERATOR_H_

#include "base/compiler_specific.h"
#include "tools/gn/target_generator.h"

// Populates a Target with the values for a group rule.
class GroupTargetGenerator : public TargetGenerator {
 public:
  GroupTargetGenerator(Target* target,
                        Scope* scope,
                        const Token& function_token,
                        Err* err);
  virtual ~GroupTargetGenerator();

 protected:
  virtual void DoRun() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GroupTargetGenerator);
};

#endif  // TOOLS_GN_GROUP_TARGET_GENERATOR_H_

