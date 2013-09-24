// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/import_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"

namespace {

// Returns a newly-allocated scope on success, null on failure.
Scope* UncachedImport(const Settings* settings,
                      const SourceFile& file,
                      const ParseNode* node_for_err,
                      Err* err) {
  const ParseNode* node = g_scheduler->input_file_manager()->SyncLoadFile(
      node_for_err->GetRange(), settings->build_settings(), file, err);
  if (!node)
    return NULL;
  const BlockNode* block = node->AsBlock();
  CHECK(block);

  scoped_ptr<Scope> scope(new Scope(settings->base_config()));
  scope->SetProcessingImport();
  block->ExecuteBlockInScope(scope.get(), err);
  if (err->has_error())
    return NULL;
  scope->ClearProcessingImport();

  return scope.release();
}

}  // namesapce

ImportManager::ImportManager() {
}

ImportManager::~ImportManager() {
  STLDeleteContainerPairSecondPointers(imports_.begin(), imports_.end());
}

bool ImportManager::DoImport(const SourceFile& file,
                             const ParseNode* node_for_err,
                             Scope* scope,
                             Err* err) {
  // See if we have a cached import, but be careful to actually do the scope
  // copying outside of the lock.
  const Scope* imported_scope = NULL;
  {
    base::AutoLock lock(lock_);
    ImportMap::const_iterator found = imports_.find(file);
    if (found != imports_.end())
      imported_scope = found->second;
  }

  if (!imported_scope) {
    // Do a new import of the file.
    imported_scope = UncachedImport(scope->settings(), file,
                                    node_for_err, err);
    if (!imported_scope)
      return false;

    // We loaded the file outside the lock. This means that there could be a
    // race and the file was already loaded on a background thread. Recover
    // from this and use the existing one if that happens.
    {
      base::AutoLock lock(lock_);
      ImportMap::const_iterator found = imports_.find(file);
      if (found != imports_.end()) {
        delete imported_scope;
        imported_scope = found->second;
      } else {
        imports_[file] = imported_scope;
      }
    }
  }

  return imported_scope->NonRecursiveMergeTo(scope, node_for_err,
                                             "import", err);
}
