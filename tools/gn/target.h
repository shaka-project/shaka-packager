// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_H_
#define TOOLS_GN_TARGET_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "tools/gn/config_values.h"
#include "tools/gn/item.h"
#include "tools/gn/script_values.h"
#include "tools/gn/source_file.h"

class InputFile;
class Settings;
class Token;

class Target : public Item {
 public:
  enum OutputType {
    UNKNOWN,
    GROUP,
    EXECUTABLE,
    SHARED_LIBRARY,
    STATIC_LIBRARY,
    COPY_FILES,
    CUSTOM,
  };
  typedef std::vector<SourceFile> FileList;
  typedef std::vector<std::string> StringVector;

  Target(const Settings* settings, const Label& label);
  virtual ~Target();

  // Item overrides.
  virtual Target* AsTarget() OVERRIDE;
  virtual const Target* AsTarget() const OVERRIDE;
  virtual void OnResolved() OVERRIDE;

  // This flag indicates if we've run the TargetGenerator for this target to
  // fill out the rest of the values. Once we've done this, we save the
  // location of the function that started the generating so that we can detect
  // duplicate declarations.
  bool HasBeenGenerated() const;
  void SetGenerated(const Token* token);

  const Settings* settings() const { return settings_; }

  OutputType output_type() const { return output_type_; }
  void set_output_type(OutputType t) { output_type_ = t; }

  bool IsLinkable() const;

  const FileList& sources() const { return sources_; }
  void swap_in_sources(FileList* s) { sources_.swap(*s); }

  const FileList& data() const { return data_; }
  void swap_in_data(FileList* d) { data_.swap(*d); }

  // Linked dependencies.
  const std::vector<const Target*>& deps() const { return deps_; }
  void swap_in_deps(std::vector<const Target*>* d) { deps_.swap(*d); }

  // Non-linked dependencies.
  const std::vector<const Target*>& datadeps() const { return datadeps_; }
  void swap_in_datadeps(std::vector<const Target*>* d) { datadeps_.swap(*d); }

  // List of configs that this class inherits settings from.
  const std::vector<const Config*>& configs() const { return configs_; }
  void swap_in_configs(std::vector<const Config*>* c) { configs_.swap(*c); }

  // List of configs that all dependencies (direct and indirect) of this
  // target get. These configs are not added to this target.
  const std::vector<const Config*>& all_dependent_configs() const {
    return all_dependent_configs_;
  }
  void swap_in_all_dependent_configs(std::vector<const Config*>* c) {
    all_dependent_configs_.swap(*c);
  }

  // List of configs that targets depending directly on this one get. These
  // configs are not added to this target.
  const std::vector<const Config*>& direct_dependent_configs() const {
    return direct_dependent_configs_;
  }
  void swap_in_direct_dependent_configs(std::vector<const Config*>* c) {
    direct_dependent_configs_.swap(*c);
  }

  const std::set<const Target*>& inherited_libraries() const {
    return inherited_libraries_;
  }

  // This config represents the configuration set directly on this target.
  ConfigValues& config_values() { return config_values_; }
  const ConfigValues& config_values() const { return config_values_; }

  ScriptValues& script_values() { return script_values_; }
  const ScriptValues& script_values() const { return script_values_; }

  const SourceDir& destdir() const { return destdir_; }
  void set_destdir(const SourceDir& d) { destdir_ = d; }

 private:
  const Settings* settings_;
  OutputType output_type_;

  FileList sources_;
  FileList data_;
  std::vector<const Target*> deps_;
  std::vector<const Target*> datadeps_;
  std::vector<const Config*> configs_;
  std::vector<const Config*> all_dependent_configs_;
  std::vector<const Config*> direct_dependent_configs_;

  // Libraries from transitive deps. Libraries need to be linked only
  // with the end target (executable, shared library). These do not get
  // pushed beyond shared library boundaries.
  std::set<const Target*> inherited_libraries_;

  ConfigValues config_values_;  // Used for all binary targets.
  ScriptValues script_values_;  // Used for script (CUSTOM) targets.

  SourceDir destdir_;

  bool generated_;
  const Token* generator_function_;  // Who generated this: for error messages.

  DISALLOW_COPY_AND_ASSIGN(Target);
};

#endif  // TOOLS_GN_TARGET_H_
