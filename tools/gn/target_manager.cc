// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target_manager.h"

#include <deque>

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/item_node.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/toolchain_manager.h"
#include "tools/gn/value.h"

TargetManager::TargetManager(const BuildSettings* build_settings)
    : build_settings_(build_settings) {
}

TargetManager::~TargetManager() {
}

Target* TargetManager::GetTarget(const Label& label,
                                 const LocationRange& specified_from_here,
                                 Target* dep_from,
                                 Err* err) {
  DCHECK(!label.is_null());
  DCHECK(!label.toolchain_dir().value().empty());
  DCHECK(!label.toolchain_name().empty());

  base::AutoLock lock(build_settings_->item_tree().lock());

  ItemNode* target_node =
      build_settings_->item_tree().GetExistingNodeLocked(label);
  Target* target = NULL;
  if (!target_node) {
    // First time we've seen this, may need to load the file.

    // Compute the settings. The common case is that we have a dep_from and
    // the toolchains match, so we can use the settings from there rather than
    // querying the toolchain manager (which requires locking, etc.).
    const Settings* settings;
    if (dep_from && dep_from->label().ToolchainsEqual(label)) {
      settings = dep_from->settings();
    } else {
      settings =
          build_settings_->toolchain_manager().GetSettingsForToolchainLocked(
              specified_from_here, label.GetToolchainLabel(), err);
      if (!settings)
        return NULL;
    }

    target = new Target(settings, label);

    target_node = new ItemNode(target);
    if (settings->greedy_target_generation()) {
      if (!target_node->SetShouldGenerate(build_settings_, err))
        return NULL;
    }
    target_node->set_originally_referenced_from_here(specified_from_here);

    build_settings_->item_tree().AddNodeLocked(target_node);

    // We're generating a node when there is no referencing one.
    if (!dep_from)
      target_node->set_generated_from_here(specified_from_here);

  } else if ((target = target_node->item()->AsTarget())) {
    // Previously saw this item as a target.

    // If we have no dep_from, we're generating it.
    if (!dep_from) {
      // In this case, it had better not already be generated.
      if (target_node->state() != ItemNode::REFERENCED) {
        *err = Err(specified_from_here,
                   "Duplicate target.",
                   "\"" + label.GetUserVisibleName(true) +
                   "\" being defined here.");
        err->AppendSubErr(Err(target_node->generated_from_here(),
                              "Originally defined here."));
        return NULL;
      } else {
        target_node->set_generated_from_here(specified_from_here);
      }
    }
  } else {
    // Error, we previously saw this thing as a non-target.
    *err = Err(specified_from_here, "Not previously a target.",
        "The target being declared here was previously seen referenced as a\n"
        "non-target (like a config)");
    err->AppendSubErr(Err(target_node->originally_referenced_from_here(),
                          "Originally referenced from here."));
    return NULL;
  }

  // Keep a record of the guy asking us for this dependency. We know if
  // somebody is adding a dependency, that guy it himself not resolved.
  if (dep_from && target_node->state() != ItemNode::RESOLVED) {
    ItemNode* from_node =
        build_settings_->item_tree().GetExistingNodeLocked(dep_from->label());
    if (!from_node->AddDependency(build_settings_, specified_from_here,
                                  target_node, err))
      return NULL;
  }

  return target;
}

bool TargetManager::TargetGenerationComplete(const Label& label,
                                             Err* err) {
  base::AutoLock lock(build_settings_->item_tree().lock());
  return build_settings_->item_tree().MarkItemDefinedLocked(
      build_settings_, label, err);
}

void TargetManager::GetAllTargets(
    std::vector<const Target*>* all_targets) const {
  base::AutoLock lock(build_settings_->item_tree().lock());

  std::vector<const Item*> all_items;
  build_settings_->item_tree().GetAllItemsLocked(&all_items);
  for (size_t i = 0; i < all_items.size(); i++) {
    const Target* t = all_items[i]->AsTarget();
    if (t)
      all_targets->push_back(t);
  }
}
