// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <windows.h>
#include <tlhelp32.h>     // for CreateToolhelp32Snapshot()
#include <map>

#include "tools/memory_watcher/memory_watcher.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/metrics/stats_counters.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "tools/memory_watcher/call_stack.h"
#include "tools/memory_watcher/preamble_patcher.h"

static base::StatsCounter mem_in_use("MemoryInUse.Bytes");
static base::StatsCounter mem_in_use_blocks("MemoryInUse.Blocks");
static base::StatsCounter mem_in_use_allocs("MemoryInUse.Allocs");
static base::StatsCounter mem_in_use_frees("MemoryInUse.Frees");

// ---------------------------------------------------------------------

MemoryWatcher::MemoryWatcher()
  : file_(NULL),
    hooked_(false),
    active_thread_id_(0) {
  MemoryHook::Initialize();
  CallStack::Initialize();

  block_map_ = new CallStackMap();

  // Register last - only after we're ready for notifications!
  Hook();
}

MemoryWatcher::~MemoryWatcher() {
  Unhook();

  CloseLogFile();

  // Pointers in the block_map are part of the MemoryHook heap.  Be sure
  // to delete the map before closing the heap.
  delete block_map_;
}

void MemoryWatcher::Hook() {
  DCHECK(!hooked_);
  MemoryHook::RegisterWatcher(this);
  hooked_ = true;
}

void MemoryWatcher::Unhook() {
  if (hooked_) {
    MemoryHook::UnregisterWatcher(this);
    hooked_ = false;
  }
}

void MemoryWatcher::OpenLogFile() {
  DCHECK(file_ == NULL);
  file_name_ = "memwatcher";
  if (!log_name_.empty()) {
    file_name_ += ".";
    file_name_ += log_name_;
  }
  file_name_ += ".log";
  char buf[16];
  file_name_ += _itoa(GetCurrentProcessId(), buf, 10);

  std::string tmp_name(file_name_);
  tmp_name += ".tmp";
  file_ = fopen(tmp_name.c_str(), "w+");
}

void MemoryWatcher::CloseLogFile() {
  if (file_ != NULL) {
    fclose(file_);
    file_ = NULL;
    std::wstring tmp_name = ASCIIToWide(file_name_);
    tmp_name += L".tmp";
    base::Move(base::FilePath(tmp_name),
               base::FilePath(ASCIIToWide(file_name_)));
  }
}

bool MemoryWatcher::LockedRecursionDetected() const {
  if (!active_thread_id_) return false;
  DWORD thread_id = GetCurrentThreadId();
  // TODO(jar): Perchance we should use atomic access to member.
  return thread_id == active_thread_id_;
}

void MemoryWatcher::OnTrack(HANDLE heap, int32 id, int32 size) {
  // Don't track zeroes.  It's a waste of time.
  if (size == 0)
    return;

  if (LockedRecursionDetected())
    return;

  // AllocationStack overrides new/delete to not allocate
  // from the main heap.
  AllocationStack* stack = new AllocationStack(size);
  if (!stack->Valid()) return;  // Recursion blocked generation of stack.

  {
    base::AutoLock lock(block_map_lock_);

    // Ideally, we'd like to verify that the block being added
    // here is not already in our list of tracked blocks.  However,
    // the lookup in our hash table is expensive and slows us too
    // much.
    CallStackMap::iterator block_it = block_map_->find(id);
    if (block_it != block_map_->end()) {
#if 0  // Don't do this until stack->ToString() uses ONLY our heap.
      active_thread_id_ = GetCurrentThreadId();
      PrivateAllocatorString output;
      block_it->second->ToString(&output);
     // VLOG(1) << "First Stack size " << stack->size() << "was\n" << output;
      stack->ToString(&output);
     // VLOG(1) << "Second Stack size " << stack->size() << "was\n" << output;
#endif  // 0

      // TODO(jar): We should delete one stack, and keep the other, perhaps
      // based on size.
      // For now, just delete the first, and keep the second?
      delete block_it->second;
    }
    // TODO(jar): Perchance we should use atomic access to member.
    active_thread_id_ = 0;  // Note: Only do this AFTER exiting above scope!

    (*block_map_)[id] = stack;
  }

  mem_in_use.Add(size);
  mem_in_use_blocks.Increment();
  mem_in_use_allocs.Increment();
}

