// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include <iostream>

#include "base/strings/string_util.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"

namespace {

void FillNeedsBlockError(const FunctionCallNode* function, Err* err) {
  *err = Err(function->function(), "This function call requires a block.",
      "The block's \"{\" must be on the same line as the function "
      "call's \")\".");
}

// This is called when a template is invoked. When we see a template
// declaration, that funciton is RunTemplate.
Value RunTemplateInvocation(Scope* scope,
                            const FunctionCallNode* invocation,
                            const std::vector<Value>& args,
                            BlockNode* block,
                            const FunctionCallNode* rule,
                            Err* err) {
  if (!EnsureNotProcessingImport(invocation, scope, err))
    return Value();
  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, invocation,
                            invocation->function().value().data(),
                            block, args, &block_scope, err))
    return Value();

  // Run the block for the rule invocation.
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Now run the rule itself with that block as the current scope.
  rule->block()->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  return Value();
}

}  // namespace

// ----------------------------------------------------------------------------

bool EnsureNotProcessingImport(const ParseNode* node,
                               const Scope* scope,
                               Err* err) {
  if (scope->IsProcessingImport()) {
    *err = Err(node, "Not valid from an import.",
        "We need to talk about this thing you are doing here. Doing this\n"
        "kind of thing from an imported file makes me feel like you are\n"
        "abusing me. Imports are for defining defaults, variables, and rules.\n"
        "The appropriate place for this kind of thing is really in a normal\n"
        "BUILD file.");
    return false;
  }
  return true;
}

bool EnsureNotProcessingBuildConfig(const ParseNode* node,
                                    const Scope* scope,
                                    Err* err) {
  if (scope->IsProcessingBuildConfig()) {
    *err = Err(node, "Not valid from the build config.",
        "You can't do this kind of thing from the build config script, "
        "silly!\nPut it in a regular BUILD file.");
    return false;
  }
  return true;
}

bool FillTargetBlockScope(const Scope* scope,
                          const FunctionCallNode* function,
                          const char* target_type,
                          const BlockNode* block,
                          const std::vector<Value>& args,
                          Scope* block_scope,
                          Err* err) {
  if (!block) {
    FillNeedsBlockError(function, err);
    return false;
  }

  // Copy the target defaults, if any, into the scope we're going to execute
  // the block in.
  const Scope* default_scope = scope->GetTargetDefaults(target_type);
  if (default_scope) {
    if (!default_scope->NonRecursiveMergeTo(block_scope, function,
                                            "target defaults", err))
      return false;
  }

  // The name is the single argument to the target function.
  if (!EnsureSingleStringArg(function, args, err))
    return false;

  // Set the target name variable to the current target, and mark it used
  // because we don't want to issue an error if the script ignores it.
  const base::StringPiece target_name("target_name");
  block_scope->SetValue(target_name, Value(function, args[0].string_value()),
                        function);
  block_scope->MarkUsed(target_name);
  return true;
}

bool EnsureSingleStringArg(const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "Incorrect arguments.",
               "This function requires a single string argument.");
    return false;
  }
  return args[0].VerifyTypeIs(Value::STRING, err);
}

const SourceDir& SourceDirForFunctionCall(const FunctionCallNode* function) {
  return function->function().location().file()->dir();
}

const Label& ToolchainLabelForScope(const Scope* scope) {
  return scope->settings()->toolchain()->label();
}

Label MakeLabelForScope(const Scope* scope,
                        const FunctionCallNode* function,
                        const std::string& name) {
  const SourceDir& input_dir = SourceDirForFunctionCall(function);
  const Label& toolchain_label = ToolchainLabelForScope(scope);
  return Label(input_dir, name, toolchain_label.dir(), toolchain_label.name());
}

