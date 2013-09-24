// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target.h"

#include "base/bind.h"
#include "tools/gn/scheduler.h"

namespace {

void TargetResolvedThunk(const base::Callback<void(const Target*)>& cb,
                         const Target* t) {
  cb.Run(t);
}

}  // namespace

Target::Target(const Settings* settings, const Label& label)
    : Item(label),
      settings_(settings),
      output_type_(UNKNOWN),
      generated_(false),
      generator_function_(NULL) {
}

Target::~Target() {
}

Target* Target::AsTarget() {
  return this;
}

const Target* Target::AsTarget() const {
  return this;
}

void Target::OnResolved() {
  // Gather info from our dependents we need.
  for (size_t dep = 0; dep < deps_.size(); dep++) {
    // All dependent configs get pulled to us, and to our dependents.
    const std::vector<const Config*>& all =
        deps_[dep]->all_dependent_configs();
    for (size_t i = 0; i < all.size(); i++) {
      configs_.push_back(all[i]);
      all_dependent_configs_.push_back(all[i]);
    }

    // Direct dependent configs get pulled only to us.
    const std::vector<const Config*>& direct =
        deps_[dep]->direct_dependent_configs();
    for (size_t i = 0; i < direct.size(); i++)
      configs_.push_back(direct[i]);

    // Direct dependent libraries.
    if (deps_[dep]->output_type() == STATIC_LIBRARY ||
        deps_[dep]->output_type() == SHARED_LIBRARY)
      inherited_libraries_.insert(deps_[dep]);

    // Inherited libraries. Don't pull transitive libraries from shared
    // libraries, since obviously those shouldn't be linked directly into
    // later deps unless explicitly specified.
    if (deps_[dep]->output_type() != SHARED_LIBRARY &&
        deps_[dep]->output_type() != EXECUTABLE) {
      const std::set<const Target*> inherited =
          deps_[dep]->inherited_libraries();
      for (std::set<const Target*>::const_iterator i = inherited.begin();
           i != inherited.end(); ++i)
        inherited_libraries_.insert(*i);
    }
  }

  if (!settings_->build_settings()->target_resolved_callback().is_null()) {
    g_scheduler->ScheduleWork(base::Bind(&TargetResolvedThunk,
        settings_->build_settings()->target_resolved_callback(),
        this));
  }
}

bool Target::HasBeenGenerated() const {
  return generated_;
}

void Target::SetGenerated(const Token* token) {
  DCHECK(!generated_);
  generated_ = true;
  generator_function_ = token;
}

bool Target::IsLinkable() const {
  return output_type_ == STATIC_LIBRARY || output_type_ == SHARED_LIBRARY;
}
