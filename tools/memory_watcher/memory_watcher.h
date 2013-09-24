// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryWatcher.
// The MemoryWatcher is a library that can be linked into any
// win32 application.  It will override the default memory allocators
// and track call stacks for any allocations that are made.  It can
// then be used to see what memory is in use.

#ifndef TOOLS_MEMORY_WATCHER_MEMORY_WATCHER_
#define TOOLS_MEMORY_WATCHER_MEMORY_WATCHER_

#include <map>
#include <functional>

#include "base/synchronization/lock.h"
#include "tools/memory_watcher/memory_hook.h"

class CallStack;
class AllocationStack;

// The MemoryWatcher installs allocation hooks and monitors
// allocations and frees.
class MemoryWatcher : MemoryObserver {
 public:
  struct StackTrack {
    CallStack* stack;
    int count;
    int size;
  };

  typedef std::map<int32, AllocationStack*, std::less<int32>,
                   PrivateHookAllocator<int32> > CallStackMap;
  typedef std::map<int32, StackTrack, std::less<int32>,
                   PrivateHookAllocator<int32> > CallStackIdMap;
  typedef std::basic_string<char, std::char_traits<char>,
                            PrivateHookAllocator<char> > PrivateAllocatorString;

  MemoryWatcher();
  virtual ~MemoryWatcher();

  // Dump all tracked pointers still in use.
  void DumpLeaks();

  // MemoryObserver interface.
  virtual void OnTrack(HANDLE heap, int32 id, int32 size);
  virtual void OnUntrack(HANDLE heap, int32 id, int32 size);

  // Sets a name that appears in the generated file name.
  void SetLogName(char* log_name);

 private:
  // Opens the logfile which we create.
  void OpenLogFile();

  // Close the logfile.
  void CloseLogFile();

  // Hook the memory hooks.
  void Hook();

  // Unhooks our memory hooks.
  void Unhook();

  // Check to see if this thread is already processing a block, and should not
  // recurse.
  bool LockedRecursionDetected() const;

  // This is for logging.
  FILE* file_;

  bool hooked_;  // True when this class has the memory_hooks hooked.

  // Either 0, or else the threadID for a thread that is actively working on
  // a stack track.  Used to avoid recursive tracking.
  DWORD active_thread_id_;

  base::Lock block_map_lock_;
  // The block_map provides quick lookups based on the allocation
  // pointer.  This is important for having fast round trips through
  // malloc/free.
  CallStackMap *block_map_;

  // The file name for that log.
  std::string file_name_;

  // An optional name that appears in the log file name (used to differentiate
  // logs).
  std::string log_name_;
};



#endif  // TOOLS_MEMORY_WATCHER_MEMORY_WATCHER_
