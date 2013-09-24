// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_INPUT_FILE_MANAGER_H_
#define TOOLS_GN_INPUT_FILE_MANAGER_H_

#include <set>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/settings.h"

class Err;
class LocationRange;
class ParseNode;
class Token;

// Manages loading and parsing files from disk. This doesn't actually have
// any context for executing the results, so potentially multiple configs
// could use the same input file (saving parsing).
//
// This class is threadsafe.
//
// InputFile objects must never be deleted while the program is running since
// various state points into them.
class InputFileManager : public base::RefCountedThreadSafe<InputFileManager> {
 public:
  // Callback issued when a file is laoded. On auccess, the parse node will
  // refer to the root block of the file. On failure, this will be NULL.
  typedef base::Callback<void(const ParseNode*)> FileLoadCallback;

  InputFileManager();

  // Loads the given file and executes the callback on the worker pool.
  //
  // There are two types of errors. For errors known synchronously, the error
  // will be set, it will return false, and no work will be scheduled.
  //
  // For parse errors and such that happen in the future, the error will be
  // logged to the scheduler and the callback will be invoked with a null
  // ParseNode pointer. The given |origin| will be blamed for the invocation.
  bool AsyncLoadFile(const LocationRange& origin,
                     const BuildSettings* build_settings,
                     const SourceFile& file_name,
                     const FileLoadCallback& callback,
                     Err* err);

  // Loads and parses the given file synchronously, returning the root block
  // corresponding to the parsed result. On error, return NULL and the given
  // Err is set.
  const ParseNode* SyncLoadFile(const LocationRange& origin,
                                const BuildSettings* build_settings,
                                const SourceFile& file_name,
                                Err* err);

  int GetInputFileCount() const;

  // Fills the vector with all input files.
  void GetAllPhysicalInputFileNames(std::vector<base::FilePath>* result) const;

 private:
  friend class base::RefCountedThreadSafe<InputFileManager>;

  struct InputFileData {
    InputFileData(const SourceFile& file_name);
    ~InputFileData();

    // Don't touch this outside the lock until it's marked loaded.
    InputFile file;

    bool loaded;

    bool sync_invocation;

    // Lists all invocations that need to be executed when the file completes
    // loading.
    std::vector<FileLoadCallback> scheduled_callbacks;

    // Event to signal when the load is complete (or fails). This is lazily
    // created only when a thread is synchronously waiting for this load (which
    // only happens for imports).
    scoped_ptr<base::WaitableEvent> completion_event;

    std::vector<Token> tokens;

    // Null before the file is loaded or if loading failed.
    scoped_ptr<ParseNode> parsed_root;
  };

  virtual ~InputFileManager();

  void BackgroundLoadFile(const LocationRange& origin,
                          const BuildSettings* build_settings,
                          const SourceFile& name,
                          InputFile* file);

  // Loads the given file. On error, sets the Err and return false.
  bool LoadFile(const LocationRange& origin,
                const BuildSettings* build_settings,
                const SourceFile& name,
                InputFile* file,
                Err* err);

  mutable base::Lock lock_;

  // Maps repo-relative filenames to the corresponding owned pointer.
  typedef base::hash_map<SourceFile, InputFileData*> InputFileMap;
  InputFileMap input_files_;

  DISALLOW_COPY_AND_ASSIGN(InputFileManager);
};

#endif  // TOOLS_GN_INPUT_FILE_MANAGER_H_
