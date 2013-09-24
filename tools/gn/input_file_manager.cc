// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/input_file_manager.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parser.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope_per_file_provider.h"
#include "tools/gn/tokenizer.h"

namespace {

void InvokeFileLoadCallback(const InputFileManager::FileLoadCallback& cb,
                            const ParseNode* node) {
  cb.Run(node);
}

}  // namespace

InputFileManager::InputFileData::InputFileData(const SourceFile& file_name)
    : file(file_name),
      loaded(false),
      sync_invocation(false) {
}

InputFileManager::InputFileData::~InputFileData() {
}

InputFileManager::InputFileManager() {
}

InputFileManager::~InputFileManager() {
  // Should be single-threaded by now.
  STLDeleteContainerPairSecondPointers(input_files_.begin(),
                                       input_files_.end());
}

bool InputFileManager::AsyncLoadFile(const LocationRange& origin,
                                     const BuildSettings* build_settings,
                                     const SourceFile& file_name,
                                     const FileLoadCallback& callback,
                                     Err* err) {
  // Try not to schedule callbacks while holding the lock. All cases that don't
  // want to schedule should return early. Otherwise, this will be scheduled
  // after we leave the lock.
  base::Closure schedule_this;
  {
    base::AutoLock lock(lock_);

    InputFileMap::const_iterator found = input_files_.find(file_name);
    if (found == input_files_.end()) {
      // New file, schedule load.
      InputFileData* data = new InputFileData(file_name);
      data->scheduled_callbacks.push_back(callback);
      input_files_[file_name] = data;

      schedule_this = base::Bind(&InputFileManager::BackgroundLoadFile,
                                 this,
                                 origin,
                                 build_settings,
                                 file_name,
                                 &data->file);
    } else {
      InputFileData* data = found->second;

      // Prevent mixing async and sync loads. See SyncLoadFile for discussion.
      if (data->sync_invocation) {
        g_scheduler->FailWithError(Err(
            origin, "Load type mismatch.",
            "The file \"" + file_name.value() + "\" was previously loaded\n"
            "synchronously (via an import) and now you're trying to load it "
            "asynchronously\n(via a deps rule). This is a class 2 misdemeanor: "
            "a single input file must\nbe loaded the same way each time to "
            "avoid blowing my tiny, tiny mind."));
        return false;
      }

      if (data->loaded) {
        // Can just directly issue the callback on the background thread.
        schedule_this = base::Bind(&InvokeFileLoadCallback, callback,
                                   data->parsed_root.get());
      } else {
        // Load is pending on this file, schedule the invoke.
        data->scheduled_callbacks.push_back(callback);
        return true;
      }
    }
  }
  g_scheduler->pool()->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE, schedule_this,
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  return true;
}

const ParseNode* InputFileManager::SyncLoadFile(
    const LocationRange& origin,
    const BuildSettings* build_settings,
    const SourceFile& file_name,
    Err* err) {
  base::AutoLock lock(lock_);

  InputFileData* data = NULL;
  InputFileMap::iterator found = input_files_.find(file_name);
  if (found == input_files_.end()) {
    base::AutoUnlock unlock(lock_);

    // Haven't seen this file yet, start loading right now.
    data = new InputFileData(file_name);
    data->sync_invocation = true;
    input_files_[file_name] = data;

    if (!LoadFile(origin, build_settings, file_name, &data->file, err))
      return NULL;
  } else {
    // This file has either been loaded or is pending loading.
    data = found->second;

    if (!data->sync_invocation) {
      // Don't allow mixing of sync and async loads. If an async load is
      // scheduled and then a bunch of threads need to load it synchronously
      // and block on it loading, it could deadlock or at least cause a lot
      // of wasted CPU while those threads wait for the load to complete (which
      // may be far back in the input queue).
      //
      // We could work around this by promoting the load to a sync load. This
      // requires a bunch of extra code to either check flags and likely do
      // extra locking (bad) or to just do both types of load on the file and
      // deal with the race condition.
      //
      // I have no practical way to test this, and generally we should have
      // all include files processed synchronously and all build files
      // processed asynchronously, so it doesn't happen in practice.
      *err = Err(
          origin, "Load type mismatch.",
          "The file \"" + file_name.value() + "\" was previously loaded\n"
          "asynchronously (via a deps rule) and now you're trying to load it "
          "synchronously.\nThis is a class 2 misdemeanor: a single input file "
          "must be loaded the same way\neach time to avoid blowing my tiny, "
          "tiny mind.");
      return NULL;
    }

    if (!data->loaded) {
      // Wait for the already-pending sync load to complete.
      if (!data->completion_event)
        data->completion_event.reset(new base::WaitableEvent(false, false));
      {
        base::AutoUnlock unlock(lock_);
        data->completion_event->Wait();
      }
    }
  }

  // The other load could have failed. In this case that error will be printed
  // to the console, but we need to return something here, so make up a
  // dummy error.
  if (!data->parsed_root)
    *err = Err(origin, "File parse failed");
  return data->parsed_root.get();
}

