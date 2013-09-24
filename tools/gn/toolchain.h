// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TOOLCHAIN_H_
#define TOOLS_GN_TOOLCHAIN_H_

#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"
#include "tools/gn/item.h"

// Holds information on a specific toolchain. This data is filled in when we
// encounter a toolchain definition.
//
// This class is an Item so it can participate in dependency management. In
// particular, when a target uses a toolchain, it should have a dependency on
// that toolchain's object so that we can be sure we loaded the toolchain
// before generating the build for that target.
//
// Note on threadsafety: The label of the toolchain never changes so can
// safetly be accessed from any thread at any time (we do this when asking for
// the toolchain name). But the values in the toolchain do, so these can't
// be accessed until this Item is resolved.
class Toolchain : public Item {
 public:
  enum ToolType {
    TYPE_NONE = 0,
    TYPE_CC,
    TYPE_CXX,
    TYPE_OBJC,
    TYPE_OBJCXX,
    TYPE_ASM,
    TYPE_ALINK,
    TYPE_SOLINK,
    TYPE_LINK,
    TYPE_STAMP,
    TYPE_COPY,

    TYPE_NUMTYPES  // Must be last.
  };

  static const char* kToolCc;
  static const char* kToolCxx;
  static const char* kToolObjC;
  static const char* kToolObjCxx;
  static const char* kToolAsm;
  static const char* kToolAlink;
  static const char* kToolSolink;
  static const char* kToolLink;
  static const char* kToolStamp;
  static const char* kToolCopy;

  struct Tool {
    Tool();
    ~Tool();

    bool empty() const {
      return command.empty() && depfile.empty() && deps.empty() &&
             description.empty() && pool.empty() && restat.empty() &&
             rspfile.empty() && rspfile_content.empty();
    }

    std::string command;
    std::string depfile;
    std::string deps;
    std::string description;
    std::string pool;
    std::string restat;
    std::string rspfile;
    std::string rspfile_content;
  };

  Toolchain(const Label& label);
  virtual ~Toolchain();

  // Item overrides.
  virtual Toolchain* AsToolchain() OVERRIDE;
  virtual const Toolchain* AsToolchain() const OVERRIDE;

  // Returns TYPE_NONE on failure.
  static ToolType ToolNameToType(const base::StringPiece& str);
  static std::string ToolTypeToName(ToolType type);

  const Tool& GetTool(ToolType type) const;
  void SetTool(ToolType type, const Tool& t);

  const std::string& environment() const { return environment_; }
  void set_environment(const std::string& env) { environment_ = env; }

 private:
  Tool tools_[TYPE_NUMTYPES];

  std::string environment_;
};

#endif  // TOOLS_GN_TOOLCHAIN_H_
