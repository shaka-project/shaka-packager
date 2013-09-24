// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/scope.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "tools/gn/parse_tree.h"

namespace {

// FLags set in the mode_flags_ of a scope. If a bit is set, it applies
// recursively to all dependent scopes.
const unsigned kProcessingBuildConfigFlag = 1;
const unsigned kProcessingDefaultBuildConfigFlag = 2;
const unsigned kProcessingImportFlag = 4;

}  // namespace

Scope::Scope(const Settings* settings)
    : const_containing_(NULL),
      mutable_containing_(NULL),
      settings_(settings),
      mode_flags_(0) {
}

Scope::Scope(Scope* parent)
    : const_containing_(NULL),
      mutable_containing_(parent),
      settings_(parent->settings()),
      mode_flags_(0) {
}

Scope::Scope(const Scope* parent)
    : const_containing_(parent),
      mutable_containing_(NULL),
      settings_(parent->settings()),
      mode_flags_(0) {
}

Scope::~Scope() {
  STLDeleteContainerPairSecondPointers(target_defaults_.begin(),
                                       target_defaults_.end());
}

const Value* Scope::GetValue(const base::StringPiece& ident,
                             bool counts_as_used) {
  // First check for programatically-provided values.
  for (ProviderSet::const_iterator i = programmatic_providers_.begin();
       i != programmatic_providers_.end(); ++i) {
    const Value* v = (*i)->GetProgrammaticValue(ident);
    if (v)
      return v;
  }

  RecordMap::iterator found = values_.find(ident);
  if (found != values_.end()) {
    if (counts_as_used)
      found->second.used = true;
    return &found->second.value;
  }

  // Search in the parent scope.
  if (const_containing_)
    return const_containing_->GetValue(ident);
  if (mutable_containing_)
    return mutable_containing_->GetValue(ident, counts_as_used);
  return NULL;
}

Value* Scope::GetValueForcedToCurrentScope(const base::StringPiece& ident,
                                           const ParseNode* set_node) {
  RecordMap::iterator found = values_.find(ident);
  if (found != values_.end())
    return &found->second.value;  // Already have in the current scope.

  // Search in the parent scope.
  if (containing()) {
    const Value* in_containing = containing()->GetValue(ident);
    if (in_containing) {
      // Promote to current scope.
      return SetValue(ident, *in_containing, set_node);
    }
  }
  return NULL;
}

const Value* Scope::GetValue(const base::StringPiece& ident) const {
  RecordMap::const_iterator found = values_.find(ident);
  if (found != values_.end())
    return &found->second.value;
  if (containing())
    return containing()->GetValue(ident);
  return NULL;
}

Value* Scope::SetValue(const base::StringPiece& ident,
                       const Value& v,
                       const ParseNode* set_node) {
  Record& r = values_[ident];  // Clears any existing value.
  r.value = v;
  r.value.set_origin(set_node);
  return &r.value;
}

bool Scope::AddTemplate(const std::string& name, const FunctionCallNode* decl) {
  if (GetTemplate(name))
    return false;
  templates_[name] = decl;
  return true;
}

const FunctionCallNode* Scope::GetTemplate(const std::string& name) const {
  TemplateMap::const_iterator found = templates_.find(name);
  if (found != templates_.end())
    return found->second;
  if (containing())
    return containing()->GetTemplate(name);
  return NULL;
}

void Scope::MarkUsed(const base::StringPiece& ident) {
  RecordMap::iterator found = values_.find(ident);
  if (found == values_.end()) {
    NOTREACHED();
    return;
  }
  found->second.used = true;
}

void Scope::MarkUnused(const base::StringPiece& ident) {
  RecordMap::iterator found = values_.find(ident);
  if (found == values_.end()) {
    NOTREACHED();
    return;
  }
  found->second.used = false;
}

bool Scope::IsSetButUnused(const base::StringPiece& ident) const {
  RecordMap::const_iterator found = values_.find(ident);
  if (found != values_.end()) {
    if (!found->second.used) {
      return true;
    }
  }
  return false;
}

