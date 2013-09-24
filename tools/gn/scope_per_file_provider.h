// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_
#define TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/scope.h"
#include "tools/gn/source_file.h"

// ProgrammaticProvider for a scope to provide it with per-file built-in
// variable support.
class ScopePerFileProvider : public Scope::ProgrammaticProvider {
 public:
  ScopePerFileProvider(Scope* scope, const SourceFile& source_file);
  virtual ~ScopePerFileProvider();

  // ProgrammaticProvider implementation.
  virtual const Value* GetProgrammaticValue(
      const base::StringPiece& ident) OVERRIDE;

 private:
  const Value* GetCurrentToolchain();
  const Value* GetDefaultToolchain();
  const Value* GetPythonPath();
  const Value* GetRelativeBuildToSourceRootDir();
  const Value* GetRelativeRootOutputDir();
  const Value* GetRelativeRootGenDir();
  const Value* GetRelativeTargetOutputDir();
  const Value* GetRelativeTargetGenDir();

  static std::string GetRootOutputDirWithNoLastSlash(const Settings* settings);
  static std::string GetRootGenDirWithNoLastSlash(const Settings* settings);

  std::string GetFileDirWithNoLastSlash() const;
  std::string GetRelativeRootWithNoLastSlash() const;

  // Inverts the given directory, returning it with no trailing slash. If the
  // result would be empty, "." is returned to indicate the current dir.
  static std::string InvertDirWithNoLastSlash(const SourceDir& dir);

  SourceFile source_file_;

  // All values are lazily created.
  scoped_ptr<Value> current_toolchain_;
  scoped_ptr<Value> default_toolchain_;
  scoped_ptr<Value> python_path_;
  scoped_ptr<Value> relative_build_to_source_root_dir_;
  scoped_ptr<Value> relative_root_output_dir_;
  scoped_ptr<Value> relative_root_gen_dir_;
  scoped_ptr<Value> relative_target_output_dir_;
  scoped_ptr<Value> relative_target_gen_dir_;

  DISALLOW_COPY_AND_ASSIGN(ScopePerFileProvider);
};

#endif  // TOOLS_GN_SCOPE_PER_FILE_PROVIDER_H_