namespace functions {

// assert ----------------------------------------------------------------------

const char kAssert[] = "assert";
const char kAssert_Help[] =
    "TODO(brettw) WRITE ME";

Value RunAssert(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err) {
  if (args.size() != 1) {
    *err = Err(function->function(), "Wrong number of arguments.",
               "assert() takes one argument, "
               "were you expecting somethig else?");
  } else if (args[0].InterpretAsInt() == 0) {
    *err = Err(function->function(), "Assertion failed.");
    if (args[0].origin()) {
      // If you do "assert(foo)" we'd ideally like to show you where foo was
      // set, and in this case the origin of the args will tell us that.
      // However, if you do "assert(foo && bar)" the source of the value will
      // be the assert like, which isn't so helpful.
      //
      // So we try to see if the args are from the same line or not. This will
      // break if you do "assert(\nfoo && bar)" and we may show the second line
      // as the source, oh well. The way around this is to check to see if the
      // origin node is inside our function call block.
      Location origin_location = args[0].origin()->GetRange().begin();
      if (origin_location.file() != function->function().location().file() ||
          origin_location.line_number() !=
              function->function().location().line_number()) {
        err->AppendSubErr(Err(args[0].origin()->GetRange(), "",
                              "This is where it was set."));
      }
    }
  }
  return Value();
}

// config ----------------------------------------------------------------------

const char kConfig[] = "config";
const char kConfig_Help[] =
    "TODO(brettw) write this.";

Value RunConfig(const FunctionCallNode* function,
                const std::vector<Value>& args,
                Scope* scope,
                Err* err) {
  if (!EnsureSingleStringArg(function, args, err) ||
      !EnsureNotProcessingImport(function, scope, err))
    return Value();

  Label label(MakeLabelForScope(scope, function, args[0].string_value()));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Generating config", label.GetUserVisibleName(true));

  // Create the empty config object.
  ItemTree* tree = &scope->settings()->build_settings()->item_tree();
  Config* config = Config::GetConfig(scope->settings(), function->GetRange(),
                                     label, NULL, err);
  if (err->has_error())
    return Value();

  // Fill it.
  const SourceDir input_dir = SourceDirForFunctionCall(function);
  ConfigValuesGenerator gen(&config->config_values(), scope,
                            function->function(), input_dir, err);
  gen.Run();
  if (err->has_error())
    return Value();

  // Mark as complete.
  {
    base::AutoLock lock(tree->lock());
    tree->MarkItemDefinedLocked(scope->settings()->build_settings(), label,
                                err);
  }
  return Value();
}

// declare_args ----------------------------------------------------------------

const char kDeclareArgs[] = "declare_args";
const char kDeclareArgs_Help[] =
    "TODO(brettw) write this.";

Value RunDeclareArgs(const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     Scope* scope,
                     Err* err) {
  // Only allow this to be called once. We use a variable in the current scope
  // with a name the parser will reject if the user tried to type it.
  const char did_declare_args_var[] = "@@declared_args";
  if (scope->GetValue(did_declare_args_var)) {
    *err = Err(function->function(), "Duplicate call to declared_args.");
    err->AppendSubErr(
        Err(scope->GetValue(did_declare_args_var)->origin()->GetRange(),
                            "See the original call."));
    return Value();
  }

  // Find the root scope where the values will be set.
  Scope* root = scope->mutable_containing();
  if (!root || root->containing() || !scope->IsProcessingBuildConfig()) {
    *err = Err(function->function(), "declare_args called incorrectly."
        "It must be called only from the build config script and in the "
        "root scope.");
    return Value();
  }

  // Take all variables set in the current scope as default values and put
  // them in the parent scope. The values in the current scope are the defaults,
  // then we apply the external args to this list.
  Scope::KeyValueVector values;
  scope->GetCurrentScopeValues(&values);
  for (size_t i = 0; i < values.size(); i++) {
    // TODO(brettw) actually import the arguments from the command line rather
    // than only using the defaults.
    root->SetValue(values[i].first, values[i].second,
                   values[i].second.origin());
  }

  scope->SetValue(did_declare_args_var, Value(function, 1), NULL);
  return Value();
}

// import ----------------------------------------------------------------------

const char kImport[] = "import";
const char kImport_Help[] =
    "import: Import a file into the current scope.\n"
    "\n"
    "  The import command loads the rules and variables resulting from\n"
    "  executing the given file into the current scope.\n"
    "\n"
    "  By convention, imported files are named with a .gni extension.\n"
    "\n"
    "  It does not do an \"include\". The imported file is executed in a\n"
    "  standalone environment from the caller of the import command. The\n"
    "  results of this execution are cached for other files that import the\n"
    "  same .gni file.\n"
    "\n"
    "  Note that you can not import a BUILD.gn file that's otherwise used\n"
    "  in the build. Files must either be imported or implicitly loaded as\n"
    "  a result of deps rules, but not both.\n"
    "\n"
    "  The imported file's scope will be merged with the scope at the point\n"
    "  import was called. If there is a conflict (both the current scope and\n"
    "  the imported file define some variable or rule with the same name)\n"
    "  a runtime error will be thrown. Therefore, it's good practice to\n"
    "  minimize the stuff that an imported file defines.\n"
    "\n"
    "Examples:\n"
    "\n"
    "  import(\"//build/rules/idl_compilation_rule.gni\")\n"
    "\n"
    "  # Looks in the current directory.\n"
    "  import(\"my_vars.gni\")\n";

Value RunImport(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err) {
  if (!EnsureSingleStringArg(function, args, err) ||
      !EnsureNotProcessingImport(function, scope, err))
    return Value();

  const SourceDir input_dir = SourceDirForFunctionCall(function);
  SourceFile import_file =
      input_dir.ResolveRelativeFile(args[0].string_value());
  scope->settings()->import_manager().DoImport(import_file, function,
                                               scope, err);
  return Value();
}

// set_defaults ----------------------------------------------------------------

const char kSetDefaults[] = "set_defaults";
const char kSetDefaults_Help[] =
    "TODO(brettw) write this.";

Value RunSetDefaults(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     BlockNode* block,
                     Err* err) {
  if (!EnsureSingleStringArg(function, args, err))
    return Value();
  const std::string& target_type(args[0].string_value());

  // Ensure there aren't defaults already set.
  if (scope->GetTargetDefaults(target_type)) {
    *err = Err(function->function(),
               "This target type defaults were already set.");
    return Value();
  }

  // Execute the block in a new scope that has a parent of the containing
  // scope.
  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, function,
                            function->function().value().data(),
                            block, args, &block_scope, err))
    return Value();

  // Run the block for the rule invocation.
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Now copy the values set on the scope we made into the free-floating one
  // (with no containing scope) used to hold the target defaults.
  Scope* dest = scope->MakeTargetDefaults(target_type);
  block_scope.NonRecursiveMergeTo(dest, function, "<SHOULD NOT FAIL>", err);
  return Value();
}

