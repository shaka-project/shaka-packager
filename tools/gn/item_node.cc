// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/item_node.h"

#include <algorithm>

#include "base/callback.h"
#include "base/logging.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/item.h"

ItemNode::ItemNode(Item* i)
    : state_(REFERENCED),
      item_(i),
      should_generate_(false) {
}

ItemNode::~ItemNode() {
}

bool ItemNode::SetShouldGenerate(const BuildSettings* build_settings,
                                 Err* err) {
  if (should_generate_)
    return true;
  should_generate_ = true;

  if (state_ == DEFINED) {
    if (!ScheduleDepsLoad(build_settings, err))
      return false;
  } else if (state_ == RESOLVED) {
    // The item may have been resolved even if we didn't set the generate bit
    // if all of its deps were loaded some other way. In this case, we need
    // to run the closure which we skipped when it became resolved.
    if (!resolved_closure_.is_null())
      resolved_closure_.Run();
  }

  // Pass the generate bit to all deps.
  for (ItemNodeMap::iterator i = direct_dependencies_.begin();
       i != direct_dependencies_.end(); ++i) {
    if (!i->first->SetShouldGenerate(build_settings, err))
      return false;
  }
  return true;
}

bool ItemNode::AddDependency(const BuildSettings* build_settings,
                             const LocationRange& specified_from_here,
                             ItemNode* node,
                             Err* err) {
  // Can't add more deps once it's been defined.
  DCHECK(state_ == REFERENCED);

  if (direct_dependencies_.find(node) != direct_dependencies_.end())
    return true;  // Already have this dep.

  direct_dependencies_[node] = specified_from_here;

  if (node->state() != RESOLVED) {
    // Wire up the pending resolution info.
    unresolved_dependencies_[node] = specified_from_here;
    node->waiting_on_resolution_[this] = specified_from_here;
  }

  if (should_generate_) {
    if (!node->SetShouldGenerate(build_settings, err))
      return false;
  }
  return true;
}

void ItemNode::MarkDirectDependencyResolved(ItemNode* node) {
  DCHECK(unresolved_dependencies_.find(node) != unresolved_dependencies_.end());
  unresolved_dependencies_.erase(node);
}

void ItemNode::SwapOutWaitingDependencySet(ItemNodeMap* out_map) {
  waiting_on_resolution_.swap(*out_map);
  DCHECK(waiting_on_resolution_.empty());
}

bool ItemNode::SetDefined(const BuildSettings* build_settings, Err* err) {
  DCHECK(state_ == REFERENCED);
  state_ = DEFINED;

  if (should_generate_)
    return ScheduleDepsLoad(build_settings, err);
  return true;
}

void ItemNode::SetResolved() {
  DCHECK(state_ != RESOLVED);
  state_ = RESOLVED;

  if (should_generate_ && !resolved_closure_.is_null())
    resolved_closure_.Run();
}

bool ItemNode::ScheduleDepsLoad(const BuildSettings* build_settings,
                                Err* err) {
  DCHECK(state_ == DEFINED);
  DCHECK(should_generate_);  // Shouldn't be requesting deps for ungenerated
                             // items.

  for (ItemNodeMap::const_iterator i = unresolved_dependencies_.begin();
       i != unresolved_dependencies_.end(); ++i) {
    Label toolchain_label = i->first->item()->label().GetToolchainLabel();
    SourceDir dir_to_load = i->first->item()->label().dir();

    if (!build_settings->toolchain_manager().ScheduleInvocationLocked(
            i->second, toolchain_label, dir_to_load, err))
      return false;
  }

  state_ = PENDING_DEPS;
  return true;
}
