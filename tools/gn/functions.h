// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_FUNCTIONS_H_
#define TOOLS_GN_FUNCTIONS_H_

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/strings/string_piece.h"

class Err;
class BlockNode;
class FunctionCallNode;
class Label;
class ListNode;
class ParseNode;
class Scope;
class SourceDir;
class Token;
class Value;

// -----------------------------------------------------------------------------

namespace functions {

// This type of function invocation takes a block node that it will execute.
typedef Value (*GenericBlockFunction)(Scope* scope,
                                      const FunctionCallNode* function,
                                      const std::vector<Value>& args,
                                      BlockNode* block,
                                      Err* err);

// This type of function takes a block, but does not need to control execution
// of it. The dispatch function will pre-execute the block and pass the
// resulting block_scope to the function.
typedef Value(*ExecutedBlockFunction)(const FunctionCallNode* function,
                                      const std::vector<Value>& args,
                                      Scope* block_scope,
                                      Err* err);

// This type of function does not take a block. It just has arguments.
typedef Value (*NoBlockFunction)(Scope* scope,
                                 const FunctionCallNode* function,
                                 const std::vector<Value>& args,
                                 Err* err);

extern const char kAssert[];
extern const char kAssert_Help[];
Value RunAssert(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err);

extern const char kComponent[];
extern const char kComponent_Help[];
Value RunComponent(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err);

extern const char kConfig[];
extern const char kConfig_Help[];
Value RunConfig(const FunctionCallNode* function,
                const std::vector<Value>& args,
                Scope* block_scope,
                Err* err);

extern const char kCopy[];
extern const char kCopy_Help[];
Value RunCopy(const FunctionCallNode* function,
              const std::vector<Value>& args,
              Scope* block_scope,
              Err* err);

extern const char kCustom[];
extern const char kCustom_Help[];
Value RunCustom(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                BlockNode* block,
                Err* err);

extern const char kDeclareArgs[];
extern const char kDeclareArgs_Help[];
Value RunDeclareArgs(const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     Scope* block_scope,
                     Err* err);

extern const char kExecScript[];
extern const char kExecScript_Help[];
Value RunExecScript(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    Err* err);

extern const char kExecutable[];
extern const char kExecutable_Help[];
Value RunExecutable(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    BlockNode* block,
                    Err* err);

extern const char kGroup[];
extern const char kGroup_Help[];
Value RunGroup(Scope* scope,
               const FunctionCallNode* function,
               const std::vector<Value>& args,
               BlockNode* block,
               Err* err);

extern const char kImport[];
extern const char kImport_Help[];
Value RunImport(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err);

extern const char kPrint[];
extern const char kPrint_Help[];
Value RunPrint(Scope* scope,
               const FunctionCallNode* function,
               const std::vector<Value>& args,
               Err* err);

extern const char kProcessFileTemplate[];
extern const char kProcessFileTemplate_Help[];
Value RunProcessFileTemplate(Scope* scope,
                             const FunctionCallNode* function,
                             const std::vector<Value>& args,
                             Err* err);

extern const char kReadFile[];
extern const char kReadFile_Help[];
Value RunReadFile(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  Err* err);

extern const char kSetDefaults[];
extern const char kSetDefaults_Help[];
Value RunSetDefaults(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     BlockNode* block,
                     Err* err);

extern const char kSetDefaultToolchain[];
extern const char kSetDefaultToolchain_Help[];
Value RunSetDefaultToolchain(Scope* scope,
                             const FunctionCallNode* function,
                             const std::vector<Value>& args,
                             Err* err);

extern const char kSetSourcesAssignmentFilter[];
extern const char kSetSourcesAssignmentFilter_Help[];
Value RunSetSourcesAssignmentFilter(Scope* scope,
                                    const FunctionCallNode* function,
                                    const std::vector<Value>& args,
                                    Err* err);

extern const char kSharedLibrary[];
extern const char kSharedLibrary_Help[];
Value RunSharedLibrary(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err);

extern const char kStaticLibrary[];
extern const char kStaticLibrary_Help[];
Value RunStaticLibrary(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err);

extern const char kTemplate[];
extern const char kTemplate_Help[];
Value RunTemplate(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  BlockNode* block,
                  Err* err);

extern const char kTest[];
extern const char kTest_Help[];
Value RunTest(Scope* scope,
              const FunctionCallNode* function,
              const std::vector<Value>& args,
              BlockNode* block,
              Err* err);

extern const char kTool[];
extern const char kTool_Help[];
Value RunTool(Scope* scope,
              const FunctionCallNode* function,
              const std::vector<Value>& args,
              BlockNode* block,
              Err* err);

extern const char kToolchain[];
extern const char kToolchain_Help[];
Value RunToolchain(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err);

extern const char kWriteFile[];
extern const char kWriteFile_Help[];
Value RunWriteFile(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   Err* err);

// -----------------------------------------------------------------------------

// One function record. Only one of the given runner types will be non-null
// which indicates the type of function it is.
struct FunctionInfo {
  FunctionInfo();
  FunctionInfo(GenericBlockFunction gbf, const char* in_help);
  FunctionInfo(ExecutedBlockFunction ebf, const char* in_help);
  FunctionInfo(NoBlockFunction nbf, const char* in_help);

  GenericBlockFunction generic_block_runner;
  ExecutedBlockFunction executed_block_runner;
  NoBlockFunction no_block_runner;

  const char* help;
};

typedef base::hash_map<base::StringPiece, FunctionInfo> FunctionInfoMap;

// Returns the mapping of all built-in functions.
const FunctionInfoMap& GetFunctions();

// Runs the given function.
Value RunFunction(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  BlockNode* block,  // Optional.
                  Err* err);

}  // namespace functions

// Helper functions -----------------------------------------------------------

// Verifies that the current scope is not processing an import. If it is, it
// will set the error, blame the given parse node for it, and return false.
bool EnsureNotProcessingImport(const ParseNode* node,
                               const Scope* scope,
                               Err* err);

// Like EnsureNotProcessingImport but checks for running the build config.
bool EnsureNotProcessingBuildConfig(const ParseNode* node,
                                    const Scope* scope,
                                    Err* err);

// Sets up the |block_scope| for executing a target (or something like it).
// The |scope| is the containing scope. It should have been already set as the
// parent for the |block_scope| when the |block_scope| was created.
//
// This will set up the target defaults and set the |target_name| variable in
// the block scope to the current target name, which is assumed to be the first
// argument to the function.
//
// On success, returns true. On failure, sets the error and returns false.
bool FillTargetBlockScope(const Scope* scope,
                          const FunctionCallNode* function,
                          const char* target_type,
                          const BlockNode* block,
                          const std::vector<Value>& args,
                          Scope* block_scope,
                          Err* err);

// Validates that the given function call has one string argument. This is
// the most common function signature, so it saves space to have this helper.
// Returns false and sets the error on failure.
bool EnsureSingleStringArg(const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           Err* err);

// Returns the source directory for the file comtaining the given function
// invocation.
const SourceDir& SourceDirForFunctionCall(const FunctionCallNode* function);

// Returns the name of the toolchain for the given scope.
const Label& ToolchainLabelForScope(const Scope* scope);

// Generates a label for the given scope, using the current directory and
// toolchain, and the given name.
Label MakeLabelForScope(const Scope* scope,
                        const FunctionCallNode* function,
                        const std::string& name);

#endif  // TOOLS_GN_FUNCTIONS_H_
