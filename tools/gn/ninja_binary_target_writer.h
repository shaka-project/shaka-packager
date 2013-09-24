// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

#include "base/compiler_specific.h"
#include "tools/gn/ninja_target_writer.h"

// Writes a .ninja file for a binary target type (an executable, a shared
// library, or a static library).
class NinjaBinaryTargetWriter : public NinjaTargetWriter {
 public:
  NinjaBinaryTargetWriter(const Target* target, std::ostream& out);
  virtual ~NinjaBinaryTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  void WriteCompilerVars();
  void WriteSources(std::vector<OutputFile>* object_files);
  void WriteLinkerStuff(const std::vector<OutputFile>& object_files);

  // Writes the build line for linking the target. Includes newline.
  void WriteLinkCommand(const OutputFile& external_output_file,
                        const OutputFile& internal_output_file,
                        const std::vector<OutputFile>& object_files);

  // Returns NULL if the source type should not be compiled on this target.
  const char* GetCommandForSourceType(SourceFileType type) const;

  const char* GetCommandForTargetType() const;

  DISALLOW_COPY_AND_ASSIGN(NinjaBinaryTargetWriter);
};

#endif  // TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

