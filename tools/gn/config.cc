// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/config.h"

#include "tools/gn/err.h"
#include "tools/gn/input_file_manager.h"
#include "tools/gn/item_node.h"
#include "tools/gn/item_tree.h"
#include "tools/gn/scheduler.h"

Config::Config(const Label& label) : Item(label) {
}

Config::~Config() {
}

Config* Config::AsConfig() {
  return this;
}

const Config* Config::AsConfig() const {
  return this;
}

// static
Config* Config::GetConfig(const Settings* settings,
                          const LocationRange& specified_from_here,
                          const Label& label,
                          Item* dep_from,
                          Err* err) {
  DCHECK(!label.is_null());

  ItemTree* tree = &settings->build_settings()->item_tree();
  base::AutoLock lock(tree->lock());

  ItemNode* node = tree->GetExistingNodeLocked(label);
  Config* config = NULL;
  if (!node) {
    config = new Config(label);
    node = new ItemNode(config);
    tree->AddNodeLocked(node);

    // Only schedule loading the given target if somebody is depending on it
    // (and we optimize by not re-asking it to run the current file).
    // Otherwise, we're probably generating it right now.
    if (dep_from && dep_from->label().dir() != label.dir()) {
      settings->build_settings()->toolchain_manager().ScheduleInvocationLocked(
          specified_from_here, label.GetToolchainLabel(), label.dir(),
          err);
    }
  } else if ((config = node->item()->AsConfig())) {
    // Previously saw this item as a config.

    // If we have no dep_from, we're generating it. In this case, it had better
    // not already be generated.
    if (!dep_from && node->state() != ItemNode::REFERENCED) {
      *err = Err(specified_from_here, "Duplicate config definition.",
          "You already told me about a config with this name.");
      return NULL;
    }
  } else {
    // Previously saw this thing as a non-config.
    *err = Err(specified_from_here,
               "Config name already used.",
               "Previously you specified a " +
               node->item()->GetItemTypeName() + " with this name instead.");
    return NULL;
  }

  // Keep a record of the guy asking us for this dependency. We know if
  // somebody is adding a dependency, that guy it himself not resolved.
  if (dep_from) {
    if (!tree->GetExistingNodeLocked(dep_from->label())->AddDependency(
            settings->build_settings(), specified_from_here, node, err))
      return NULL;
  }
  return config;
}
