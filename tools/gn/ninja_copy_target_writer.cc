// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_copy_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/string_utils.h"

NinjaCopyTargetWriter::NinjaCopyTargetWriter(const Target* target,
                                             std::ostream& out)
    : NinjaTargetWriter(target, out) {
}

NinjaCopyTargetWriter::~NinjaCopyTargetWriter() {
}

void NinjaCopyTargetWriter::Run() {
  // The dest dir should be inside the output dir so we can just remove the
  // prefix and get ninja-relative paths.
  const std::string& output_dir =
      settings_->build_settings()->build_dir().value();
  const std::string& dest_dir = target_->destdir().value();
  DCHECK(StartsWithASCII(dest_dir, output_dir, true));
  std::string relative_dest_dir(&dest_dir[output_dir.size()],
                                dest_dir.size() - output_dir.size());

  const Target::FileList& sources = target_->sources();
  std::vector<OutputFile> dest_files;
  dest_files.reserve(sources.size());

  // Write out rules for each file copied.
  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];

    // The files should have the same name but in the dest dir.
    base::StringPiece name_part = FindFilename(&input_file.value());
    OutputFile dest_file(relative_dest_dir);
    AppendStringPiece(&dest_file.value(), name_part);

    dest_files.push_back(dest_file);

    out_ << "build ";
    path_output_.WriteFile(out_, dest_file);
    out_ << ": copy ";
    path_output_.WriteFile(out_, input_file);
    out_ << std::endl;
  }

  // Write out the rule for the target to copy all of them.
  out_ << std::endl << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": stamp";
  for (size_t i = 0; i < dest_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, dest_files[i]);
  }
  out_ << std::endl;

  // TODO(brettw) need some kind of stamp file for depending on this, as well
  // as order_only=prebuild.
  // TODO(brettw) also need to write out the dependencies of this rule (maybe
  // we're copying output files around).
}
