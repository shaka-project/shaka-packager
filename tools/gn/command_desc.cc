// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>
#include <sstream>

#include "base/command_line.h"
#include "tools/gn/commands.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/item.h"
#include "tools/gn/item_node.h"
#include "tools/gn/label.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

namespace {

struct CompareTargetLabel {
  bool operator()(const Target* a, const Target* b) const {
    return a->label() < b->label();
  }
};

void RecursiveCollectDeps(const Target* target, std::set<Label>* result) {
  if (result->find(target->label()) != result->end())
    return;  // Already did this target.
  result->insert(target->label());

  const std::vector<const Target*>& deps = target->deps();
  for (size_t i = 0; i < deps.size(); i++)
    RecursiveCollectDeps(deps[i], result);

  const std::vector<const Target*>& datadeps = target->datadeps();
  for (size_t i = 0; i < datadeps.size(); i++)
    RecursiveCollectDeps(datadeps[i], result);
}

// Prints dependencies of the given target (not the target itself).
void RecursivePrintDeps(const Target* target,
                        const Label& default_toolchain,
                        int indent_level) {
  std::vector<const Target*> sorted_deps = target->deps();
  const std::vector<const Target*> datadeps = target->datadeps();
  for (size_t i = 0; i < datadeps.size(); i++)
    sorted_deps.push_back(datadeps[i]);
  std::sort(sorted_deps.begin(), sorted_deps.end(), CompareTargetLabel());

  std::string indent(indent_level * 2, ' ');
  for (size_t i = 0; i < sorted_deps.size(); i++) {
    OutputString(indent +
        sorted_deps[i]->label().GetUserVisibleName(default_toolchain) + "\n");
    RecursivePrintDeps(sorted_deps[i], default_toolchain, indent_level + 1);
  }
}

void PrintDeps(const Target* target, bool display_header) {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  Label toolchain_label = target->label().GetToolchainLabel();

  // Tree mode is separate.
  if (cmdline->HasSwitch("tree")) {
    if (display_header)
      OutputString("\nDependency tree:\n");
    RecursivePrintDeps(target, toolchain_label, 1);
    return;
  }

  // Collect the deps to display.
  std::vector<Label> deps;
  if (cmdline->HasSwitch("all")) {
    if (display_header)
      OutputString("\nAll recursive dependencies:\n");

    std::set<Label> all_deps;
    RecursiveCollectDeps(target, &all_deps);
    for (std::set<Label>::iterator i = all_deps.begin();
         i != all_deps.end(); ++i)
      deps.push_back(*i);
  } else {
    if (display_header) {
      OutputString("\nDirect dependencies "
                   "(try also \"--all\" and \"--tree\"):\n");
    }

    const std::vector<const Target*>& target_deps = target->deps();
    for (size_t i = 0; i < target_deps.size(); i++)
      deps.push_back(target_deps[i]->label());

    const std::vector<const Target*>& target_datadeps = target->datadeps();
    for (size_t i = 0; i < target_datadeps.size(); i++)
      deps.push_back(target_datadeps[i]->label());
  }

  std::sort(deps.begin(), deps.end());
  for (size_t i = 0; i < deps.size(); i++)
    OutputString("  " + deps[i].GetUserVisibleName(toolchain_label) + "\n");
}

void PrintConfigs(const Target* target, bool display_header) {
  // Configs (don't sort since the order determines how things are processed).
  if (display_header)
    OutputString("\nConfigs (in order applying):\n");

  Label toolchain_label = target->label().GetToolchainLabel();
  const std::vector<const Config*>& configs = target->configs();
  for (size_t i = 0; i < configs.size(); i++) {
    OutputString("  " +
        configs[i]->label().GetUserVisibleName(toolchain_label) + "\n");
  }
}

void PrintSources(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nSources:\n");

  Target::FileList sources = target->sources();
  std::sort(sources.begin(), sources.end());
  for (size_t i = 0; i < sources.size(); i++)
    OutputString("  " + sources[i].value() + "\n");
}

// Attempts to attribute the gen dependency of the given target to some source
// code and outputs the string to the output stream.
//
// The attribution of the source of the dependencies is stored in the ItemNode
// which is the parallel structure to the target dependency map, so we have
// to jump through a few loops to find everything.
void OutputSourceOfDep(const Target* target,
                       const Label& dep_label,
                       std::ostream& out) {
  ItemTree& item_tree = target->settings()->build_settings()->item_tree();
  base::AutoLock lock(item_tree.lock());

  ItemNode* target_node = item_tree.GetExistingNodeLocked(target->label());
  CHECK(target_node);
  ItemNode* dep_node = item_tree.GetExistingNodeLocked(dep_label);
  CHECK(dep_node);

  const ItemNode::ItemNodeMap& direct_deps = target_node->direct_dependencies();
  ItemNode::ItemNodeMap::const_iterator found = direct_deps.find(dep_node);
  if (found == direct_deps.end())
    return;

  const Location& location = found->second.begin();
  out << "       (Added by " + location.file()->name().value() << ":"
      << location.line_number() << ")\n";
}

// Templatized writer for writing out different config value types.
template<typename T> struct DescValueWriter {};
template<> struct DescValueWriter<std::string> {
  void operator()(const std::string& str, std::ostream& out) const {
    out << "    " << str << "\n";
  }
};
template<> struct DescValueWriter<SourceFile> {
  void operator()(const SourceFile& file, std::ostream& out) const {
    out << "    " << file.value() << "\n";
  }
};
template<> struct DescValueWriter<SourceDir> {
  void operator()(const SourceDir& dir, std::ostream& out) const {
    out << "    " << dir.value() << "\n";
  }
};

// Writes a given config value type to the string, optionally with attribution.
// This should match RecursiveTargetConfigToStream in the order it traverses.
template<typename T> void OutputRecursiveTargetConfig(
    const Target* target,
    const char* header_name,
    const std::vector<T>& (ConfigValues::* getter)() const) {
  bool display_blame = CommandLine::ForCurrentProcess()->HasSwitch("blame");

  DescValueWriter<T> writer;
  std::ostringstream out;

  // First write the values from the config itself.
  if (!(target->config_values().*getter)().empty()) {
    if (display_blame)
      out << "  From " << target->label().GetUserVisibleName(false) << "\n";
    ConfigValuesToStream(target->config_values(), getter, writer, out);
  }

  // TODO(brettw) annotate where forced config includes came from!

  // Then write the configs in order.
  for (size_t i = 0; i < target->configs().size(); i++) {
    const Config* config = target->configs()[i];
    const ConfigValues& values = config->config_values();

    if (!(values.*getter)().empty()) {
      if (display_blame) {
        out << "  From " << config->label().GetUserVisibleName(false) << "\n";
        OutputSourceOfDep(target, config->label(), out);
      }
      ConfigValuesToStream(values, getter, writer, out);
    }
  }

  std::string out_str = out.str();
  if (!out_str.empty()) {
    OutputString(std::string(header_name) + "\n");
    OutputString(out_str);
  }
}

}  // namespace