bool Scope::CheckForUnusedVars(Err* err) const {
  for (RecordMap::const_iterator i = values_.begin();
       i != values_.end(); ++i) {
    if (!i->second.used) {
      std::string help = "You set the variable \"" + i->first.as_string() +
          "\" here and it was unused before it went\nout of scope.";

      const BinaryOpNode* binary = i->second.value.origin()->AsBinaryOp();
      if (binary) {
        // Make a nicer error message for normal var sets.
        *err = Err(binary->left()->GetRange(), "Assignment had no effect.",
                   help);
      } else {
        // This will happen for internally-generated variables.
        *err = Err(i->second.value.origin(), "Assignment had no effect.", help);
      }
      return false;
    }
  }
  return true;
}

void Scope::GetCurrentScopeValues(KeyValueVector* output) const {
  output->reserve(values_.size());
  for (RecordMap::const_iterator i = values_.begin(); i != values_.end(); ++i) {
    output->push_back(std::make_pair(i->first, i->second.value));
  }
}

bool Scope::NonRecursiveMergeTo(Scope* dest,
                                const ParseNode* node_for_err,
                                const char* desc_for_err,
                                Err* err) const {
  // Values.
  for (RecordMap::const_iterator i = values_.begin(); i != values_.end(); ++i) {
    const Value* existing_value = dest->GetValue(i->first);
    if (existing_value) {
      // Value present in both the source and the dest.
      std::string desc_string(desc_for_err);
      *err = Err(node_for_err, "Value collision.",
          "This " + desc_string + " contains \"" + i->first.as_string() + "\"");
      err->AppendSubErr(Err(i->second.value, "defined here.",
          "Which would clobber the one in your current scope"));
      err->AppendSubErr(Err(*existing_value, "defined here.",
          "Executing " + desc_string + " should not conflict with anything "
          "in the current\nscope."));
      return false;
    }
    dest->values_[i->first] = i->second;
  }

  // Target defaults are owning pointers.
  for (NamedScopeMap::const_iterator i = target_defaults_.begin();
       i != target_defaults_.end(); ++i) {
    if (dest->GetTargetDefaults(i->first)) {
      // TODO(brettw) it would be nice to know the origin of a
      // set_target_defaults so we can give locations for the colliding target
      // defaults.
      std::string desc_string(desc_for_err);
      *err = Err(node_for_err, "Target defaults collision.",
          "This " + desc_string + " contains target defaults for\n"
          "\"" + i->first + "\" which would clobber one for the\n"
          "same target type in your current scope. It's unfortunate that I'm "
          "too stupid\nto tell you the location of where the target defaults "
          "were set. Usually\nthis happens in the BUILDCONFIG.gn file.");
      return false;
    }

    Scope* s = new Scope(settings_);
    i->second->NonRecursiveMergeTo(s, node_for_err, "<SHOULDN'T HAPPEN>", err);
    dest->target_defaults_[i->first] = s;
  }

  // Sources assignment filter.
  if (sources_assignment_filter_) {
    if (dest->GetSourcesAssignmentFilter()) {
      // Sources assignment filter present in both the source and the dest.
      std::string desc_string(desc_for_err);
      *err = Err(node_for_err, "Assignment filter collision.",
          "The " + desc_string + " contains a sources_assignment_filter which\n"
          "would clobber the one in your current scope.");
      return false;
    }
    dest->sources_assignment_filter_.reset(
        new PatternList(*sources_assignment_filter_));
  }

  // Templates.
  for (TemplateMap::const_iterator i = templates_.begin();
       i != templates_.end(); ++i) {
    const FunctionCallNode* existing_template = dest->GetTemplate(i->first);
    if (existing_template) {
      // Rule present in both the source and the dest.
      std::string desc_string(desc_for_err);
      *err = Err(node_for_err, "Template collision.",
          "This " + desc_string + " contains a template \"" + i->first + "\"");
      err->AppendSubErr(Err(i->second->function(), "defined here.",
          "Which would clobber the one in your current scope"));
      err->AppendSubErr(Err(existing_template->function(), "defined here.",
          "Executing " + desc_string + " should not conflict with anything "
          "in the current\nscope."));
      return false;
    }
    dest->templates_.insert(*i);
  }

  return true;
}