void MemoryWatcher::OnUntrack(HANDLE heap, int32 id, int32 size) {
  DCHECK_GE(size, 0);

  // Don't bother with these.
  if (size == 0)
    return;

  if (LockedRecursionDetected())
    return;

  {
    base::AutoLock lock(block_map_lock_);
    active_thread_id_ = GetCurrentThreadId();

    // First, find the block in our block_map.
    CallStackMap::iterator it = block_map_->find(id);
    if (it != block_map_->end()) {
      AllocationStack* stack = it->second;
      DCHECK(stack->size() == size);
      block_map_->erase(id);
      delete stack;
    } else {
      // Untracked item.  This happens a fair amount, and it is
      // normal.  A lot of time elapses during process startup
      // before the allocation routines are hooked.
      size = 0;  // Ignore size in tallies.
    }
    // TODO(jar): Perchance we should use atomic access to member.
    active_thread_id_ = 0;
  }

  mem_in_use.Add(-size);
  mem_in_use_blocks.Decrement();
  mem_in_use_frees.Increment();
}

void MemoryWatcher::SetLogName(char* log_name) {
  if (!log_name)
    return;

  log_name_ = log_name;
}

// Help sort lists of stacks based on allocation cost.
// Note: Sort based on allocation count is interesting too!
static bool CompareCallStackIdItems(MemoryWatcher::StackTrack* left,
                                    MemoryWatcher::StackTrack* right) {
  return left->size > right->size;
}


void MemoryWatcher::DumpLeaks() {
  // We can only dump the leaks once.  We'll cleanup the hooks here.
  if (!hooked_)
    return;
  Unhook();

  base::AutoLock lock(block_map_lock_);
  active_thread_id_ = GetCurrentThreadId();

  OpenLogFile();

  // Aggregate contributions from each allocated block on per-stack basis.
  CallStackIdMap stack_map;
  for (CallStackMap::iterator block_it = block_map_->begin();
      block_it != block_map_->end(); ++block_it) {
    AllocationStack* stack = block_it->second;
    int32 stack_hash = stack->hash();
    int32 alloc_block_size = stack->size();
    CallStackIdMap::iterator it = stack_map.find(stack_hash);
    if (it == stack_map.end()) {
      StackTrack tracker;
      tracker.count = 1;
      tracker.size = alloc_block_size;
      tracker.stack = stack;  // Temporary pointer into block_map_.
      stack_map[stack_hash] = tracker;
    } else {
      it->second.count++;
      it->second.size += alloc_block_size;
    }
  }
  // Don't release lock yet, as block_map_ is still pointed into.

  // Put references to StrackTracks into array for sorting.
  std::vector<StackTrack*, PrivateHookAllocator<int32> >
      stack_tracks(stack_map.size());
  CallStackIdMap::iterator it = stack_map.begin();
  for (size_t i = 0; i < stack_tracks.size(); ++i) {
    stack_tracks[i] = &(it->second);
    ++it;
  }
  sort(stack_tracks.begin(), stack_tracks.end(), CompareCallStackIdItems);

  int32 total_bytes = 0;
  int32 total_blocks = 0;
  for (size_t i = 0; i < stack_tracks.size(); ++i) {
    StackTrack* stack_track = stack_tracks[i];
    fwprintf(file_, L"%d bytes, %d allocs, #%d\n",
             stack_track->size, stack_track->count, i);
    total_bytes += stack_track->size;
    total_blocks += stack_track->count;

    CallStack* stack = stack_track->stack;
    PrivateAllocatorString output;
    stack->ToString(&output);
    fprintf(file_, "%s", output.c_str());
  }
  fprintf(file_, "Total Leaks:  %d\n", total_blocks);
  fprintf(file_, "Total Stacks: %d\n", stack_tracks.size());
  fprintf(file_, "Total Bytes:  %d\n", total_bytes);
  CloseLogFile();
}
