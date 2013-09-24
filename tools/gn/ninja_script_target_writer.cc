// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_script_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/err.h"
#include "tools/gn/file_template.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

NinjaScriptTargetWriter::NinjaScriptTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out) {
}

NinjaScriptTargetWriter::~NinjaScriptTargetWriter() {
}

void NinjaScriptTargetWriter::Run() {
  WriteEnvironment();

  // Run the script from the dir of the BUILD file. This has no trailing
  // slash.
  const SourceDir& script_cd = target_->label().dir();
  std::string script_cd_to_root = InvertDir(script_cd);
  if (script_cd_to_root.empty()) {
    script_cd_to_root = ".";
  } else {
    // Remove trailing slash
    DCHECK(script_cd_to_root[script_cd_to_root.size() - 1] == '/');
    script_cd_to_root.resize(script_cd_to_root.size() - 1);
  }
  std::string script_relative_to_cd =
      script_cd_to_root + target_->script_values().script().value();

  std::string custom_rule_name = WriteRuleDefinition(script_relative_to_cd);

  // Precompute the common dependencies for each step. This includes the
  // script itself (changing the script should force a rebuild) and any data
  // files.
  //
  // TODO(brettw) this needs to be re-thought. "data" is supposed to be runtime
  // data (i.e. for tests and such) rather than compile-time dependencies for
  // each target. If we really need this, we need to have a different way to
  // express it.
  //
  // One idea: add an "inputs" variable to specify this kind of thing. We
  // should probably make it an error to specify data but no inputs for a
  // script as a way to catch people doing the wrong way.
  std::ostringstream common_deps_stream;
  path_output_.WriteFile(common_deps_stream, target_->script_values().script());
  const Target::FileList& datas = target_->data();
  for (size_t i = 0; i < datas.size(); i++) {
    common_deps_stream << " ";
    path_output_.WriteFile(common_deps_stream, datas[i]);
  }
  const std::string& common_deps = common_deps_stream.str();

  // Collects all output files for writing below.
  std::vector<OutputFile> output_files;

  if (!has_sources()) {
    // Write separate rules for each input source file.
    WriteSourceRules(custom_rule_name, common_deps, script_cd,
                     script_cd_to_root, &output_files);
  } else {
    // No sources, write a rule that invokes the script once with the
    // outputs as outputs, and the data as inputs.
    out_ << "build";
    const Target::FileList& outputs = target_->script_values().outputs();
    for (size_t i = 0; i < outputs.size(); i++) {
      OutputFile output_path(
          RemovePrefix(outputs[i].value(),
                       settings_->build_settings()->build_dir().value()));
      output_files.push_back(output_path);
      out_ << " ";
      path_output_.WriteFile(out_, output_path);
    }
    out_ << ": " << custom_rule_name << " " << common_deps << std::endl;
  }
  out_ << std::endl;

  WriteStamp(output_files);
}

std::string NinjaScriptTargetWriter::WriteRuleDefinition(
    const std::string& script_relative_to_cd) {
  // Make a unique name for this rule.
  std::string target_label = target_->label().GetUserVisibleName(true);
  std::string custom_rule_name(target_label);
  ReplaceChars(custom_rule_name, ":/()", "_", &custom_rule_name);
  custom_rule_name.append("_rule");

  // Use a unique name for the response file when there are multiple build
  // steps so that they don't stomp on each other. When there are no sources,
  // there will be only one invocation so we can use a simple name.
  std::string rspfile = custom_rule_name;
  if (has_sources())
    rspfile += ".$unique_name";
  rspfile += ".rsp";

  out_ << "rule " << custom_rule_name << std::endl;
  out_ << "  command = $pythonpath gyp-win-tool action-wrapper $arch "
       << rspfile << " ";
  path_output_.WriteDir(out_, target_->label().dir(),
                        PathOutput::DIR_NO_LAST_SLASH);
  out_ << std::endl;
  out_ << "  description = CUSTOM " << target_label << std::endl;
  out_ << "  restat = 1" << std::endl;
  out_ << "  rspfile = " << rspfile << std::endl;

  // The build command goes in the rsp file.
  out_ << "  rspfile_content = $pythonpath " << script_relative_to_cd;
  for (size_t i = 0; i < target_->script_values().args().size(); i++) {
    const std::string& arg = target_->script_values().args()[i];
    out_ << " ";
    WriteArg(arg);
  }
  out_ << std::endl;
  return custom_rule_name;
}

void NinjaScriptTargetWriter::WriteArg(const std::string& arg) {
  // This can be optimized if it's called a lot.
  EscapeOptions options;
  options.mode = ESCAPE_NINJA;
  std::string output_str = EscapeString(arg, options);

  // Do this substitution after escaping our our $ will be escaped (which we
  // don't want).
  ReplaceSubstringsAfterOffset(&output_str, 0, FileTemplate::kSource,
                               "${source}");
  ReplaceSubstringsAfterOffset(&output_str, 0, FileTemplate::kSourceNamePart,
                               "${source_name_part}");
  out_ << output_str;
}

void NinjaScriptTargetWriter::WriteSourceRules(
    const std::string& custom_rule_name,
    const std::string& common_deps,
    const SourceDir& script_cd,
    const std::string& script_cd_to_root,
    std::vector<OutputFile>* output_files) {
  // Construct the template for generating the output files from each source.
  const Target::FileList& outputs = target_->script_values().outputs();
  std::vector<std::string> output_template_args;
  for (size_t i = 0; i < outputs.size(); i++) {
    // All outputs should be in the output dir.
    output_template_args.push_back(
        RemovePrefix(outputs[i].value(),
                     settings_->build_settings()->build_dir().value()));
  }
  FileTemplate output_template(output_template_args);

  // Prevent re-allocating each time by initializing outside the loop.
  std::vector<std::string> output_template_result;

  // Path output formatter for wrigin source paths passed to the script.
  PathOutput script_source_path_output(script_cd, ESCAPE_SHELL, true);

  const Target::FileList& sources = target_->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    // Write outputs for this source file computed by the template.
    out_ << "build";
    output_template.ApplyString(sources[i].value(), &output_template_result);
    for (size_t out_i = 0; out_i < output_template_result.size(); out_i++) {
      OutputFile output_path(output_template_result[out_i]);
      output_files->push_back(output_path);
      out_ << " ";
      path_output_.WriteFile(out_, output_path);
    }

    out_ << ": " << custom_rule_name
         << " " << common_deps
         << " ";
    path_output_.WriteFile(out_, sources[i]);
    out_ << std::endl;

    out_ << "  unique_name = " << i << std::endl;

    // The source file here should be relative to the script directory since
    // this is the variable passed to the script. Here we slightly abuse the
    // OutputFile object by putting a non-output-relative path in it to signal
    // that the PathWriter should not prepend directories.
    out_ << "  source = ";
    script_source_path_output.WriteFile(out_, sources[i]);
    out_ << std::endl;

    out_ << "  source_name_part = "
         << FindFilenameNoExtension(&sources[i].value()).as_string()
         << std::endl;
  }
}

void NinjaScriptTargetWriter::WriteStamp(
    const std::vector<OutputFile>& output_files) {
  out_ << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": stamp";
  for (size_t i = 0; i < output_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, output_files[i]);
  }
  out_ << std::endl;
}
