// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_binary_target_writer.h"

#include "tools/gn/config_values_extractors.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/string_utils.h"

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA;

  // Some flag strings are actually multiple flags that expect to be just
  // added to the command line. We assume that quoting is done by the
  // buildfiles if it wants such things quoted.
  opts.inhibit_quoting = true;

  return opts;
}

struct DefineWriter {
  void operator()(const std::string& s, std::ostream& out) const {
    out << " -D" << s;
  }
};

struct IncludeWriter {
  IncludeWriter(PathOutput& path_output,
                const NinjaHelper& h)
      : helper(h),
        path_output_(path_output),
        old_inhibit_quoting_(path_output.inhibit_quoting()) {
    // Inhibit quoting since we'll put quotes around the whole thing ourselves.
    // Since we're writing in NINJA escaping mode, this won't actually do
    // anything, but I think we may need to change to shell-and-then-ninja
    // escaping for this in the future.
    path_output_.set_inhibit_quoting(true);
  }
  ~IncludeWriter() {
    path_output_.set_inhibit_quoting(old_inhibit_quoting_);
  }

  void operator()(const SourceDir& d, std::ostream& out) const {
    out << " \"-I";
    // It's important not to include the trailing slash on directories or on
    // Windows it will be a backslash and the compiler might think we're
    // escaping the quote!
    path_output_.WriteDir(out, d, PathOutput::DIR_NO_LAST_SLASH);
    out << "\"";
  }

  const NinjaHelper& helper;
  PathOutput& path_output_;
  bool old_inhibit_quoting_;  // So we can put the PathOutput back.
};

}  // namespace

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out) {
}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() {
}

void NinjaBinaryTargetWriter::Run() {
  WriteEnvironment();

  WriteCompilerVars();

  std::vector<OutputFile> obj_files;
  WriteSources(&obj_files);

  WriteLinkerStuff(obj_files);
}

