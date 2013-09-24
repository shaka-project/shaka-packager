// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCRIPT_TARGET_GENERATOR_H_
#define TOOLS_GN_SCRIPT_TARGET_GENERATOR_H_

#include "base/compiler_specific.h"
#include "tools/gn/target_generator.h"

// Populates a Target with the values from a custom script rule.
class ScriptTargetGenerator : public TargetGenerator {
 public:
  ScriptTargetGenerator(Target* target,
                      Scope* scope,
                      const Token& function_token,
                      Err* err);
  virtual ~ScriptTargetGenerator();

 protected:
  virtual void DoRun() OVERRIDE;

 private:
  void FillScript();
  void FillScriptArgs();
  void FillOutputs();

  DISALLOW_COPY_AND_ASSIGN(ScriptTargetGenerator);
};

#endif  // TOOLS_GN_SCRIPT_TARGET_GENERATOR_H_

