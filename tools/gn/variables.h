// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_VARIABLES_H_
#define TOOLS_GN_VARIABLES_H_

#include <map>

#include "base/strings/string_piece.h"

namespace variables {

// Builtin vars ----------------------------------------------------------------

extern const char kCurrentToolchain[];
extern const char kCurrentToolchain_HelpShort[];
extern const char kCurrentToolchain_Help[];

extern const char kDefaultToolchain[];
extern const char kDefaultToolchain_HelpShort[];
extern const char kDefaultToolchain_Help[];

extern const char kPythonPath[];
extern const char kPythonPath_HelpShort[];
extern const char kPythonPath_Help[];

extern const char kRelativeBuildToSourceRootDir[];
extern const char kRelativeBuildToSourceRootDir_HelpShort[];
extern const char kRelativeBuildToSourceRootDir_Help[];

extern const char kRelativeRootGenDir[];
extern const char kRelativeRootGenDir_HelpShort[];
extern const char kRelativeRootGenDir_Help[];

extern const char kRelativeRootOutputDir[];
extern const char kRelativeRootOutputDir_HelpShort[];
extern const char kRelativeRootOutputDir_Help[];

extern const char kRelativeTargetGenDir[];
extern const char kRelativeTargetGenDir_HelpShort[];
extern const char kRelativeTargetGenDir_Help[];

extern const char kRelativeTargetOutputDir[];
extern const char kRelativeTargetOutputDir_HelpShort[];
extern const char kRelativeTargetOutputDir_Help[];

// Target vars -----------------------------------------------------------------

extern const char kAllDependentConfigs[];
extern const char kAllDependentConfigs_HelpShort[];
extern const char kAllDependentConfigs_Help[];

extern const char kCflags[];
extern const char kCflags_HelpShort[];
extern const char* kCflags_Help;

extern const char kCflagsC[];
extern const char kCflagsC_HelpShort[];
extern const char* kCflagsC_Help;

extern const char kCflagsCC[];
extern const char kCflagsCC_HelpShort[];
extern const char* kCflagsCC_Help;

extern const char kCflagsObjC[];
extern const char kCflagsObjC_HelpShort[];
extern const char* kCflagsObjC_Help;

extern const char kCflagsObjCC[];
extern const char kCflagsObjCC_HelpShort[];
extern const char* kCflagsObjCC_Help;

extern const char kConfigs[];
extern const char kConfigs_HelpShort[];
extern const char kConfigs_Help[];

extern const char kDatadeps[];
extern const char kDatadeps_HelpShort[];
extern const char kDatadeps_Help[];

extern const char kDefines[];
extern const char kDefines_HelpShort[];
extern const char kDefines_Help[];

extern const char kDeps[];
extern const char kDeps_HelpShort[];
extern const char kDeps_Help[];

extern const char kDirectDependentConfigs[];
extern const char kDirectDependentConfigs_HelpShort[];
extern const char kDirectDependentConfigs_Help[];

extern const char kLdflags[];
extern const char kLdflags_HelpShort[];
extern const char kLdflags_Help[];

extern const char kSources[];
extern const char kSources_HelpShort[];
extern const char kSources_Help[];


// -----------------------------------------------------------------------------

struct VariableInfo {
  VariableInfo();
  VariableInfo(const char* in_help_short,
               const char* in_help);

  const char* help_short;
  const char* help;
};

typedef std::map<base::StringPiece, VariableInfo> VariableInfoMap;

// Returns the built-in readonly variables.
// Note: this is used only for help so this getter is not threadsafe.
const VariableInfoMap& GetBuiltinVariables();

// Returns the variables used by target generators.
// Note: this is used only for help so this getter is not threadsafe.
const VariableInfoMap& GetTargetVariables();

}  // namespace variables

#endif  // TOOLS_GN_VARIABLES_H_
