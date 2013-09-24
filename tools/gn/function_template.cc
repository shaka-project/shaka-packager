// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

namespace functions {

const char kTemplate[] = "template";
const char kTemplate_Help[] =
    "template: Define a template rule.\n"
    "\n"
    "  A template defines a custom rule name that can expand to one or more\n"
    "  other rules (typically built-in rules like \"static_library\"). It\n"
    "  provides a way to add to the built-in target types.\n"
    "\n"
    "  The template() function is used to declare a template. To invoke the\n"
    "  template, just use the name of the template like any other target\n"
    "  type.\n"
    "\n"
    "More details:\n"
    "\n"
    "  Semantically, the code in the template is stored. When a function\n"
    "  with the name is called, the block following the invocation is\n"
    "  executed, *then* your template code is executed. So if the invocation\n"
    "  sets the |source| variable, for example, that variable will be\n"
    "  accessible to you when the template code runs.\n"
    "\n"
    "  The template() function does not generate a closure, so the\n"
    "  environment, current directory, etc. will all be the same as from\n"
    "  the template is invoked.\n"
    "\n"
    "Hints:\n"
    "\n"
    "  If your template expands to more than one target, be sure to name\n"
    "  the intermediate targets based on the name of the template\n"
    "  instantiation so that the names are globally unique. The variable\n"
    "  |target_name| will be this name.\n"
    "\n"
    "  Likewise, you will always want to generate a target in your template\n"
    "  with the original |target_name|. Otherwise, invoking your template\n"
    "  will not actually generate a node in the dependency graph that other\n"
    "  targets can reference.\n"
    "\n"
    "  Often you will want to declare your template in a special file that\n"
    "  other files will import (see \"gn help import\") so your template\n"
    "  rule can be shared across build files.\n"
    "\n"
    "Example of defining a template:\n"
    "\n"
    "  template(\"my_idl\") {\n"
    "    # Maps input files to output files, used in both targets below.\n"
    "    filter = [ \"$target_gen_dir/{{source_name_part}}.cc\",\n"
    "               \"$target_gen_dir/{{source_name_part}}.h\" ]\n"
    "\n"
    "    # Intermediate target to compile IDL to C source.\n"
    "    custom(\"${target_name}_code_gen\") {\n"
    "      # The |sources| will be inherited from the surrounding scope so\n"
    "      # we don't need to redefine it.\n"
    "      script = \"foo.py\"\n"
    "      outputs = filter  # Variable from above.\n"
    "    }\n"
    "\n"
    "    # Name the static library the same as the template invocation so\n"
    "    # instanting this template produces something that other targets\n"
    "    # can link to in their deps.\n"
    "    static_library(target_name) {\n"
    "      # Generates the list of sources.\n"
    "      # See \"gn help process_file_template\"\n"
    "      sources = process_file_template(sources, filter)\n"
    "    }\n"
    "  }\n"
    "\n"
    "Example of invoking the resulting template:\n"
    "\n"
    "  my_idl(\"foo_idl_files\") {\n"
    "    sources = [ \"foo.idl\", \"bar.idl\" ]\n"
    "  }\n";

Value RunTemplate(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  BlockNode* block,
                  Err* err) {
  // TODO(brettw) determine if the function is built-in and throw an error if
  // it is.
  if (args.size() != 1) {
    *err = Err(function->function(),
               "Need exactly one string arg to template.");
    return Value();
  }
  if (!args[0].VerifyTypeIs(Value::STRING, err))
    return Value();
  std::string template_name = args[0].string_value();

  const FunctionCallNode* existing_template = scope->GetTemplate(template_name);
  if (existing_template) {
    *err = Err(function, "Duplicate template definition.",
               "A template with this name was already defined.");
    err->AppendSubErr(Err(existing_template->function(),
                          "Previous definition."));
    return Value();
  }

  scope->AddTemplate(template_name, function);
  return Value();
}

}  // namespace functions