int InputFileManager::GetInputFileCount() const {
  base::AutoLock lock(lock_);
  return input_files_.size();
}

void InputFileManager::GetAllPhysicalInputFileNames(
    std::vector<base::FilePath>* result) const {
  base::AutoLock lock(lock_);
  result->reserve(input_files_.size());
  for (InputFileMap::const_iterator i = input_files_.begin();
       i != input_files_.end(); ++i) {
    if (!i->second->file.physical_name().empty())
      result->push_back(i->second->file.physical_name());
  }
}

void InputFileManager::BackgroundLoadFile(const LocationRange& origin,
                                          const BuildSettings* build_settings,
                                          const SourceFile& name,
                                          InputFile* file) {
  Err err;
  if (!LoadFile(origin, build_settings, name, file, &err))
    g_scheduler->FailWithError(err);
}

bool InputFileManager::LoadFile(const LocationRange& origin,
                                const BuildSettings* build_settings,
                                const SourceFile& name,
                                InputFile* file,
                                Err* err) {
  // Do all of this stuff outside the lock. We should not give out file
  // pointers until the read is complete.
  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Loading", name.value());

  // Read.
  base::FilePath primary_path = build_settings->GetFullPath(name);
  if (!file->Load(primary_path)) {
    if (!build_settings->secondary_source_path().empty()) {
      // Fall back to secondary source tree.
      base::FilePath secondary_path =
          build_settings->GetFullPathSecondary(name);
      if (!file->Load(secondary_path)) {
        *err = Err(origin, "Can't load input file.",
                   "Unable to load either \n" +
                   FilePathToUTF8(primary_path) + " or \n" +
                   FilePathToUTF8(secondary_path));
        return false;
      }
    } else {
      *err = Err(origin,
                 "Unable to load \"" + FilePathToUTF8(primary_path) + "\".");
      return false;
    }
  }

  // Tokenize.
  std::vector<Token> tokens = Tokenizer::Tokenize(file, err);
  if (err->has_error())
    return false;

  // Parse.
  scoped_ptr<ParseNode> root = Parser::Parse(tokens, err);
  if (err->has_error())
    return false;
  ParseNode* unowned_root = root.get();

  std::vector<FileLoadCallback> callbacks;
  {
    base::AutoLock lock(lock_);
    DCHECK(input_files_.find(name) != input_files_.end());

    InputFileData* data = input_files_[name];
    data->loaded = true;
    data->tokens.swap(tokens);
    data->parsed_root = root.Pass();

    callbacks.swap(data->scheduled_callbacks);
  }

  // Run pending invocations. Theoretically we could schedule each of these
  // separately to get some parallelism. But normally there will only be one
  // item in the list, so that's extra overhead and complexity for no gain.
  for (size_t i = 0; i < callbacks.size(); i++)
    callbacks[i].Run(unowned_root);
  return true;
}
