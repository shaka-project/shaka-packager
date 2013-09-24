// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_IMPORT_MANAGER_H_
#define TOOLS_GN_IMPORT_MANAGER_H_

#include <map>

#include "base/synchronization/lock.h"

class Err;
class ParseNode;
class Scope;
class SourceFile;

// Provides a cache of the results of importing scopes so the results can
// be re-used rather than running the imported files multiple times.
class ImportManager {
 public:
  ImportManager();
  ~ImportManager();

  // Does an import of the given file into the given scope. On error, sets the
  // error and returns false.
  bool DoImport(const SourceFile& file,
                const ParseNode* node_for_err,
                Scope* scope,
                Err* err);

 private:
  base::Lock lock_;

  // Owning pointers to the scopes.
  typedef std::map<SourceFile, const Scope*> ImportMap;
  ImportMap imports_;

  DISALLOW_COPY_AND_ASSIGN(ImportManager);
};

#endif  // TOOLS_GN_IMPORT_MANAGER_H_