// set_sources_assignment_filter -----------------------------------------------

const char kSetSourcesAssignmentFilter[] = "set_sources_assignment_filter";
const char kSetSourcesAssignmentFilter_Help[] =
    "TODO(brettw) write this.";

Value RunSetSourcesAssignmentFilter(Scope* scope,
                                    const FunctionCallNode* function,
                                    const std::vector<Value>& args,
                                    Err* err) {
  if (args.size() != 1) {
    *err = Err(function, "set_sources_assignment_filter takes one argument.");
  } else {
    scoped_ptr<PatternList> f(new PatternList);
    f->SetFromValue(args[0], err);
    if (!err->has_error())
      scope->set_sources_assignment_filter(f.Pass());
  }
  return Value();
}

// print -----------------------------------------------------------------------

const char kPrint[] = "print";
const char kPrint_Help[] =
    "print(...)\n"
    "  Prints all arguments to the console separated by spaces. A newline is\n"
    "  automatically appended to the end.\n"
    "\n"
    "  This function is intended for debugging. Note that build files are run\n"
    "  in parallel so you may get interleaved prints. A buildfile may also\n"
    "  be executed more than once in parallel in the context of different\n"
    "  toolchains so the prints from one file may be duplicated or\n"
    "  interleaved with itself.\n"
    "\n"
    "Examples:\n"
    "  print(\"Hello world\")\n"
    "\n"
    "  print(sources, deps)\n";

Value RunPrint(Scope* scope,
               const FunctionCallNode* function,
               const std::vector<Value>& args,
               Err* err) {
  for (size_t i = 0; i < args.size(); i++) {
    if (i != 0)
      std::cout << " ";
    std::cout << args[i].ToString();
  }
  std::cout << std::endl;
  return Value();
}

