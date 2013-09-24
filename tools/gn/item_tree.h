// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ITEM_TREE_H_
#define TOOLS_GN_ITEM_TREE_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "tools/gn/label.h"

class BuildSettings;
class Err;
class Item;
class ItemNode;

// Represents the full dependency tree if labeled items in the system.
// Generally you will interact with this through the target manager, etc.
//
// There are two modes for filling out the dependency tree:
//
// - In greedy mode, every target we encounter will be generated. This means
//   that we'll recursively load all of its subdependencies. So if you have
//   a build file that's loaded for any reason, all targets in that build file
//   will be generated.
//
// - In non-greedy mode, we'll only generate and load dependncies for targets
//   that have the should_generate bit set. This allows us to load the minimal
//   set of buildfiles required for one or more targets.
//
// The main build is generally run in greedy mode, since people expect to be
// be able to write random tests and have them show up in the output. We'll
// switch into non-greed mode when doing diagnostics (like displaying the
// dependency tree on the command line) and for dependencies on targets in
// other toolchains. The toolchain behavior is important, if target A depends
// on B with an alternate toolchain, it doesn't mean we should recursively
// generate all targets in the buildfile just to get B: we should generate the
// and load the minimum number of files in order to resolve B.
class ItemTree {
 public:
  ItemTree();
  ~ItemTree();

  // This lock must be held when calling the "Locked" functions below.
  base::Lock& lock() const { return lock_; }

  // Returns NULL if the item is not found.
  //
  // The lock must be held.
  ItemNode* GetExistingNodeLocked(const Label& label);

  // There must not be an item with this label in the tree already. Takes
  // ownership of the pointer.
  //
  // The lock must be held.
  void AddNodeLocked(ItemNode* node);

  // Mark the given item as being defined. If it has no unresolved
  // dependencies, it will be marked resolved, and the resolved state will be
  // recursively pushed into the dependency tree. Returns an error if there was
  // an error.
  bool MarkItemDefinedLocked(const BuildSettings* build_settings,
                             const Label& label,
                             Err* err);

  // Fills the given vector with all known items.
  void GetAllItemsLocked(std::vector<const Item*>* dest) const;

  // Returns an error if there are unresolved dependencies, or no error if
  // there aren't.
  //
  // The lock should not be held.
  Err CheckForBadItems() const;

 private:
  void MarkItemResolvedLocked(ItemNode* node);

  // Given a set of unresolved nodes, looks for cycles and returns the error
  // message describing any cycles it found.
  std::string CheckForCircularDependenciesLocked(
      const std::vector<const ItemNode*>& bad_nodes) const;

  mutable base::Lock lock_;

  typedef base::hash_map<Label, ItemNode*> StringToNodeHash;
  StringToNodeHash items_;  // Owning pointer.

  DISALLOW_COPY_AND_ASSIGN(ItemTree);
};

#endif  // TOOLS_GN_ITEM_TREE_H_