// desc ------------------------------------------------------------------------

const char kDesc[] = "desc";
const char kDesc_HelpShort[] =
    "desc: Show lots of insightful information about a target.";
const char kDesc_Help[] =
    "gn desc <target label> [<what to show>] [--blame] [--all | --tree]\n"
    "  Displays information about a given labeled target.\n"
    "\n"
    "Possibilities for <what to show>:\n"
    "  (If unspecified an overall summary will be displayed.)\n"
    "\n"
    "  sources\n"
    "      Source files.\n"
    "\n"
    "  configs\n"
    "      Shows configs applied to the given target, sorted in the order\n"
    "      they're specified. This includes both configs specified in the\n"
    "      \"configs\" variable, as well as configs pushed onto this target\n"
    "      via dependencies specifying \"all\" or \"direct\" dependent\n"
    "      configs.\n"
    "\n"
    "  deps [--all | --tree]\n"
    "      Show immediate (or, when \"--all\" or \"--tree\" is specified,\n"
    "      recursive) dependencies of the given target. \"--tree\" shows them\n"
    "      in a tree format.  Otherwise, they will be sorted alphabetically.\n"
    "      Both \"deps\" and \"datadeps\" will be included.\n"
    "\n"
    "  defines    [--blame]\n"
    "  includes   [--blame]\n"
    "  cflags     [--blame]\n"
    "  cflags_cc  [--blame]\n"
    "  cflags_cxx [--blame]\n"
    "  ldflags    [--blame]\n"
    "      Shows the given values taken from the target and all configs\n"
    "      applying. See \"--blame\" below.\n"
    "\n"
    "  --blame\n"
    "      Used with any value specified by a config, this will name\n"
    "      the config that specified the value.\n"
    "\n"
    "Note:\n"
    "  This command will show the full name of directories and source files,\n"
    "  but when directories and source paths are written to the build file,\n"
    "  they will be adjusted to be relative to the build directory. So the\n"
    "  values for paths displayed by this command won't match (but should\n"
    "  mean the same thing.\n"
    "\n"
    "Examples:\n"
    "  gn desc //base:base\n"
    "      Summarizes the given target.\n"
    "\n"
    "  gn desc :base_unittests deps --tree\n"
    "      Shows a dependency tree of the \"base_unittests\" project in\n"
    "      the current directory.\n"
    "\n"
    "  gn desc //base defines --blame\n"
    "      Shows defines set for the //base:base target, annotated by where\n"
    "      each one was set from.\n";