void NinjaBinaryTargetWriter::WriteCompilerVars() {
  // Defines.
  out_ << "defines =";
  RecursiveTargetConfigToStream(target_, &ConfigValues::defines,
                                DefineWriter(), out_);
  out_ << std::endl;

  // Includes.
  out_ << "includes =";
  RecursiveTargetConfigToStream(target_, &ConfigValues::includes,
                                IncludeWriter(path_output_, helper_), out_);

  out_ << std::endl;

  // C flags and friends.
  EscapeOptions flag_escape_options = GetFlagOptions();
#define WRITE_FLAGS(name) \
    out_ << #name " ="; \
    RecursiveTargetConfigStringsToStream(target_, &ConfigValues::name, \
                                         flag_escape_options, out_); \
    out_ << std::endl;

  WRITE_FLAGS(cflags)
  WRITE_FLAGS(cflags_c)
  WRITE_FLAGS(cflags_cc)
  WRITE_FLAGS(cflags_objc)
  WRITE_FLAGS(cflags_objcc)

#undef WRITE_FLAGS

  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteSources(
    std::vector<OutputFile>* object_files) {
  const Target::FileList& sources = target_->sources();
  object_files->reserve(sources.size());

  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];

    SourceFileType input_file_type = GetSourceFileType(input_file,
                                                       settings_->target_os());
    if (input_file_type == SOURCE_UNKNOWN)
      continue;  // Skip unknown file types.
    const char* command = GetCommandForSourceType(input_file_type);
    if (!command)
      continue;  // Skip files not needing compilation.

    OutputFile output_file = helper_.GetOutputFileForSource(
        target_, input_file, input_file_type);
    object_files->push_back(output_file);

    out_ << "build ";
    path_output_.WriteFile(out_, output_file);
    out_ << ": " << command << " ";
    path_output_.WriteFile(out_, input_file);
    out_ << std::endl;
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkerStuff(
    const std::vector<OutputFile>& object_files) {
  // Manifest file on Windows.
  // TODO(brettw) this seems not to be necessary for static libs, skip in
  // that case?
  OutputFile windows_manifest;
  if (settings_->IsWin()) {
    windows_manifest.value().assign(helper_.GetTargetOutputDir(target_));
    windows_manifest.value().append(target_->label().name());
    windows_manifest.value().append(".intermediate.manifest");
    out_ << "manifests = ";
    path_output_.WriteFile(out_, windows_manifest);
    out_ << std::endl;
  }

  // Linker flags, append manifest flag on Windows to reference our file.
  out_ << "ldflags =";
  RecursiveTargetConfigStringsToStream(target_, &ConfigValues::ldflags,
                                       GetFlagOptions(), out_);
  // HACK ERASEME BRETTW FIXME
  if (settings_->IsWin()) {
    out_ << " /MANIFEST /ManifestFile:";
    path_output_.WriteFile(out_, windows_manifest);
    out_ << " /DEBUG /MACHINE:X86 /LIBPATH:\"C:\\Program Files (x86)\\Windows Kits\\8.0\\Lib\\win8\\um\\x86\" /DELAYLOAD:dbghelp.dll /DELAYLOAD:dwmapi.dll /DELAYLOAD:shell32.dll /DELAYLOAD:uxtheme.dll /safeseh /dynamicbase /ignore:4199 /ignore:4221 /nxcompat /SUBSYSTEM:CONSOLE /INCREMENTAL /FIXED:NO /DYNAMICBASE:NO wininet.lib dnsapi.lib version.lib msimg32.lib ws2_32.lib usp10.lib psapi.lib dbghelp.lib winmm.lib shlwapi.lib kernel32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib user32.lib uuid.lib odbc32.lib odbccp32.lib delayimp.lib /NXCOMPAT";
  }
  out_ << std::endl;

  // Libraries to link.
  out_ << "libs =";
  if (settings_->IsMac()) {
    // TODO(brettw) fix this.
    out_ << " -framework AppKit -framework ApplicationServices -framework Carbon -framework CoreFoundation -framework Foundation -framework IOKit -framework Security";
  }
  out_ << std::endl;

  // The external output file is the one that other libs depend on.
  OutputFile external_output_file = helper_.GetTargetOutputFile(target_);

  // The internal output file is the "main thing" we think we're making. In
  // the case of shared libraries, this is the shared library and the external
  // output file is the import library. In other cases, the internal one and
  // the external one are the same.
  OutputFile internal_output_file;
  if (target_->output_type() == Target::SHARED_LIBRARY) {
    if (settings_->IsWin()) {
      internal_output_file = OutputFile(target_->label().name() + ".dll");
    } else {
      internal_output_file = external_output_file;
    }
  } else {
    internal_output_file = external_output_file;
  }

  // In Python see "self.ninja.build(output, command, input,"
  WriteLinkCommand(external_output_file, internal_output_file, object_files);

  if (target_->output_type() == Target::SHARED_LIBRARY) {
    // The shared object name doesn't include a path.
    out_ << "  soname = ";
    out_ << FindFilename(&internal_output_file.value());
    out_ << std::endl;

    out_ << "  lib = ";
    path_output_.WriteFile(out_, internal_output_file);
    out_ << std::endl;

    if (settings_->IsWin()) {
      out_ << "  dll = ";
      path_output_.WriteFile(out_, internal_output_file);
      out_ << std::endl;
    }

    if (settings_->IsWin()) {
      out_ << "  implibflag = /IMPLIB:";
      path_output_.WriteFile(out_, external_output_file);
      out_ << std::endl;
    }

    // TODO(brettw) postbuild steps.
    if (settings_->IsMac())
      out_ << "  postbuilds = $ && (export BUILT_PRODUCTS_DIR=/Users/brettw/prj/src/out/gn; export CONFIGURATION=Debug; export DYLIB_INSTALL_NAME_BASE=@rpath; export EXECUTABLE_NAME=libbase.dylib; export EXECUTABLE_PATH=libbase.dylib; export FULL_PRODUCT_NAME=libbase.dylib; export LD_DYLIB_INSTALL_NAME=@rpath/libbase.dylib; export MACH_O_TYPE=mh_dylib; export PRODUCT_NAME=base; export PRODUCT_TYPE=com.apple.product-type.library.dynamic; export SDKROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk; export SRCROOT=/Users/brettw/prj/src/out/gn/../../base; export SOURCE_ROOT=\"$${SRCROOT}\"; export TARGET_BUILD_DIR=/Users/brettw/prj/src/out/gn; export TEMP_DIR=\"$${TMPDIR}\"; (cd ../../base && ../build/mac/strip_from_xcode); G=$$?; ((exit $$G) || rm -rf libbase.dylib) && exit $$G)";
  }

  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkCommand(
    const OutputFile& external_output_file,
    const OutputFile& internal_output_file,
    const std::vector<OutputFile>& object_files) {
  out_ << "build ";
  path_output_.WriteFile(out_, internal_output_file);
  if (external_output_file != internal_output_file) {
    out_ << " ";
    path_output_.WriteFile(out_, external_output_file);
  }
  out_ << ": " << GetCommandForTargetType();

  // Object files.
  for (size_t i = 0; i < object_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, object_files[i]);
  }

  // Library inputs (deps and inherited static libraries).
  //
  // Static libraries since they're just a collection of the object files so
  // don't need libraries linked with them, but we still need to go through
  // the list and find non-linkable data deps in the "deps" section. We'll
  // collect all non-linkable deps and put it in the order-only deps below.
  std::vector<const Target*> extra_data_deps;
  const std::vector<const Target*>& deps = target_->deps();
  const std::set<const Target*>& inherited = target_->inherited_libraries();
  for (size_t i = 0; i < deps.size(); i++) {
    if (inherited.find(deps[i]) != inherited.end())
      continue;
    if (target_->output_type() != Target::STATIC_LIBRARY &&
        deps[i]->IsLinkable()) {
      out_ << " ";
      path_output_.WriteFile(out_, helper_.GetTargetOutputFile(deps[i]));
    } else {
      extra_data_deps.push_back(deps[i]);
    }
  }
  for (std::set<const Target*>::const_iterator i = inherited.begin();
       i != inherited.end(); ++i) {
    if (target_->output_type() == Target::STATIC_LIBRARY) {
      extra_data_deps.push_back(*i);
    } else {
      out_ << " ";
      path_output_.WriteFile(out_, helper_.GetTargetOutputFile(*i));
    }
  }

  // Append data dependencies as order-only dependencies.
  const std::vector<const Target*>& datadeps = target_->datadeps();
  const std::vector<SourceFile>& data = target_->data();
  if (!extra_data_deps.empty() || !datadeps.empty() || !data.empty()) {
    out_ << " ||";

    // Non-linkable deps in the deps section above.
    for (size_t i = 0; i < extra_data_deps.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_,
                             helper_.GetTargetOutputFile(extra_data_deps[i]));
    }

    // Data deps.
    for (size_t i = 0; i < datadeps.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_, helper_.GetTargetOutputFile(datadeps[i]));
    }

    // Data files.
    const std::vector<SourceFile>& data = target_->data();
    for (size_t i = 0; i < data.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_, data[i]);
    }
  }

  out_ << std::endl;
}

const char* NinjaBinaryTargetWriter::GetCommandForSourceType(
    SourceFileType type) const {
  if (type == SOURCE_C)
    return "cc";
  if (type == SOURCE_CC)
    return "cxx";

  // TODO(brettw) asm files.

  if (settings_->IsMac()) {
    if (type == SOURCE_M)
      return "objc";
    if (type == SOURCE_MM)
      return "objcxx";
  }

  if (settings_->IsWin()) {
    if (type == SOURCE_RC)
      return "rc";
  }

  // TODO(brettw) stuff about "S" files on non-Windows.
  return NULL;
}

const char* NinjaBinaryTargetWriter::GetCommandForTargetType() const {
  if (target_->output_type() == Target::STATIC_LIBRARY) {
    // TODO(brettw) stuff about standalong static libraryes on Unix in
    // WriteTarget in the Python one, and lots of postbuild steps.
    return "alink";
  }

  if (target_->output_type() == Target::SHARED_LIBRARY)
    return "solink";

  return "link";
}