// -----------------------------------------------------------------------------

FunctionInfo::FunctionInfo()
    : generic_block_runner(NULL),
      executed_block_runner(NULL),
      no_block_runner(NULL),
      help(NULL) {
}

FunctionInfo::FunctionInfo(GenericBlockFunction gbf, const char* in_help)
    : generic_block_runner(gbf),
      executed_block_runner(NULL),
      no_block_runner(NULL),
      help(in_help) {
}

FunctionInfo::FunctionInfo(ExecutedBlockFunction ebf, const char* in_help)
    : generic_block_runner(NULL),
      executed_block_runner(ebf),
      no_block_runner(NULL),
      help(in_help) {
}

FunctionInfo::FunctionInfo(NoBlockFunction nbf, const char* in_help)
    : generic_block_runner(NULL),
      executed_block_runner(NULL),
      no_block_runner(nbf),
      help(in_help) {
}

// Setup the function map via a static initializer. We use this because it
// avoids race conditions without having to do some global setup function or
// locking-heavy singleton checks at runtime. In practice, we always need this
// before we can do anything interesting, so it's OK to wait for the
// initializer.
struct FunctionInfoInitializer {
  FunctionInfoMap map;

  FunctionInfoInitializer() {
    #define INSERT_FUNCTION(command) \
        map[k##command] = FunctionInfo(&Run##command, k##command##_Help);

    INSERT_FUNCTION(Assert)
    INSERT_FUNCTION(Component)
    INSERT_FUNCTION(Config)
    INSERT_FUNCTION(Copy)
    INSERT_FUNCTION(Custom)
    INSERT_FUNCTION(DeclareArgs)
    INSERT_FUNCTION(ExecScript)
    INSERT_FUNCTION(Executable)
    INSERT_FUNCTION(Group)
    INSERT_FUNCTION(Import)
    INSERT_FUNCTION(Print)
    INSERT_FUNCTION(ProcessFileTemplate)
    INSERT_FUNCTION(ReadFile)
    INSERT_FUNCTION(SetDefaults)
    INSERT_FUNCTION(SetDefaultToolchain)
    INSERT_FUNCTION(SetSourcesAssignmentFilter)
    INSERT_FUNCTION(SharedLibrary)
    INSERT_FUNCTION(StaticLibrary)
    INSERT_FUNCTION(Template)
    INSERT_FUNCTION(Test)
    INSERT_FUNCTION(Tool)
    INSERT_FUNCTION(Toolchain)
    INSERT_FUNCTION(WriteFile)

    #undef INSERT_FUNCTION
  }
};
const FunctionInfoInitializer function_info;

const FunctionInfoMap& GetFunctions() {
  return function_info.map;
}

Value RunFunction(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  BlockNode* block,
                  Err* err) {
  const Token& name = function->function();

  const FunctionInfoMap& function_map = GetFunctions();
  FunctionInfoMap::const_iterator found_function =
      function_map.find(name.value());
  if (found_function == function_map.end()) {
    // No build-in function matching this, check for a template.
    const FunctionCallNode* rule =
        scope->GetTemplate(function->function().value().as_string());
    if (rule)
      return RunTemplateInvocation(scope, function, args, block, rule, err);

    *err = Err(name, "Unknown function.");
    return Value();
  }

  if (found_function->second.generic_block_runner) {
    if (!block) {
      FillNeedsBlockError(function, err);
      return Value();
    }
    return found_function->second.generic_block_runner(
        scope, function, args, block, err);
  }

  if (found_function->second.executed_block_runner) {
    if (!block) {
      FillNeedsBlockError(function, err);
      return Value();
    }

    Scope block_scope(scope);
    block->ExecuteBlockInScope(&block_scope, err);
    if (err->has_error())
      return Value();
    return found_function->second.executed_block_runner(
        function, args, &block_scope, err);
  }

  // Otherwise it's a no-block function.
  return found_function->second.no_block_runner(scope, function, args, err);
}

}  // namespace functions