int RunDesc(const std::vector<std::string>& args) {
  if (args.size() != 1 && args.size() != 2) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn desc <target_name> <what to display>\"").PrintToStdout();
    return 1;
  }

  const Target* target = GetTargetForDesc(args);
  if (!target)
    return 1;

#define CONFIG_VALUE_HANDLER(name) \
    } else if (what == #name) { \
      OutputRecursiveTargetConfig(target, #name, &ConfigValues::name);

  if (args.size() == 2) {
    // User specified one thing to display.
    const std::string& what = args[1];
    if (what == "configs") {
      PrintConfigs(target, false);
    } else if (what == "sources") {
      PrintSources(target, false);
    } else if (what == "deps") {
      PrintDeps(target, false);

    CONFIG_VALUE_HANDLER(defines)
    CONFIG_VALUE_HANDLER(includes)
    CONFIG_VALUE_HANDLER(cflags)
    CONFIG_VALUE_HANDLER(cflags_c)
    CONFIG_VALUE_HANDLER(cflags_cc)
    CONFIG_VALUE_HANDLER(ldflags)

    } else {
      OutputString("Don't know how to display \"" + what + "\".\n");
      return 1;
    }

#undef CONFIG_VALUE_HANDLER
    return 0;
  }

  // Display summary.

  // Generally we only want to display toolchains on labels when the toolchain
  // is different than the default one for this target (which we always print
  // in the header).
  Label target_toolchain = target->label().GetToolchainLabel();

  // Header.
  std::string title_target =
      "Target: " + target->label().GetUserVisibleName(false);
  std::string title_toolchain =
      "Toolchain: " + target_toolchain.GetUserVisibleName(false);
  OutputString(title_target + "\n", DECORATION_YELLOW);
  OutputString(title_toolchain + "\n", DECORATION_YELLOW);
  OutputString(std::string(
      std::max(title_target.size(), title_toolchain.size()), '=') + "\n");

  PrintSources(target, true);
  PrintConfigs(target, true);
  OutputString("\n  (Use \"gn desc <label> <thing you want to see>\" to show "
               "the actual values\n   applied by the different configs. "
               "See \"gn help desc\" for more.)\n");
  PrintDeps(target, true);

  return 0;
}

}  // namespace commands