Scope* Scope::MakeTargetDefaults(const std::string& target_type) {
  if (GetTargetDefaults(target_type))
    return NULL;

  Scope** dest = &target_defaults_[target_type];
  if (*dest) {
    NOTREACHED();  // Already set.
    return *dest;
  }
  *dest = new Scope(settings_);
  return *dest;
}

const Scope* Scope::GetTargetDefaults(const std::string& target_type) const {
  NamedScopeMap::const_iterator found = target_defaults_.find(target_type);
  if (found != target_defaults_.end())
    return found->second;
  if (containing())
    return containing()->GetTargetDefaults(target_type);
  return NULL;
}

const PatternList* Scope::GetSourcesAssignmentFilter() const {
  if (sources_assignment_filter_)
    return sources_assignment_filter_.get();
  if (containing())
    return containing()->GetSourcesAssignmentFilter();
  return NULL;
}

void Scope::SetProcessingBuildConfig() {
  DCHECK((mode_flags_ & kProcessingBuildConfigFlag) == 0);
  mode_flags_ |= kProcessingBuildConfigFlag;
}

void Scope::ClearProcessingBuildConfig() {
  DCHECK(mode_flags_ & kProcessingBuildConfigFlag);
  mode_flags_ &= ~(kProcessingBuildConfigFlag);
}

bool Scope::IsProcessingBuildConfig() const {
  if (mode_flags_ & kProcessingBuildConfigFlag)
    return true;
  if (containing())
    return containing()->IsProcessingBuildConfig();
  return false;
}

void Scope::SetProcessingDefaultBuildConfig() {
  DCHECK((mode_flags_ & kProcessingDefaultBuildConfigFlag) == 0);
  mode_flags_ |= kProcessingDefaultBuildConfigFlag;
}

void Scope::ClearProcessingDefaultBuildConfig() {
  DCHECK(mode_flags_ & kProcessingDefaultBuildConfigFlag);
  mode_flags_ &= ~(kProcessingDefaultBuildConfigFlag);
}

bool Scope::IsProcessingDefaultBuildConfig() const {
  if (mode_flags_ & kProcessingDefaultBuildConfigFlag)
    return true;
  if (containing())
    return containing()->IsProcessingDefaultBuildConfig();
  return false;
}

void Scope::SetProcessingImport() {
  DCHECK((mode_flags_ & kProcessingImportFlag) == 0);
  mode_flags_ |= kProcessingImportFlag;
}

void Scope::ClearProcessingImport() {
  DCHECK(mode_flags_ & kProcessingImportFlag);
  mode_flags_ &= ~(kProcessingImportFlag);
}

bool Scope::IsProcessingImport() const {
  if (mode_flags_ & kProcessingImportFlag)
    return true;
  if (containing())
    return containing()->IsProcessingImport();
  return false;
}

void Scope::SetProperty(const void* key, void* value) {
  if (!value) {
    DCHECK(properties_.find(key) != properties_.end());
    properties_.erase(key);
  } else {
    properties_[key] = value;
  }
}

void* Scope::GetProperty(const void* key, const Scope** found_on_scope) const {
  PropertyMap::const_iterator found = properties_.find(key);
  if (found != properties_.end()) {
    if (found_on_scope)
      *found_on_scope = this;
    return found->second;
  }
  if (containing())
    return containing()->GetProperty(key, found_on_scope);
  return NULL;
}

void Scope::AddProvider(ProgrammaticProvider* p) {
  programmatic_providers_.insert(p);
}

void Scope::RemoveProvider(ProgrammaticProvider* p) {
  DCHECK(programmatic_providers_.find(p) != programmatic_providers_.end());
  programmatic_providers_.erase(p);
}
