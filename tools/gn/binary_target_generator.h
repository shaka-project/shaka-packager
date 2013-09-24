// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_BINARY_TARGET_GENERATOR_H_
#define TOOLS_GN_BINARY_TARGET_GENERATOR_H_

#include "base/compiler_specific.h"
#include "tools/gn/target_generator.h"

// Populates a Target with the values from a binary rule (executable, shared
// library, or static library).
class BinaryTargetGenerator : public TargetGenerator {
 public:
  BinaryTargetGenerator(Target* target,
                        Scope* scope,
                        const Token& function_token,
                        Target::OutputType type,
                        Err* err);
  virtual ~BinaryTargetGenerator();

 protected:
  virtual void DoRun() OVERRIDE;

 private:
  Target::OutputType output_type_;

  DISALLOW_COPY_AND_ASSIGN(BinaryTargetGenerator);
};

#endif  // TOOLS_GN_BINARY_TARGET_GENERATOR_H_

