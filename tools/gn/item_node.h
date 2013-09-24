// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ITEM_NODE_H_
#define TOOLS_GN_ITEM_NODE_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/location.h"

class BuildSettings;
class Err;
class Item;

// Represents a node in the depdency tree. It references an Item which is
// the actual thing.
//
// The items and nodes are split apart so that the ItemTree can manipulate
// the dependencies one one thread while the Item itself is been modified on
// another.
class ItemNode {
 public:
  // The state of this node. As more of the load progresses, we'll move
  // downward in this list toward the resolved state.
  enum State {
    // Another item has referenced this one by name, but we have not yet
    // encountered its definition.
    REFERENCED = 0,

    // We've seen the definition of this item but have not requested that its
    // dependencies be loaded. In non-greedy generation mode (see item_tree.h)
    // some nodes will stay in this state forever as long as they're not needed
    // for anything that is required.
    DEFINED,

    // The item has been defined and we've requested that all of the
    // dependencies be loaded. Not all of the dependencies have been resolved,
    // however, and we're still waiting on some build files to be run (or
    // perhaps there are undefined dependencies).
    PENDING_DEPS,

    // All of this item's transitive dependencies have been found and
    // resolved.
    RESOLVED,
  };

  // Stores a set of ItemNodes and the associated range that the dependency
  // was added from.
  typedef std::map<ItemNode*, LocationRange> ItemNodeMap;

  // Takes ownership of the pointer.
  // Initial state will be REFERENCED.
  ItemNode(Item* i);
  ~ItemNode();

  State state() const { return state_; }

  // This closure will be executed when the item is resolved and it has the
  // should_generate flag set.
  void set_resolved_closure(const base::Closure& closure) {
    resolved_closure_ = closure;
  }

  const Item* item() const { return item_.get(); }
  Item* item() { return item_.get(); }

  // True if this item should be generated. In greedy mode, this will alwyas
  // be set. Otherwise, this bit will be "pushed" through the tree to
  // generate the minimum set of targets required for some special base target.
  // Initialized to false.
  //
  // If this item has been defined, setting this flag will schedule the load
  // of dependent nodes and also set their should_generate bits.
  bool should_generate() const { return should_generate_; }
  bool SetShouldGenerate(const BuildSettings* build_settings, Err* err);

  // Where this was created from, which might be when it was generated or
  // when it was first referenced from another target.
  const LocationRange& originally_referenced_from_here() const {
    return originally_referenced_from_here_;
  }
  void set_originally_referenced_from_here(const LocationRange& r) {
    originally_referenced_from_here_ = r;
  }

  // Where this was generated from. This will be empty for items that have
  // been referenced but not generated. Note that this has to be one the
  // ItemNode because it can be changing from multiple threads and we need
  // to be sure that access is serialized.
  const LocationRange& generated_from_here() const {
    return generated_from_here_;
  }
  void set_generated_from_here(const LocationRange& r) {
    generated_from_here_ = r;
  }

  const ItemNodeMap& direct_dependencies() const {
    return direct_dependencies_;
  }
  const ItemNodeMap& unresolved_dependencies() const {
    return unresolved_dependencies_;
  }

  bool AddDependency(const BuildSettings* build_settings,
                     const LocationRange& specified_from_here,
                     ItemNode* node,
                     Err* err);

  // Removes the given dependency from the unresolved list. Does not do
  // anything else to update waiters.
  void MarkDirectDependencyResolved(ItemNode* node);

  // Destructively retrieve the set of waiting nodes.
  void SwapOutWaitingDependencySet(ItemNodeMap* out_map);

  // Marks this item state as defined (see above). If the should generate
  // flag is set, this will schedule a load of the dependencies and
  // automatically transition to the PENDING_DEPS state.
  bool SetDefined(const BuildSettings* build_settings, Err* err);

  // Marks this item state as resolved (see above).
  void SetResolved();

 private:
  // Schedules loading the dependencies of this node. The current state must
  // be DEFINED, and this call will transition the state to PENDING_DEPS.
  //
  // Requesting deps can fail. On failure returns false and sets the err.
  bool ScheduleDepsLoad(const BuildSettings* build_settings, Err* err);

  State state_;
  scoped_ptr<Item> item_;
  bool should_generate_;  // See getter above.

  LocationRange originally_referenced_from_here_;
  LocationRange generated_from_here_;

  // What to run when this item is resolved.
  base::Closure resolved_closure_;

  // Everything this item directly depends on.
  ItemNodeMap direct_dependencies_;

  // Unresolved things this item directly depends on.
  ItemNodeMap unresolved_dependencies_;

  // These items are waiting on us to be resolved before they can be
  // resolved. This is the backpointer for unresolved_dependencies_.
  ItemNodeMap waiting_on_resolution_;

  DISALLOW_COPY_AND_ASSIGN(ItemNode);
};

#endif  // TOOLS_GN_ITEM_NODE_H_
