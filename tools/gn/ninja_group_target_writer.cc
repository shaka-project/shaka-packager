// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_group_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/string_utils.h"

NinjaGroupTargetWriter::NinjaGroupTargetWriter(const Target* target,
                                               std::ostream& out)
    : NinjaTargetWriter(target, out) {
}

NinjaGroupTargetWriter::~NinjaGroupTargetWriter() {
}

void NinjaGroupTargetWriter::Run() {
  // A group rule just generates a stamp file with dependencies on each of
  // the deps in the group.
  out_ << std::endl << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": stamp";

  const std::vector<const Target*>& deps = target_->deps();
  for (size_t i = 0; i < deps.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, helper_.GetTargetOutputFile(deps[i]));
  }

  const std::vector<const Target*>& datadeps = target_->datadeps();
  for (size_t i = 0; i < datadeps.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, helper_.GetTargetOutputFile(datadeps[i]));
  }
  out_ << std::endl;
}
