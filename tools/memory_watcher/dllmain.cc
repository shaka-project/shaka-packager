// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The memory_watcher.dll is hooked by simply linking it.  When we get the
// windows notification that this DLL is loaded, we do a few things:
//    1) Register a Hot Key.
//       Only one process can hook the Hot Key, so one will get it, and the
//       others will silently fail.
//    2) Create a thread to wait on an event.
//       Since only one process will get the HotKey, it will be responsible for
//       notifying all process when it's time to do something.  Each process
//       will have a thread waiting for communication from the master to dump
//       the callstacks.

#include <windows.h>

#include "base/at_exit.h"
#include "tools/memory_watcher/memory_watcher.h"
#include "tools/memory_watcher/hotkey.h"

class MemoryWatcherDumpKey;  // Defined below.

static wchar_t* kDumpEvent = L"MemWatcher.DumpEvent";
static base::AtExitManager* g_memory_watcher_exit_manager = NULL;
static MemoryWatcher* g_memory_watcher = NULL;
static MemoryWatcherDumpKey* g_hotkey_handler = NULL;
static HANDLE g_dump_event = INVALID_HANDLE_VALUE;
static HANDLE g_quit_event = INVALID_HANDLE_VALUE;
static HANDLE g_watcher_thread = INVALID_HANDLE_VALUE;

// A HotKey to dump the memory statistics.
class MemoryWatcherDumpKey : public HotKeyHandler {
 public:
  MemoryWatcherDumpKey(UINT modifiers, UINT vkey)
    : HotKeyHandler(modifiers, vkey) {}

  virtual LRESULT OnHotKey(UINT, WPARAM, LPARAM, BOOL& bHandled) {
    SetEvent(g_dump_event);
    return 1;
  }
};

// Creates the global memory watcher.
void CreateMemoryWatcher() {
  g_memory_watcher_exit_manager = new base::AtExitManager();
  g_memory_watcher = new MemoryWatcher();
  // Register ALT-CONTROL-D to Dump Memory stats.
  g_hotkey_handler = new MemoryWatcherDumpKey(MOD_ALT|MOD_CONTROL, 0x44);
}

// Deletes the global memory watcher.
void DeleteMemoryWatcher() {
  if (g_hotkey_handler)
    delete g_hotkey_handler;
  g_hotkey_handler = NULL;
  if (g_memory_watcher)
    delete g_memory_watcher;
  g_memory_watcher = NULL;

  // Intentionly leak g_memory_watcher_exit_manager.
}

// Thread for watching for key events.
DWORD WINAPI ThreadMain(LPVOID) {
  bool stopping = false;
  HANDLE events[2] =  { g_dump_event, g_quit_event };
  while (!stopping) {
    DWORD rv = WaitForMultipleObjects(2, events, FALSE, INFINITE);
    switch (rv) {
      case WAIT_OBJECT_0:
        if (g_memory_watcher) {
          g_memory_watcher->DumpLeaks();
        }
        stopping = true;
        break;
      case WAIT_OBJECT_0 + 1:
        stopping = true;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return 0;
}

// Creates the background thread
void CreateBackgroundThread() {
  // Create a named event which can be used to notify
  // all watched processes.
  g_dump_event = CreateEvent(0, TRUE, FALSE, kDumpEvent);
  DCHECK(g_dump_event != NULL);

  // Create a local event which can be used to kill our
  // background thread.
  g_quit_event = CreateEvent(0, TRUE, FALSE, NULL);
  DCHECK(g_quit_event != NULL);

  // Create the background thread.
  g_watcher_thread = CreateThread(0,
                                0,
                                ThreadMain,
                                0,
                                0,
                                0);
  DCHECK(g_watcher_thread != NULL);
}

// Tell the background thread to stop.
void StopBackgroundThread() {
  // Send notification to our background thread.
  SetEvent(g_quit_event);

  // Wait for our background thread to die.
  DWORD rv = WaitForSingleObject(g_watcher_thread, INFINITE);
  DCHECK(rv == WAIT_OBJECT_0);

  // Cleanup our global handles.
  CloseHandle(g_quit_event);
  CloseHandle(g_dump_event);
  CloseHandle(g_watcher_thread);
}

bool IsChromeExe() {
  return GetModuleHandleA("chrome.exe") != NULL;
}

extern "C" {
// DllMain is the windows entry point to this DLL.
// We use the entry point as the mechanism for starting and stopping
// the MemoryWatcher.
BOOL WINAPI DllMain(HINSTANCE dll_instance, DWORD reason,
                              LPVOID reserved) {
  if (!IsChromeExe())
    return FALSE;

  switch (reason) {
    case DLL_PROCESS_ATTACH:
      CreateMemoryWatcher();
      CreateBackgroundThread();
      break;
    case DLL_PROCESS_DETACH:
      DeleteMemoryWatcher();
      StopBackgroundThread();
      break;
  }
  return TRUE;
}

__declspec(dllexport) void __cdecl SetLogName(char* name) {
  g_memory_watcher->SetLogName(name);
}

}  // extern "C"
